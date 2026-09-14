/*
 * CYD_AnimatedPixelClock - Main Entry Point
 *
 * ESP32 Cheap Yellow Display (ILI9341 320x240 or ST7796S 480x320) running an
 * animated retro-arcade clock.
 *
 * Ported from AnimatedPixelClock by Keralots, which targeted an ESP32-S3 and
 * two chained 64x64 HUB75 panels. The PC-metrics, audio-visualizer and ambient
 * screensaver modes of the original are not built here; see archive/README.md.
 */

// ========== User Configuration ==========
// Edit include/config.h to configure WiFi and device options
#include "config.h"

#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config/globals.h"
#include "debug.h"
#include "display/cyd_display.h"
#include "timezones.h"
#include "utils/utils.h"

// ========== External Objects ==========
extern WebServer server;         // Defined in web.cpp
extern Preferences preferences;  // Defined in settings.cpp

// ========== Display Objects ==========
// The panel, and the RGB565 canvas the clock styles actually draw into. The
// canvas presents the upstream HUB75 shim's API (clearDisplay / display), so
// every clock style compiles against it unchanged.
TFT_eSPI tft = TFT_eSPI();
CydDisplay display;

// ========== Global State ==========
uint8_t debugLevel = DEBUG_LEVEL;  // declared extern in debug.h
Settings settings;
bool displayAvailable = false;
bool ntpSynced = false;
unsigned long lastNtpSyncTime = 0;
unsigned long lastReceived = 0;
unsigned long wifiDisconnectTime = 0;
unsigned long nextDisplayUpdate = 0;
bool wifiConnected = false;   // WiFi connection status for icon display
bool httpForceClock = false;  // HTTP override to force clock mode (via /api/mode/clock)

// ========== Module Includes ==========
#include "clocks/clock_globals.h"
#include "clocks/clocks.h"
#include "display/display.h"
#include "network/network.h"
#include "notify/notify.h"
#include "sensors/ldr.h"
#include "status_led/rgb_led.h"
#include "touch/touch.h"
#include "weather/weather.h"
#include "web/web.h"

// ========== Helper Functions ==========

// One non-blocking read of the clock. getLocalTime(info, 0) cannot do this: it
// stamps millis() then loops while elapsed <= budget, so a tick landing in
// between skips every attempt and reports failure with the time available.
bool peekLocalTime(struct tm *info) {
  time_t now;
  time(&now);
  localtime_r(&now, info);
  return info->tm_year > 120;
}

// Helper function to get time with short timeout
bool getTimeWithTimeout(struct tm *timeinfo, unsigned long timeout_ms) {
  if (!ntpSynced) {
    if (getLocalTime(timeinfo, timeout_ms)) {
      // Verify time is reasonable (year > 2020) before accepting
      if (timeinfo->tm_year > 120) {  // tm_year is years since 1900
        ntpSynced = true;
        lastNtpSyncTime = millis();
        DBG_INFO("NTP successfully synchronized");
        return true;
      }
    }
    return false;
  }
  return getLocalTime(timeinfo, timeout_ms);
}

// Returns optimal refresh rate in Hz based on current display mode.
//
// Always adaptive. The manual fixed-Hz override (and its web control) was
// removed upstream - a user-pinned low rate only made animations choppy.
int getOptimalRefreshRate() {
  // A notification banner may scroll over any screen - keep it silky.
  if (notifyActive()) {
    return 60;
  }

  // Boost during active motion for smooth animation.
  if (isAnimationActive()) {
    return 60;
  }

  switch (settings.clockStyle) {
    case 1:  // Standard
    case 2:  // Large
      // Static clocks update once a second; 2 Hz is plenty.
      return 2;
    default:
      // Animated clocks: 20 Hz keeps character movement smooth.
      return 20;
  }
}

// Style IDs the user can reach, in the order a tap walks through them.
// 4 is a legacy alias for Space Invaders; 13 is retired.
static const uint8_t SELECTABLE_STYLES[] = {0, 1, 2, 3, 5, 6, 7, 8,
                                            9, 10, 11, 12, 14, 15, 16, 17};
static constexpr size_t SELECTABLE_STYLE_COUNT =
    sizeof(SELECTABLE_STYLES) / sizeof(SELECTABLE_STYLES[0]);

