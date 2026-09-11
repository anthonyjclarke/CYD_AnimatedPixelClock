#pragma once

/*
 * CYD_AnimatedPixelClock - Onboard RGB status LED
 *
 * CYD-only addition. Every supported board carries a common-anode RGB LED;
 * the pins are in include/config.h (red differs on the 4.0"). A board without
 * one compiles this to stubs.
 *
 * Common anode means the pins sink current: duty 0 is full brightness and 255
 * is off. The PWM writes below invert for you - pass normal 0-255 values where
 * 255 means "as bright as this channel goes".
 *
 * It reports device state, not clock content: amber while the setup portal is
 * open, a red pulse while WiFi is down, a blue pulse for a notification, and
 * dark when all is well. initRgbLed() flashes red, green, blue once at boot.
 */

#include <Arduino.h>

#include "config.h"

// Attach the PWM channels. Safe on boards with no RGB LED.
void initRgbLed();

// Derive the colour from current device state and drive the LED. Call from
// loop(); rate-limited internally and a no-op when settings.rgbLedEnabled is
// false or the board has no LED.
void updateRgbLed();

// Drive the LED directly, bypassing state derivation. Values are 0-255 with
// 255 = full brightness; inversion for the common anode is handled here.
void rgbLedSet(uint8_t r, uint8_t g, uint8_t b);

// Turn the LED fully off.
void rgbLedOff();
