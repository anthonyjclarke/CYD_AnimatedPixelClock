/*
 * CYD_AnimatedPixelClock - XPT2046 touchscreen
 *
 * Every CYD this project targets uses an XPT2046 resistive controller, but the
 * boards wire it two ways and the driver has to match the wiring:
 *
 *   Own bus - 2.4" and 2.8". The XPT2046 has dedicated pins (CLK 25, MISO 39,
 *   MOSI 32, CS 33, IRQ 36) and is driven here by XPT2046_Touchscreen on VSPI.
 *   The display sits on HSPI (USE_HSPI_PORT), so the two never share a bus.
 *
 *   Shared bus - 4.0" ESP32-32E. The XPT2046 hangs off the display's own SPI
 *   lines with CS 33. Only TFT_eSPI can drive it safely: it owns that bus and
 *   drops to SPI_TOUCH_FREQUENCY for each touch transaction. The board env
 *   defines TOUCH_CS=33, which is what switches TFT_eSPI's touch support on.
 *
 * Whether TOUCH_CS is defined picks the wiring. Everything above the hardware -
 * edge detection, debounce, the implausible-read guard and calibration storage -
 * is shared, so both behave identically once a sample is in hand.
 */

#include "touch.h"

#include <Preferences.h>

#include "../config/globals.h"
#include "debug.h"

#if !HAS_RESISTIVE_TOUCH

// Capacitive board, or no touch fitted. Stubs only: nothing here claims the
// touch GPIOs, which on a capacitive CYD belong to a CST820 on I2C.
void initTouch() { DBG_INFO("Touch: no resistive controller on this board"); }
bool touchPressed() { return false; }
TouchGesture touchPoll() { return TOUCH_NONE; }
int16_t touchX() { return 0; }
int16_t touchY() { return 0; }
uint16_t touchRawX() { return 0; }
uint16_t touchRawY() { return 0; }
void touchSetCalibration(uint16_t, uint16_t, uint16_t, uint16_t) {}
void touchClearCalibration() {}

#else  // HAS_RESISTIVE_TOUCH

#if defined(TOUCH_CS)
#include <TFT_eSPI.h>
extern TFT_eSPI tft;  // defined in main.cpp; owns the shared bus
#else
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#endif