// Advance to the next clock style and persist it. Weather is skipped until it
// has been configured, matching how Cycle All treats it.
static void advanceClockStyle() {
  size_t current = 0;
  for (size_t i = 0; i < SELECTABLE_STYLE_COUNT; i++) {
    if (SELECTABLE_STYLES[i] == settings.clockStyle) {
      current = i;
      break;
    }
  }

  for (size_t n = 0; n < SELECTABLE_STYLE_COUNT; n++) {
    current = (current + 1) % SELECTABLE_STYLE_COUNT;
    const uint8_t candidate = SELECTABLE_STYLES[current];
    if (candidate == 14 && !weatherConfigured()) {
      continue;  // Weather clock has no location yet
    }
    applyClockStyle(candidate, "touch");
    break;
  }
  saveSettings();
}

// Rotation uses elapsed time, independent of wall-clock adjustments.
#include "clocks/cycle_config.h"
void cycleClockScreens() {
  static char previous[128] = "";
  static CycleEntry entries[CYCLE_COUNT];
  static unsigned index = 0;
  static uint32_t started = 0, lastRendered = 0;
  uint32_t now = millis();
  if (strcmp(previous, settings.cycleConfig) || now - lastRendered > 2000) {
    if (!parseCycleConfig(settings.cycleConfig, entries)) parseCycleConfig(CYCLE_DEFAULT, entries);
    strcpy(previous, settings.cycleConfig);
    index = 0; started = now;
  }
  lastRendered = now;
  if (entries[index].seconds && now - started >= entries[index].seconds * 1000UL) {
    index = (index + 1) % CYCLE_COUNT; started = now;
  }
  for (unsigned n = 0; n < CYCLE_COUNT; ++n) {
    if (entries[index].seconds && (entries[index].style != 14 || weatherConfigured())) break;
    index = (index + 1) % CYCLE_COUNT; started = now;
  }
  static int lastStyle = -1;
  if (lastStyle != entries[index].style) {
    // Rotation changes the rendered style without touching settings.clockStyle,
    // which stays 9 (Cycle All) - so log it directly rather than via
    // applyClockStyle(), which would overwrite the user's chosen style.
    DBG_INFO("Cycle All: %s (%d) -> %s (%d) for %us",
             clockStyleName((uint8_t)lastStyle), lastStyle,
             clockStyleName((uint8_t)entries[index].style), entries[index].style,
             entries[index].seconds);
    resetClockAnimationState();
    lastStyle = entries[index].style;
  }
  switch (entries[index].style) {
    case 0: displayClockWithMario(); break;
    case 1: displayStandardClock(); break;
    case 2: displayLargeClock(); break;
    case 3: displayClockWithSpaceInvader(); break;
    case 5: displayClockWithPong(); break;
    case 6: displayClockWithPacman(); break;
    case 7: displayClockWithSnake(); break;
    case 8: displayClockWithTetris(); break;
    case 10: displayClockWithAsteroids(); break;
    case 11: displayClockWithDino(); break;
    case 12: displayClockWithMatrixRain(); break;
    case 14: displayClockWithWeather(); break;
    case 15: displayClockWithBomberman(); break;
    case 16: displayClockWithTron(); break;
    case 17: displayClockWithDoom(); break;
  }
}

// Render whichever clock style is selected.
static void renderActiveClock() {
  switch (settings.clockStyle) {
    case 0:  displayClockWithMario(); break;
    case 1:  displayStandardClock(); break;
    case 2:  displayLargeClock(); break;
    case 3:
    case 4:  displayClockWithSpaceInvader(); break;
    case 5:  displayClockWithPong(); break;
    case 6:  displayClockWithPacman(); break;
    case 7:  displayClockWithSnake(); break;
    case 8:  displayClockWithTetris(); break;
    case 9:  cycleClockScreens(); break;
    case 10: displayClockWithAsteroids(); break;
    case 11: displayClockWithDino(); break;
    case 12: displayClockWithMatrixRain(); break;
    case 14: displayClockWithWeather(); break;
    case 15: displayClockWithBomberman(); break;
    case 16: displayClockWithTron(); break;
    case 17: displayClockWithDoom(); break;
    default: displayStandardClock(); break;
  }
}


