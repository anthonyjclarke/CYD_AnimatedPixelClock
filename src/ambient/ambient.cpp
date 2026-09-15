/*
 * CYD_AnimatedPixelClock - Ambient screensaver dispatcher
 *
 * Schedule check, effect dispatch and the optional corner clock, as upstream,
 * plus the port's tap-to-peek at the clock. The effects live in ambient_*.cpp.
 */

#include "ambient.h"

#include "../clocks/clocks.h"
#include "../config/globals.h"
#include "../display/display.h"
#include "config.h"
#include "debug.h"

extern bool httpForceAmbient;  // defined in main.cpp

namespace {

bool peeking = false;
uint32_t peekStart = 0;
const char *forcedBy = "API";  // what last forced the screensaver on, for the log

bool inScheduleWindow() {
  if (!settings.ambientEnabled) return false;

  struct tm timeinfo;
  // Non-blocking: a timeout here would stall every loop iteration while NTP
  // is still syncing.
  if (!peekLocalTime(&timeinfo)) return false;

  int h = timeinfo.tm_hour;
  int s = settings.ambientStartHour;
  int e = settings.ambientEndHour;
  if (s == e) return false;  // empty window
  // Wraps midnight the same way scheduled dimming does.
  return (s < e) ? (h >= s && h < e) : (h >= s || h < e);
}

// Small HH:MM in the top-right corner, on a black backing so it stays
// readable over bright effects.
void drawAmbientClock() {
  struct tm timeinfo;
  if (!peekLocalTime(&timeinfo)) return;

  int displayHour, displayMin;
  bool isPM;
  formatTimeForDisplay(timeinfo.tm_hour, timeinfo.tm_min, displayHour,
                       displayMin, isPM);
  char timeStr[6];
  sprintf(timeStr, "%02d%c%02d", displayHour, shouldShowColon() ? ':' : ' ',
          displayMin);

  display.fillRect(SCREEN_WIDTH - 34, 0, 34, 10, DISPLAY_BLACK);
  display.setTextSize(1);
  display.setTextColor(DISPLAY_WHITE);
  display.setCursor(SCREEN_WIDTH - 31, 1);
  display.print(timeStr);
}

}  // namespace

const char *ambientStyleName(uint8_t style) {
  switch (style) {
    case AMBIENT_PACMAN: return "Pac-Man maze";
    case AMBIENT_STARS: return "Starfield";
    case AMBIENT_AQUARIUM: return "Aquarium";
    default: return "Space Invaders";
  }
}

bool ambientPeeking() {
  if (peeking && millis() - peekStart >= AMBIENT_PEEK_MS) {
    peeking = false;
    DBG_INFO("Screensaver: clock peek over");
  }
  return peeking;
}

void ambientPeekClock() {
  peeking = true;
  peekStart = millis();
  DBG_INFO("Screensaver: showing the clock for %us",
           (unsigned)(AMBIENT_PEEK_MS / 1000));
}

void ambientStart(const char *source) {
  peeking = false;
  forcedBy = source;
  httpForceAmbient = true;
}

void ambientToggleFromTouch() {
  if (!ambientActive()) {
    ambientStart("touch");
    return;
  }
  httpForceAmbient = false;
  // Inside the schedule window the screensaver would come straight back, so step
  // it aside for a while, the way a tap does.
  if (inScheduleWindow()) {
    ambientPeekClock();
  }
}

bool ambientActive() {
  if (ambientPeeking()) return false;
  return httpForceAmbient || inScheduleWindow();
}

void ambientUpdate() {
  static bool wasActive = false;
  const bool active = ambientActive();
  if (active == wasActive) return;
  wasActive = active;
  if (active) {
    DBG_INFO("Screensaver on: %s (%s)", ambientStyleName(settings.ambientStyle),
             httpForceAmbient ? forcedBy : "schedule");
  } else if (!peeking) {
    // A peek logs itself; this is the screensaver genuinely ending.
    DBG_INFO("Screensaver off");
  }
}

void displayAmbient() {
  switch (settings.ambientStyle) {
    case AMBIENT_PACMAN: ambientPacmanChaseFrame(); break;
    case AMBIENT_STARS: ambientStarsFrame(); break;
    case AMBIENT_AQUARIUM: ambientAquariumFrame(); break;
    default: ambientInvadersFrame(); break;
  }
  if (settings.ambientShowClock) {
    drawAmbientClock();
  }
}
