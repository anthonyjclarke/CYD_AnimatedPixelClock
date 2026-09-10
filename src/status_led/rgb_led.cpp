/*
 * CYD_AnimatedPixelClock - Onboard RGB status LED
 *
 * See rgb_led.h. Everything here is compiled out when the board has no LED.
 */

#include "rgb_led.h"

#include <WiFi.h>

#include "../config/globals.h"
#include "../notify/notify.h"
#include "debug.h"

#if HAS_RGB_LED

namespace {

// LEDC channel 0 belongs to the backlight (see include/config.h), so the three
// colour channels start at 1.
constexpr uint8_t CH_R = 1;
constexpr uint8_t CH_G = 2;
constexpr uint8_t CH_B = 3;
constexpr uint32_t LED_FREQ = 5000;
constexpr uint8_t LED_BITS = 8;

// The status LED sits next to the panel and is distractingly bright at full
// duty. Everything is scaled by this before being written.
constexpr uint8_t STATUS_LEVEL = 40;

constexpr uint32_t UPDATE_INTERVAL_MS = 250;
uint32_t lastUpdateMs = 0;
bool started = false;

// Last colour written, so an unchanged state costs no PWM writes.
uint8_t lastR = 0, lastG = 0, lastB = 0;
bool lastValid = false;

}  // namespace

void initRgbLed() {
  ledcSetup(CH_R, LED_FREQ, LED_BITS);
  ledcSetup(CH_G, LED_FREQ, LED_BITS);
  ledcSetup(CH_B, LED_FREQ, LED_BITS);
  ledcAttachPin(RGB_LED_R, CH_R);
  ledcAttachPin(RGB_LED_G, CH_G);
  ledcAttachPin(RGB_LED_B, CH_B);
  started = true;
  rgbLedOff();
  DBG_INFO("RGB status LED ready on GPIO %u/%u/%u", RGB_LED_R, RGB_LED_G, RGB_LED_B);
}

void rgbLedSet(uint8_t r, uint8_t g, uint8_t b) {
  if (!started) {
    return;
  }
  if (lastValid && r == lastR && g == lastG && b == lastB) {
    return;
  }
  lastR = r; lastG = g; lastB = b; lastValid = true;

  // Common anode: invert, so 255 in means duty 0 out (full brightness).
  ledcWrite(CH_R, 255 - r);
  ledcWrite(CH_G, 255 - g);
  ledcWrite(CH_B, 255 - b);
}

void rgbLedOff() {
  rgbLedSet(0, 0, 0);
}

void updateRgbLed() {
  if (!started) {
    return;
  }
  if (!settings.rgbLedEnabled) {
    rgbLedOff();
    return;
  }

  const uint32_t now = millis();
  if (now - lastUpdateMs < UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateMs = now;

  // A notification outranks connection state - it is the thing the user is
  // meant to notice. Pulsed so it reads as an alert rather than a power light.
  if (notifyActive()) {
    const bool on = ((now / 400) % 2) == 0;
    rgbLedSet(0, 0, on ? STATUS_LEVEL : 0);
    return;
  }

  switch (WiFi.getMode()) {
    case WIFI_AP:
    case WIFI_AP_STA:
      // Waiting in the setup portal - amber, steady.
      rgbLedSet(STATUS_LEVEL, STATUS_LEVEL / 2, 0);
      return;
    default:
      break;
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Disconnected or reconnecting - slow red pulse.
    const bool on = ((now / 800) % 2) == 0;
    rgbLedSet(on ? STATUS_LEVEL : 0, 0, 0);
    return;
  }

  // Connected and idle. The clock itself is the status indicator; leave the
  // LED dark so it does not glow next to the panel all night.
  rgbLedOff();
}

#else  // !HAS_RGB_LED

void initRgbLed() {}
void updateRgbLed() {}
void rgbLedSet(uint8_t, uint8_t, uint8_t) {}
void rgbLedOff() {}

#endif  // HAS_RGB_LED
