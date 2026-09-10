/*
 * AnimatedPixelClock - Settings Module Header
 *
 * Declarations for settings persistence functions.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "../config/globals.h"
#include <Preferences.h>

// Initialize settings (load from NVS or set defaults)
void loadSettings();

// Save current settings to NVS
void saveSettings();

// Erase every stored setting so the next boot picks up the compiled defaults.
// Owns the NVS namespace name, which is why this lives here rather than in the
// web handler - hardcoding it there is how the reset came to clear the wrong
// namespace after the port renamed it.
void factoryResetSettings();

// Brightness helpers
uint8_t sanitizeBrightnessValue(uint8_t value);
bool isZeroBrightnessAllowed();
void sanitizeBrightnessSettings();

extern Preferences preferences;

#endif // SETTINGS_H