// Once-a-minute status line. Cheap, and it answers the questions that otherwise
// need a rebuild to investigate: is the heap trending down, is WiFi weak, which
// style is actually running, and is the display's row-change detection earning
// its keep. getMinFreeHeap() is the low-water mark since boot, so a slow leak
// shows as that number falling even while free heap looks healthy.
static void logStatusHeartbeat() {
  static uint32_t lastBeat = 0;
  const uint32_t now = millis();
  if (lastBeat != 0 && now - lastBeat < 60000) {
    return;
  }
  lastBeat = now;

  const uint32_t up = now / 1000;
  DBG_INFO("Status: up %02u:%02u:%02u | style %s (%u) | heap %u free, %u min | "
           "rows %u/%d | %s",
           (unsigned)(up / 3600), (unsigned)((up / 60) % 60), (unsigned)(up % 60),
           clockStyleName(settings.clockStyle), settings.clockStyle,
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
           display.lastRowsPushed(), SCREEN_HEIGHT,
           ntpSynced ? "NTP ok" : "NTP pending");

  if (WiFi.status() == WL_CONNECTED) {
    DBG_INFO("        WiFi %s  %s  %d dBm", WiFi.SSID().c_str(),
             WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    DBG_WARN("        WiFi disconnected");
  }

  if (settings.ldrAutoBrightness && ldrAvailable()) {
    DBG_INFO("        LDR raw %u -> backlight %u", ldrRaw(), ldrBrightness());
  }
}

// ========== setup() ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  DBG_INFO("=======================================================");
  DBG_INFO("CYD_AnimatedPixelClock %s", FIRMWARE_VERSION);
  DBG_INFO("Board    %s", BOARD_NAME);
  DBG_INFO("Canvas   %dx%d @ x%d -> panel %dx%d (%u bytes)", CANVAS_WIDTH,
           CANVAS_HEIGHT, DISPLAY_SCALE, PANEL_WIDTH, PANEL_HEIGHT,
           (unsigned)CANVAS_BYTES);
  DBG_INFO("Sprites  x%d, character band %dpx, digits %dx%d", SPRITE_SCALE,
           CHAR_BAND, DIGIT_W, DIGIT_H);
#if HAS_RGB_LED
  DBG_INFO("Hardware touch %s, LDR %s, RGB LED R%u/G%u/B%u", TOUCH_BACKEND_NAME,
           HAS_LDR ? "GPIO34" : "none", RGB_LED_R, RGB_LED_G, RGB_LED_B);
#else
  DBG_INFO("Hardware touch %s, LDR %s, RGB LED none", TOUCH_BACKEND_NAME,
           HAS_LDR ? "GPIO34" : "none");
#endif
  DBG_INFO("Debug    level %u (1=err 2=warn 3=info 4=verbose)", debugLevel);
  DBG_INFO("=======================================================");

  // Allocate the canvas before anything else takes heap. It cannot happen in
  // CydDisplay's constructor: globals are built before FreeRTOS adds the
  // startup-stack regions to the heap, and the 4.0" canvas (76.8KB in one
  // block) failed to allocate there.
  display.allocateBuffer();

  // Load settings from flash
  loadSettings();

  // Initialize display
  displayAvailable = initDisplay();

  // Apply saved brightness setting
  if (displayAvailable) {
    applyDisplayBrightness();
  }

  if (!displayAvailable) {
    DBG_WARN("Display not available, continuing without display");
  } else {
    display.clearDisplay();
    display.setTextColor(DISPLAY_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("PIXEL CLOCK");
    display.setCursor(10, 35);
    display.println("INSERT COIN");
    display.display();
  }

  // CYD hardware with no upstream equivalent. Each is a no-op on a board that
  // does not carry the part.
  initTouch();
  initLdr();
  initRgbLed();

  // Check if hardcoded WiFi credentials are provided
  bool useManualWiFi = (strlen(HARDCODED_WIFI_SSID) > 0);

  if (useManualWiFi) {
    DBG_INFO("Using hardcoded WiFi credentials");
    if (!connectManualWiFi(HARDCODED_WIFI_SSID, HARDCODED_WIFI_PASSWORD)) {
      DBG_WARN("Manual WiFi connection failed - falling back to WiFiManager portal");
      useManualWiFi = false;
    }
  }

  if (!useManualWiFi) {
    initNetwork();
  }

  // Keep the radio awake: WiFi modem sleep delays inbound ACKs to the beacon
  // interval, which stalls large web-page transfers for seconds.
  WiFi.setSleep(false);

  // Initialize NTP
  initNTP();

  // Apply the scheduled dim/off level now that the time is (usually) synced, so
  // the panel comes up at the correct night brightness instead of the un-dimmed
  // boot value. The loop's checkScheduledBrightness() covers the case where NTP
  // was not yet ready here.
  if (displayAvailable) {
    refreshDisplayBrightnessNow();
  }

  // Initialize WiFi connection status flag
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  // Configure hardware watchdog timer
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);

  // Setup web server
  setupWebServer();

  // Background weather fetcher (idles cheaply while weather is disabled)
  startWeatherTask();

  // Show IP address for 5 seconds (configurable via web interface)
  if (displayAvailable && settings.showIPAtBoot) {
    displayConnected();
    delay(5000);
  }

  DBG_INFO("Setup complete - free heap %u bytes", (unsigned)ESP.getFreeHeap());
}

