#pragma once

/*
 * CYD_AnimatedPixelClock - User Configuration
 *
 * ============================================================
 *   ALL USER-TUNEABLE CONSTANTS LIVE HERE.
 *   Nothing in src/ should hardcode a pin, threshold or SSID.
 * ============================================================
 *
 * Display geometry, the TFT pin map and the SPI clock are set as build flags
 * per board environment in platformio.ini - not here - so a new CYD variant is
 * a new [env:] block rather than an edit to this file.
 *
 * WiFi credentials and API keys belong in include/secrets.h (gitignored).
 * Copy include/secrets.h.example to get started.
 */

#include <Arduino.h>

// ============================ Version ====================================
// Single source of truth for the firmware version: reported at boot, in the web
// UI, /api/info, the mDNS TXT record and to Improv-Serial.
//
// Releases live on `main`, tagged vX.Y.Z, with the bare version number. On `dev`
// the version carries a -dev suffix, so a development build is never mistaken
// for the release it will become. Drop the suffix only when cutting a release.
//
// Upstream AnimatedPixelClock (Keralots) reached 2.3.0 on HUB75 hardware. This
// CYD port restarts its own history at 1.0.0; see CHANGELOG.md.
constexpr const char *FIRMWARE_VERSION = "1.4.0-dev";
constexpr const char *UPSTREAM_VERSION = "2.3.0";

// ============================ Identity ===================================
// The web UI sidebar and /api/info show both halves: this port's repository and
// the project it is based on. Keep both - MIT requires the upstream notice (see
// LICENSE). Improv-Serial and mDNS hold one name, so they get PROJECT_NAME.
constexpr const char *PROJECT_NAME = "CYD_AnimatedPixelClock";
constexpr const char *PROJECT_REPO_URL = "https://github.com/anthonyjclarke/CYD_AnimatedPixelClock";
constexpr const char *PROJECT_REPO_LABEL = "github.com/anthonyjclarke";
constexpr const char *UPSTREAM_PROJECT = "AnimatedPixelClock";
constexpr const char *UPSTREAM_AUTHOR = "Keralots";
constexpr const char *UPSTREAM_REPO_URL = "https://github.com/Keralots/AnimatedPixelClock";

// ====================== Logical canvas geometry ==========================
// The clock styles draw into an off-screen RGB565 canvas of SCREEN_WIDTH x
// SCREEN_HEIGHT logical pixels. Each logical pixel is painted as a
// DISPLAY_SCALE x DISPLAY_SCALE block when pushed to the TFT, which is what
// preserves the chunky pixel-art look on an LCD.
//
// Set per board in platformio.ini:
//   esp32-cyd-28  160x120 @ 2x -> 320x240
//   esp32-cyd-40  240x160 @ 2x -> 480x320
#ifndef CANVAS_WIDTH
#define CANVAS_WIDTH 160
#endif
#ifndef CANVAS_HEIGHT
#define CANVAS_HEIGHT 120
#endif
#ifndef DISPLAY_SCALE
#define DISPLAY_SCALE 2
#endif

// Animation code addresses the canvas through these two names only.
#define SCREEN_WIDTH CANVAS_WIDTH
#define SCREEN_HEIGHT CANVAS_HEIGHT

// Panel pixels actually painted. Must match the physical panel exactly - a
// mismatch means either letterboxing or drawing off the edge.
constexpr int PANEL_WIDTH = CANVAS_WIDTH * DISPLAY_SCALE;
constexpr int PANEL_HEIGHT = CANVAS_HEIGHT * DISPLAY_SCALE;

// Board identity, reported at boot, by the web UI and by /api/info. The 2.4" env
// also defines BOARD_CYD_28, because it shares that board's LDR and RGB LED;
// BOARD_CYD_24 is what tells the two apart for display purposes.
#if defined(BOARD_CYD_40)
#define BOARD_NAME "ESP32 CYD 4.0\" (ST7796S)"
#define DISPLAY_MODEL "ST7796S 480x320 TFT"
#elif defined(BOARD_CYD_24)
#define BOARD_NAME "ESP32 CYD 2.4\" (ILI9341)"
#define DISPLAY_MODEL "ILI9341 320x240 TFT"
#else
#define BOARD_NAME "ESP32 CYD 2.8\" (ILI9341)"
#define DISPLAY_MODEL "ILI9341 320x240 TFT"
#endif

// Landscape. TFT_eSPI rotation 1 = USB port on the right for both CYD variants.
constexpr uint8_t TFT_ROTATION = 1;

