/*
 * CYD_AnimatedPixelClock - Display Module
 *
 * Display initialization, brightness control and the global display object.
 *
 * The object is a CydDisplay: an Adafruit-GFX canvas pushed to a TFT_eSPI
 * panel, presenting the same API the upstream HUB75 shim did. Clock styles
 * draw through it without knowing which hardware is underneath.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include "config.h"

#include "cyd_display.h"
extern CydDisplay display;
#ifndef DISPLAY_WHITE
  #define DISPLAY_WHITE 0xFFFF
#endif
#ifndef DISPLAY_BLACK
  #define DISPLAY_BLACK 0x0000
#endif

// ---- User-editable sprite colors ----
// Returns the configured RGB565 color for a ColorSlot. Macro expands at call
// sites where `settings` (config.h) is in scope. Slot enum + defaults live in
// config/color_slots.h.
#define SPRITE_COLOR(slot) (settings.spriteColors[(slot)])

// Time-digit + colon color for the active clock style (per-style, replaces the
// old single global COL_DIGITS). Defined in clocks/clock_common.cpp.
uint16_t digitColor();

// Initialize display - returns true on success
bool initDisplay();

// Repaint every row on the next frame. Call after anything that writes the
// panel behind the canvas's back (boot screens, touch calibration).
void forceDisplayRepaint();
void applyDisplayBrightness();
void refreshDisplayBrightnessNow();
void checkScheduledBrightness();

// True while the scheduled power-off window is currently active (panel dark).
bool isDisplayScheduledOff();

// Runtime display control (HTTP API) - not persisted to flash
void setDisplayForcedOff(bool off);
bool isDisplayForcedOff();
void setDisplayBrightnessPercent(uint8_t percent);

#endif // DISPLAY_H