// ========== loop() ==========
void loop() {
  // Feed watchdog
  esp_task_wdt_reset();

  // Check and apply scheduled brightness (time-based dimming)
  checkScheduledBrightness();

  logStatusHeartbeat();

  // CYD peripherals
  updateLdr();
  updateRgbLed();
  if (touchTapped()) {
    advanceClockStyle();
  }

  // Handle web server requests
  server.handleClient();

  // Retry NTP sync periodically if not synced
  if (!ntpSynced && millis() - lastNtpSyncTime > 30000) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 100)) {
      if (timeinfo.tm_year > 120) {
        ntpSynced = true;
        lastNtpSyncTime = millis();
        DBG_INFO("NTP sync successful (retry)");
      }
    } else {
      applyTimezone();  // SNTP client might be dead - restart it
      DBG_WARN("NTP retry: restarted SNTP client");
    }
    lastNtpSyncTime = millis();
  }

  // Periodic NTP re-sync even when already synced (safety net). SNTP keeps the
  // system clock valid between refreshes, so refresh the timezone/SNTP client
  // without clearing ntpSynced - dropping it would needlessly re-anchor the
  // clocks every hour and could flash "Syncing time..." for a frame.
  if (ntpSynced && millis() - lastNtpSyncTime > NTP_RESYNC_INTERVAL) {
    applyTimezone();
    lastNtpSyncTime = millis();
    DBG_INFO("Periodic NTP re-sync triggered");
  }

  // Re-anchor the animated clocks when NTP first becomes valid (or after a
  // reconnect resync). At boot the clocks seed displayed_hour/min with a 00:00
  // fallback; if the first sync lands inside a clock's minute-change window the
  // animation advances from 00:00 instead of jumping to the real time, leaving
  // the clock stuck near 00:00 until a reboot or clock-cycle.
  static bool prevNtpSynced = false;
  if (ntpSynced && !prevNtpSynced) {
    resetClockAnimationState();
    struct tm now_tm;
    if (getLocalTime(&now_tm, 10)) {
      syncDisplayedTime(&now_tm);
    }
  }
  prevNtpSynced = ntpSynced;

  // Display update with adaptive refresh rate
  int targetHz = getOptimalRefreshRate();
  unsigned long frameInterval = 1000 / targetHz;

  if (millis() >= nextDisplayUpdate && displayAvailable && !isDisplayForcedOff()) {
    // Schedule from the previous deadline, not from "now": scheduling from now
    // adds the loop latency to every frame, so frames drift off the fixed grid
    // and the animation tick gates beat against them (visible micro-stutter).
    nextDisplayUpdate += frameInterval;
    if (millis() >= nextDisplayUpdate) {
      // Fell behind (stall or refresh-rate change) - resync to now.
      nextDisplayUpdate = millis() + frameInterval;
    }

    display.clearDisplay();
    renderActiveClock();

    // Notification banner draws over whatever screen is active.
    if (notifyActive()) {
      drawNotifyOverlay();
    }

    display.display();
  }

  // WiFi reconnection handling
  handleWiFiReconnection();
}
