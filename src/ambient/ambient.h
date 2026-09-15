#pragma once

/*
 * CYD_AnimatedPixelClock - Ambient screensaver
 *
 * Ported from AnimatedPixelClock by Keralots. Full-screen effects shown instead
 * of the clock during a scheduled window, or when forced via /api/mode/ambient.
 * Each effect keeps its own file-local state and renders one frame per call.
 *
 * The port carries upstream's four procedural effects - Space Invaders battle,
 * Pac-Man maze, Starfield and Aquarium - re-laid-out for the CYD canvas. The
 * "This is fine" frames and the uploaded .pca player stay in
 * archive/upstream-src/ambient/. See DEVIATIONS.md.
 */

#include <Arduino.h>

// Effect ids as stored in settings.ambientStyle. They keep upstream's numbers so
// an exported config maps across; 2 (retired upstream), 5 and 6 (not ported)
// have no effect in this build.
enum AmbientStyle : uint8_t {
  AMBIENT_INVADERS = 0,
  AMBIENT_PACMAN = 1,
  AMBIENT_STARS = 3,
  AMBIENT_AQUARIUM = 4,
};

// Map an unported or out-of-range id to Space Invaders, so the web UI select
// always has a matching option and an imported upstream config cannot select
// an effect this build does not have.
inline uint8_t normalizeAmbientStyle(int s) {
  return (s == AMBIENT_PACMAN || s == AMBIENT_STARS || s == AMBIENT_AQUARIUM)
             ? (uint8_t)s
             : (uint8_t)AMBIENT_INVADERS;
}

// Human-readable effect name, for logs.
const char *ambientStyleName(uint8_t style);

// True while the screensaver should replace the clock: forced by the API or
// inside the schedule window, and not paused by a tap.
bool ambientActive();

// A tap on the screensaver shows the clock for AMBIENT_PEEK_MS. While the clock
// shows, taps change the clock style as usual and each restarts that time.
void ambientPeekClock();
bool ambientPeeking();

// Force the screensaver on now, until /api/mode/auto, /api/mode/clock, a long
// press or a reboot. `source` names what asked, for the log ("API", "touch").
void ambientStart(const char *source);

// Long press: start the screensaver if the clock is showing, otherwise leave it
// and return to the clock.
void ambientToggleFromTouch();

// Call every loop pass: logs the screensaver starting and stopping.
void ambientUpdate();

// Render one frame of the selected effect, plus the optional corner clock.
void displayAmbient();

// Per-effect frames (called by displayAmbient's dispatcher).
void ambientInvadersFrame();
void ambientPacmanChaseFrame();
void ambientStarsFrame();
void ambientAquariumFrame();