// Canvas cost in heap: 160x120 = 37.5KB, 240x160 = 75KB. Logged at boot.
constexpr size_t CANVAS_BYTES = (size_t)CANVAS_WIDTH * CANVAS_HEIGHT * 2;

// ====================== CYD hardware - touch (XPT2046) ===================
// Every board here uses a resistive XPT2046, wired one of two ways. Whether
// TOUCH_CS is defined in the board env picks which:
//
//   TOUCH_CS undefined - own bus (2.4", 2.8"). The dedicated pins below, driven
//   by XPT2046_Touchscreen on VSPI. Do NOT define TOUCH_CS on these boards: it
//   would make TFT_eSPI drive CS 33 too, and read touch off the display bus,
//   where nothing is connected.
//
//   TOUCH_CS=33 - shared bus (4.0" ESP32-32E). The XPT2046 sits on the display's
//   SPI lines and TFT_eSPI drives it. The dedicated pins below are unused.
//
// Capacitive boards (the "C" suffix, e.g. ESP32-2432S024C) fit a CST820 on I2C
// using some of the same GPIOs. Set HAS_RESISTIVE_TOUCH=0 there: the touch
// module compiles to stubs and never touches those pins.
#ifndef HAS_RESISTIVE_TOUCH
#define HAS_RESISTIVE_TOUCH 1
#endif

#if !HAS_RESISTIVE_TOUCH
#define TOUCH_BACKEND_NAME "none"
#elif defined(TOUCH_CS)
#define TOUCH_BACKEND_NAME "XPT2046 on shared display SPI"
#else
#define TOUCH_BACKEND_NAME "XPT2046 on own VSPI"
#endif

constexpr uint8_t TOUCH_SPI_CLK = 25;
constexpr uint8_t TOUCH_SPI_MISO = 39;  // input-only pin - correct for MISO
constexpr uint8_t TOUCH_SPI_MOSI = 32;
constexpr uint8_t TOUCH_CS_PIN = 33;
constexpr uint8_t TOUCH_IRQ_PIN = 36;   // input-only pin

// Touch SPI must stay at or below 2.5MHz or reads are unreliable.
constexpr uint32_t TOUCH_SPI_FREQUENCY = 2500000;

// Ignore repeat taps inside this window (ms).
constexpr uint32_t TOUCH_DEBOUNCE_MS = 250;

// Holding a finger down this long is a long press (ms), which starts or stops the
// screensaver. Anything shorter is a tap, counted when the finger lifts.
constexpr uint32_t TOUCH_LONG_PRESS_MS = 800;

// A resistive panel drops the odd sample mid-press. The finger only counts as
// lifted once contact has been gone this long (ms), so a hold is not split.
constexpr uint32_t TOUCH_RELEASE_MS = 60;

// Pressure thresholds for the shared-bus backend, which polls pressure instead
// of waiting on an IRQ. Press and release differ so a finger resting near the
// threshold does not retrigger: 350 is TFT_eSPI's own press threshold, 120 the
// release level AuroraDemo_CYD uses on the same 4.0" board.
constexpr uint16_t TOUCH_Z_PRESS = 350;
constexpr uint16_t TOUCH_Z_RELEASE = 120;

// Pressure at or above this is not a finger. An XPT2046 that is not answering
// reads Z1 == Z2, which its pressure maths turns into exactly 4095.
constexpr uint16_t TOUCH_Z_SATURATED = 4000;

// Raw XPT2046 span used when no calibration has been stored yet. Real values
// are captured by the on-screen calibration routine and kept in NVS.
constexpr uint16_t TOUCH_RAW_MIN = 200;
constexpr uint16_t TOUCH_RAW_MAX = 3700;

// ====================== CYD hardware - LDR (auto-brightness) =============
// GPIO 34 is input-only with an ADC - correct for the onboard photoresistor.
// The 4.0" board does not populate an LDR, so auto-brightness is compiled out.
#ifdef BOARD_CYD_28
#define HAS_LDR 1
constexpr uint8_t LDR_PIN = 34;
#else
#define HAS_LDR 0
#endif

// ADC counts (12-bit, 0-4095). The CYD's LDR reads LOW in bright light.
constexpr uint16_t LDR_RAW_BRIGHT = 300;   // full sun -> maximum backlight
constexpr uint16_t LDR_RAW_DARK = 3200;    // dark room -> minimum backlight
constexpr uint8_t LDR_SAMPLES = 8;         // rolling average window
constexpr uint32_t LDR_SAMPLE_INTERVAL_MS = 500;
constexpr uint8_t LDR_MIN_BRIGHTNESS = 12;  // never fully dark while enabled
constexpr uint8_t LDR_MAX_BRIGHTNESS = 255;

