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
// Upstream AnimatedPixelClock (Keralots) reached 2.3.0 on HUB75 hardware.
// This CYD port restarts at 1.0.0 with its own history; see CHANGELOG.md.
constexpr const char *FIRMWARE_VERSION = "1.0.0";
constexpr const char *UPSTREAM_VERSION = "2.3.0";

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

// Board identity, reported by the web UI and /api/info.
#ifdef BOARD_CYD_40
#define BOARD_NAME "ESP32 CYD 4.0\" (ST7796S)"
#else
#define BOARD_NAME "ESP32 CYD 2.8\" (ILI9341)"
#endif

// Landscape. TFT_eSPI rotation 1 = USB port on the right for both CYD variants.
constexpr uint8_t TFT_ROTATION = 1;

// Canvas cost in heap: 160x120 = 37.5KB, 240x160 = 75KB. Logged at boot.
constexpr size_t CANVAS_BYTES = (size_t)CANVAS_WIDTH * CANVAS_HEIGHT * 2;

// ====================== CYD hardware - touch (XPT2046) ===================
// The touch controller sits on its own SPI bus, separate from the display.
// Driven by XPT2046_Touchscreen on VSPI; TOUCH_CS is deliberately NOT defined
// as a build flag so TFT_eSPI does not also try to claim the chip.
//
// Resistive XPT2046 is the only controller supported. The capacitive CYD
// variants (the "C" suffix boards, e.g. ESP32-2432S024C) fit a CST820 on I2C
// using some of the same GPIOs, so driving them as SPI would be wrong. Set
// HAS_RESISTIVE_TOUCH=0 in the board env there: the touch module compiles to
// stubs and never touches those pins.
#ifndef HAS_RESISTIVE_TOUCH
#define HAS_RESISTIVE_TOUCH 1
#endif
constexpr uint8_t TOUCH_SPI_CLK = 25;
constexpr uint8_t TOUCH_SPI_MISO = 39;  // input-only pin - correct for MISO
constexpr uint8_t TOUCH_SPI_MOSI = 32;
constexpr uint8_t TOUCH_CS_PIN = 33;
constexpr uint8_t TOUCH_IRQ_PIN = 36;   // input-only pin

// Touch SPI must stay at or below 2.5MHz or reads are unreliable.
constexpr uint32_t TOUCH_SPI_FREQUENCY = 2500000;

// Ignore repeat presses inside this window (ms).
constexpr uint32_t TOUCH_DEBOUNCE_MS = 250;

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
// Only the 2.8" board populates it.
#ifdef BOARD_CYD_28
#define HAS_RGB_LED 1
constexpr uint8_t RGB_LED_R = 17;
constexpr uint8_t RGB_LED_G = 16;
constexpr uint8_t RGB_LED_B = 4;
#else
#define HAS_RGB_LED 0
#endif

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

// Optional hardcoded credentials for modules with a faulty AP mode. Define
// these in include/secrets.h (gitignored) or via -D build flags - never here.
#ifndef HARDCODED_WIFI_SSID
#define HARDCODED_WIFI_SSID ""
#endif
#ifndef HARDCODED_WIFI_PASSWORD
#define HARDCODED_WIFI_PASSWORD ""
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
