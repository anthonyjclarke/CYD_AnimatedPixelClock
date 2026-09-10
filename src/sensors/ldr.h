#pragma once

/*
 * CYD_AnimatedPixelClock - Ambient light sensor (LDR)
 *
 * CYD-only addition; the upstream HUB75 build had no light sensor and relied
 * purely on the scheduled dim / off windows.
 *
 * The 2.8" board populates a photoresistor on GPIO 34 (input-only, ADC1). The
 * 4.0" board does not, so every entry point compiles away to a stub there and
 * ldrAvailable() reports false - callers need no board checks of their own.
 *
 * Readings feed a rolling average before being mapped to a backlight level:
 * a single ADC sample on the ESP32 is noisy enough to make the backlight
 * visibly hunt.
 */

#include <Arduino.h>

#include "config.h"

// Configure the ADC. Safe to call on boards with no LDR.
void initLdr();

// Sample on a millis() interval and fold into the rolling average.
// Call from loop(); returns immediately when it is not yet time to sample.
void updateLdr();

// True when this board has an LDR and at least one sample has been taken.
bool ldrAvailable();

// Latest averaged raw ADC reading (0-4095). 0 when unavailable.
uint16_t ldrRaw();

// Rolling average mapped to a backlight level, clamped to
// [settings.ldrMinBrightness, LDR_MAX_BRIGHTNESS]. The CYD's LDR reads LOW in
// bright light, so the mapping is inverted.
uint8_t ldrBrightness();