// ====================== CYD hardware - RGB status LED ====================
// Onboard common-anode RGB LED: writing 0 is full brightness, 255 is off.
// All three boards fit one. Green is GPIO 16 and blue GPIO 17 on each; red is
// GPIO 4 on the 2.4" and 2.8" (ESP32-2432S024 / -028R) but GPIO 22 on the 4.0"
// ESP32-32E (LCDwiki E32R40T).
#if defined(BOARD_CYD_40)
#define HAS_RGB_LED 1
constexpr uint8_t RGB_LED_R = 22;
constexpr uint8_t RGB_LED_G = 16;
constexpr uint8_t RGB_LED_B = 17;
#elif defined(BOARD_CYD_28)
#define HAS_RGB_LED 1
constexpr uint8_t RGB_LED_R = 4;
constexpr uint8_t RGB_LED_G = 16;
constexpr uint8_t RGB_LED_B = 17;
#else
#define HAS_RGB_LED 0
#endif

// ====================== Ambient screensaver ==============================
// How long a tap on the screensaver shows the clock before the effect resumes
// (ms). Taps while the clock shows change its style and restart the time.
constexpr uint32_t AMBIENT_PEEK_MS = 60000;

// ====================== Backlight (PWM) ==================================
// Replaces the HUB75 panel's setBrightness8(). TFT_BL is a build flag: GPIO 21
// on the 2.8" board, GPIO 27 on the 4.0".
constexpr uint8_t BACKLIGHT_LEDC_CHANNEL = 0;
constexpr uint32_t BACKLIGHT_LEDC_FREQ = 5000;
constexpr uint8_t BACKLIGHT_LEDC_BITS = 8;

// ========================== WiFi Configuration ===========================
// Access Point name for initial setup.
// AP_PASSWORD: leave as "" for an open (passwordless) AP - easiest for users.
#define AP_NAME "PixelClock-Setup"
#define AP_PASSWORD ""

// Optional hardcoded credentials for modules with a faulty AP mode. Put them in
// include/secrets.h (gitignored - copy include/secrets.h.example), or pass
// -DHARDCODED_WIFI_SSID / -DHARDCODED_WIFI_PASSWORD. Never here. secrets.h is
// picked up automatically when it exists.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef HARDCODED_WIFI_SSID
#ifdef SECRET_WIFI_SSID
#define HARDCODED_WIFI_SSID SECRET_WIFI_SSID
#else
#define HARDCODED_WIFI_SSID ""
#endif
#endif
#ifndef HARDCODED_WIFI_PASSWORD
#ifdef SECRET_WIFI_PASS
#define HARDCODED_WIFI_PASSWORD SECRET_WIFI_PASS
#else
#define HARDCODED_WIFI_PASSWORD ""
#endif
#endif

// Restart if WiFi has been lost for this long (ms).
constexpr uint32_t WIFI_RECONNECT_TIMEOUT = 60000;

// ========================= Network Configuration =========================
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// NTP resync interval (1 hour, ms).
constexpr uint32_t NTP_RESYNC_INTERVAL = 3600000;

// ========================= Timing Configuration ==========================
// Maximum time (ms) that animated clocks can override NTP time. After this,
// force a resync even if the animation is still running - prevents drift when
// packets are dropped during long animations.
constexpr uint32_t TIME_OVERRIDE_MAX_MS = 60000;

// ======================== Watchdog Configuration =========================
constexpr uint32_t WATCHDOG_TIMEOUT_SECONDS = 15;

// ====================== QR Code Setup Configuration ======================
// Show a scannable QR code during WiFi AP setup instead of text instructions.
// Well suited to the CYD's larger, higher-density panel.
#define QR_SETUP_ENABLED 1  // 1 = QR code, 0 = text instructions

// ================== Improv-Serial WiFi Setup (USB) =======================
// In-browser WiFi provisioning over the CYD's CP2102 bridge. Active only on
// first boot (no saved WiFi); the WiFiManager AP portal runs in parallel as a
// fallback. Costs nothing if no browser is listening. Set to 0 to reclaim
// flash if the app partition ever gets tight.
#define IMPROV_SETUP_ENABLED 1
constexpr uint32_t IMPROV_SETUP_WINDOW_MS = 180000;  // 3-min listen window