namespace {

constexpr const char *NVS_NAMESPACE = "cydtouch";

// Calibration bounds, loaded from NVS. Defaults span the typical raw range so
// the panel is usable before anyone calibrates.
uint16_t calXMin = TOUCH_RAW_MIN;
uint16_t calXMax = TOUCH_RAW_MAX;
uint16_t calYMin = TOUCH_RAW_MIN;
uint16_t calYMax = TOUCH_RAW_MAX;

bool started = false;
bool warnedImplausible = false;
uint32_t lastTapMs = 0;

// Gesture state for the press in progress.
bool fingerDown = false;
bool longPressFired = false;
uint32_t pressStartMs = 0;
uint32_t lastContactMs = 0;
uint16_t pressZ = 0;

uint16_t lastRawX = 0, lastRawY = 0;
int16_t lastX = 0, lastY = 0;

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

// A real press cannot produce these readings. Both touch faults seen on
// hardware looked exactly like this: an XPT2046 that is not answering returns
// the same value for Z1 and Z2, which its pressure formula turns into 4095, with
// the coordinates pinned to 0 or the rail. Accepting such a read turned a dead
// touch panel into a phantom tap that changed the clock style and saved it.
bool plausible(uint16_t rawX, uint16_t rawY, uint16_t z) {
  if (z >= TOUCH_Z_SATURATED) return false;
  if (rawX == 0 && rawY == 0) return false;
  if (rawX >= 4095 || rawY >= 4095) return false;
  return true;
}

#if defined(TOUCH_CS)

constexpr const char *BACKEND = "shared display SPI";

// TFT_eSPI brings the bus and the touch chip select up inside tft.init(), so
// there is nothing to start here - which also means touch cannot work if the
// display failed to initialise.
void hwBegin() {}

// Poll pressure with hysteresis: press above TOUCH_Z_PRESS, release only below
// TOUCH_Z_RELEASE, so a finger resting near the threshold does not retrigger.
bool hwSample(uint16_t &x, uint16_t &y, uint16_t &z) {
  static bool down = false;
  z = tft.getTouchRawZ();
  if (down) {
    if (z < TOUCH_Z_RELEASE) down = false;
  } else if (z > TOUCH_Z_PRESS) {
    down = true;
  }
  if (!down) return false;
  tft.getTouchRaw(&x, &y);
  return true;
}

#else  // own bus

constexpr const char *BACKEND = "own VSPI";

// Dedicated bus. The display is on HSPI - only because USE_HSPI_PORT is set in
// platformio.ini; TFT_eSPI defaults to VSPI, and sharing it with this driver
// makes touch fail intermittently. The XPT2046 must also stay at or below
// 2.5MHz or its reads are unreliable.
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

void hwBegin() {
  touchSPI.begin(TOUCH_SPI_CLK, TOUCH_SPI_MISO, TOUCH_SPI_MOSI, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  // Rotation 1 matches the display's landscape orientation.
  ts.setRotation(TFT_ROTATION);
}

// The library only samples after the IRQ line falls, so the interrupt check is
// a cheap pin read in front of a full SPI transaction.
bool hwSample(uint16_t &x, uint16_t &y, uint16_t &z) {
  if (!(ts.tirqTouched() && ts.touched())) return false;
  const TS_Point p = ts.getPoint();
  x = (uint16_t)p.x;
  y = (uint16_t)p.y;
  z = (uint16_t)p.z;
  return true;
}

#endif  // TOUCH_CS

}  // namespace

void initTouch() {
  hwBegin();

  Preferences prefs;
  // A read-only begin() fails when the namespace has never been written, which
  // is simply the state before anyone calibrates - not worth a warning.
  if (prefs.begin(NVS_NAMESPACE, true)) {
    calXMin = prefs.getUShort("xMin", TOUCH_RAW_MIN);
    calXMax = prefs.getUShort("xMax", TOUCH_RAW_MAX);
    calYMin = prefs.getUShort("yMin", TOUCH_RAW_MIN);
    calYMax = prefs.getUShort("yMax", TOUCH_RAW_MAX);
    prefs.end();
    DBG_INFO("Touch calibration x[%u..%u] y[%u..%u]", calXMin, calXMax, calYMin, calYMax);
  } else {
    DBG_INFO("Touch calibration: none stored, using defaults");
  }

  started = true;
  DBG_INFO("Touch ready: XPT2046 on %s (CS %u)", BACKEND, TOUCH_CS_PIN);
}

bool touchPressed() {
  if (!started) {
    return false;
  }
  uint16_t x = 0, y = 0, z = 0;
  return hwSample(x, y, z) && plausible(x, y, z);
}

TouchGesture touchPoll() {
  if (!started || !settings.touchEnabled) {
    fingerDown = false;
    return TOUCH_NONE;
  }

  uint16_t rawX = 0, rawY = 0, z = 0;
  const uint32_t now = millis();
  bool contact = hwSample(rawX, rawY, z);

  if (contact && !plausible(rawX, rawY, z)) {
    // Warn once per boot - a dead controller would otherwise log every poll -
    // then keep the detail at verbose. An impossible read is never a finger.
    if (!warnedImplausible) {
      DBG_WARN("Touch: ignored implausible read raw(%u,%u) pressure %u - is the "
               "controller answering on these pins?", rawX, rawY, z);
      warnedImplausible = true;
    } else {
      DBG_VERBOSE("Touch: ignored implausible read raw(%u,%u) pressure %u", rawX, rawY, z);
    }
    contact = false;
  }

  if (contact) {
    lastContactMs = now;
    if (!fingerDown) {
      // Press edge: remember where it began, for the log and for touchX/Y().
      fingerDown = true;
      longPressFired = false;
      pressStartMs = now;
      pressZ = z;
      lastRawX = rawX;
      lastRawY = rawY;
      lastX = mapAxis(rawX, calXMin, calXMax, SCREEN_WIDTH);
      lastY = mapAxis(rawY, calYMin, calYMax, SCREEN_HEIGHT);
    }
    if (!longPressFired && now - pressStartMs >= TOUCH_LONG_PRESS_MS) {
      longPressFired = true;
      DBG_INFO("Touch: long press at canvas(%d,%d) raw(%u,%u) pressure %u", lastX,
               lastY, lastRawX, lastRawY, pressZ);
      return TOUCH_LONG_PRESS;
    }
    return TOUCH_NONE;
  }

  // No contact. Only a release once contact has been gone for TOUCH_RELEASE_MS,
  // so a sample dropped mid-press does not split one hold into two taps.
  if (!fingerDown || now - lastContactMs < TOUCH_RELEASE_MS) {
    return TOUCH_NONE;
  }
  fingerDown = false;
  if (longPressFired) {
    return TOUCH_NONE;  // the long press was already reported
  }
  if (now - lastTapMs < TOUCH_DEBOUNCE_MS) {
    return TOUCH_NONE;
  }
  lastTapMs = now;
  DBG_INFO("Touch: tap at canvas(%d,%d) raw(%u,%u) pressure %u", lastX, lastY,
           lastRawX, lastRawY, pressZ);
  return TOUCH_TAP;
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

#endif  // HAS_RESISTIVE_TOUCH
