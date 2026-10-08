#pragma once

/*
 * CYD_AnimatedPixelClock - Boot health: OTA rollback and crash report
 *
 * Ported from NickoScope's fork of AnimatedPixelClock (commit 8ec3045, by
 * Nikolay Miroshnichenko). Not part of upstream.
 *
 * An image installed over the air is confirmed only once it has run, so the
 * bootloader can put the previous one back if it has not; and the crash report
 * the SDK keeps in flash is read out at boot, kept in NVS and shown in
 * /api/info. See boot_health.cpp.
 */

#include <ArduinoJson.h>

// Call once in setup(), after the canvas is allocated: reads the running
// image's OTA state and the crash report in flash, if there is one.
void healthBegin();

// Call every loop() pass. displayIdle: nothing is expected on the panel (no
// display, or switched off), so drawn frames are not required to confirm.
void healthTick(bool displayIdle);

// Call after each frame is pushed: counts frames towards confirming the image.
void healthNoteFrame();

// Factory reset: erases the stored crash report (NVS "health").
void healthClear();

// For /api/info: "ota" (slot, state, confirmation) and "lastCrash".
void healthInfoJson(JsonObject out);
