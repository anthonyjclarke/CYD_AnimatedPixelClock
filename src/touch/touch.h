#pragma once

/*
 * CYD_AnimatedPixelClock - XPT2046 touchscreen
 *
 * CYD-only addition; the HUB75 original had no input device at all.
 *
 * The touch controller runs on its own VSPI instance, separate from the
 * display's HSPI bus. TOUCH_CS is deliberately NOT defined as a TFT_eSPI build
 * flag - if it were, TFT_eSPI would drive the same chip select and the two
 * drivers would fight over the bus.
 *
 * Current interaction is a single gesture: tap anywhere to advance to the next
 * enabled clock style. That needs no coordinate accuracy, so the firmware is
 * usable before anyone runs a calibration. Calibration bounds are still read
 * from and written to NVS (never hardcoded) so mapped coordinates are correct
 * for any future on-screen UI.
 */

#include <Arduino.h>

#include "config.h"

// Bring up the touch SPI bus and load calibration from NVS.
void initTouch();

// Poll for a debounced tap. Call from loop(). Returns true once per press.
bool touchTapped();

// True while the panel is being pressed.
bool touchPressed();

// Last tap in canvas coordinates (0..SCREEN_WIDTH-1, 0..SCREEN_HEIGHT-1),
// mapped through the stored calibration. Only meaningful after touchTapped().
int16_t touchX();
int16_t touchY();

// Raw controller reading of the last tap, for calibration tooling.
uint16_t touchRawX();
uint16_t touchRawY();

// Persist calibration bounds to NVS and apply them immediately.
void touchSetCalibration(uint16_t xMin, uint16_t xMax, uint16_t yMin, uint16_t yMax);

// Erase stored calibration, returning to the compiled defaults. Part of a
// factory reset; owns its own NVS namespace name.
void touchClearCalibration();
