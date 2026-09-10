/*
 * CYD_AnimatedPixelClock - XPT2046 touchscreen
 *
 * See touch.h for why this does not go through TFT_eSPI.
 */

#include "touch.h"

#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "../config/globals.h"

#include "debug.h"

#if HAS_RESISTIVE_TOUCH

namespace {

// Dedicated bus. The display is on HSPI - which is only true because
// USE_HSPI_PORT is set in platformio.ini; TFT_eSPI defaults to VSPI, and
// sharing it with this driver makes touch fail intermittently. The XPT2046 must
// also stay at or below 2.5MHz or its reads are unreliable.
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// Calibration bounds, loaded from NVS. Defaults span the typical raw range so
// the panel is usable before anyone calibrates.
uint16_t calXMin = TOUCH_RAW_MIN;
uint16_t calXMax = TOUCH_RAW_MAX;
uint16_t calYMin = TOUCH_RAW_MIN;
uint16_t calYMax = TOUCH_RAW_MAX;

bool started = false;
uint32_t lastTapMs = 0;
bool wasPressed = false;

uint16_t lastRawX = 0, lastRawY = 0;
int16_t lastX = 0, lastY = 0;

constexpr const char *NVS_NAMESPACE = "cydtouch";

// Map a raw reading into canvas space, guarding against a degenerate
// calibration (equal bounds would make map() divide by zero).
int16_t mapAxis(uint16_t raw, uint16_t lo, uint16_t hi, int16_t span) {
  if (hi == lo) {
    return 0;
  }
  if (raw < lo) raw = lo;
  if (raw > hi) raw = hi;
  long v = map(raw, lo, hi, 0, span - 1);
  if (v < 0) v = 0;
  if (v > span - 1) v = span - 1;
  return (int16_t)v;
}

}  // namespace

void initTouch() {
  touchSPI.begin(TOUCH_SPI_CLK, TOUCH_SPI_MISO, TOUCH_SPI_MOSI, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  // Rotation 1 matches the display's landscape orientation.
  ts.setRotation(TFT_ROTATION);

  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, true)) {
    calXMin = prefs.getUShort("xMin", TOUCH_RAW_MIN);
    calXMax = prefs.getUShort("xMax", TOUCH_RAW_MAX);
    calYMin = prefs.getUShort("yMin", TOUCH_RAW_MIN);
    calYMax = prefs.getUShort("yMax", TOUCH_RAW_MAX);
    prefs.end();
    DBG_INFO("Touch calibration x[%u..%u] y[%u..%u]", calXMin, calXMax, calYMin, calYMax);
  } else {
    DBG_WARN("Touch calibration namespace unavailable - using defaults");
  }

  started = true;
  DBG_INFO("Touch ready (CS %u, IRQ %u)", TOUCH_CS_PIN, TOUCH_IRQ_PIN);
}

bool touchPressed() {
  if (!started) {
    return false;
  }
  // Check the interrupt line before touching the bus: tirqTouched() is a cheap
  // pin read, while touched() costs a full SPI transaction.
  return ts.tirqTouched() && ts.touched();
}

bool touchTapped() {
  if (!started || !settings.touchEnabled) {
    return false;
  }

  const bool pressed = touchPressed();

  // Fire on the press edge only, so holding a finger down does not repeat.
  if (!pressed) {
    wasPressed = false;
    return false;
  }
  if (wasPressed) {
    return false;
  }
  wasPressed = true;

  const uint32_t now = millis();
  if (now - lastTapMs < TOUCH_DEBOUNCE_MS) {
    return false;
  }
  lastTapMs = now;

  const TS_Point p = ts.getPoint();
  lastRawX = p.x;
  lastRawY = p.y;
  lastX = mapAxis(p.x, calXMin, calXMax, SCREEN_WIDTH);
  lastY = mapAxis(p.y, calYMin, calYMax, SCREEN_HEIGHT);

  DBG_INFO("Touch: tap at canvas(%d,%d) raw(%u,%u) pressure %d", lastX, lastY,
           lastRawX, lastRawY, p.z);
  return true;
}

int16_t touchX() { return lastX; }
int16_t touchY() { return lastY; }
uint16_t touchRawX() { return lastRawX; }
uint16_t touchRawY() { return lastRawY; }

void touchSetCalibration(uint16_t xMin, uint16_t xMax, uint16_t yMin, uint16_t yMax) {
  if (xMin == xMax || yMin == yMax) {
    DBG_WARN("Rejecting degenerate touch calibration");
    return;
  }
  calXMin = xMin; calXMax = xMax;
  calYMin = yMin; calYMax = yMax;

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    DBG_ERROR("Could not open touch calibration namespace for writing");
    return;
  }
  prefs.putUShort("xMin", xMin);
  prefs.putUShort("xMax", xMax);
  prefs.putUShort("yMin", yMin);
  prefs.putUShort("yMax", yMax);
  prefs.end();
  DBG_INFO("Touch calibration saved x[%u..%u] y[%u..%u]", xMin, xMax, yMin, yMax);
}

void touchClearCalibration() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.clear();
    prefs.end();
    calXMin = TOUCH_RAW_MIN; calXMax = TOUCH_RAW_MAX;
    calYMin = TOUCH_RAW_MIN; calYMax = TOUCH_RAW_MAX;
    DBG_INFO("Touch calibration erased");
  }
}

#else  // !HAS_RESISTIVE_TOUCH - capacitive board, or touch not fitted.
// Stubs only. Nothing here claims the touch GPIOs, which on a capacitive CYD
// belong to a CST820 on I2C rather than to an SPI controller.

void initTouch() { DBG_INFO("Touch: no resistive controller on this board"); }
bool touchPressed() { return false; }
bool touchTapped() { return false; }
int16_t touchX() { return 0; }
int16_t touchY() { return 0; }
uint16_t touchRawX() { return 0; }
uint16_t touchRawY() { return 0; }
void touchSetCalibration(uint16_t, uint16_t, uint16_t, uint16_t) {}
void touchClearCalibration() {}

#endif  // HAS_RESISTIVE_TOUCH
