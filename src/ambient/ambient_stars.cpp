/*
 * CYD_AnimatedPixelClock - Ambient: Starfield Warp
 *
 * Ported from AnimatedPixelClock by Keralots. Perspective starfield flying
 * toward the viewer. dt-based so the speed is refresh-rate independent.
 *
 * CYD port: the star count and projection spread follow the canvas, so the
 * field is as dense and as wide as upstream's was on 128x64.
 */

#include <math.h>

#include "ambient.h"
#include "../clocks/clock_layout.h"
#include "../display/display.h"

// 96 stars on upstream's 128x64, scaled by area: 225 at 160x120, 450 at 240x160.
#define STAR_COUNT (96 * SCREEN_WIDTH * SCREEN_HEIGHT / (128 * 64))
// Projection spread, as upstream's 44 x 26 px was for 128x64.
#define STAR_SPREAD_X (SCREEN_WIDTH * 44 / 128)
#define STAR_SPREAD_Y (SCREEN_HEIGHT * 26 / 64)

struct Star {
  float x, y, z;  // x,y in [-1,1], z in (0.05, 1]
};

static Star stars[STAR_COUNT];
static bool starsInit = false;
static unsigned long lastStarUpdate = 0;

static void respawnStar(Star& s) {
  s.x = random(-1000, 1001) / 1000.0f;
  s.y = random(-1000, 1001) / 1000.0f;
  s.z = random(400, 1001) / 1000.0f;
}

void ambientStarsFrame() {
  unsigned long now = millis();
  if (!starsInit) {
    for (int i = 0; i < STAR_COUNT; i++) {
      respawnStar(stars[i]);
      stars[i].z = random(50, 1001) / 1000.0f;  // spread initial depth
    }
    lastStarUpdate = now;
    starsInit = true;
  }

  float dt = (now - lastStarUpdate) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  lastStarUpdate = now;

  for (int i = 0; i < STAR_COUNT; i++) {
    Star& s = stars[i];
    s.z -= 0.55f * dt;
    if (s.z <= 0.05f) {
      respawnStar(s);
      continue;
    }
    int px = SCREEN_WIDTH / 2 + (int)(s.x / s.z * STAR_SPREAD_X);
    int py = SCREEN_HEIGHT / 2 + (int)(s.y / s.z * STAR_SPREAD_Y);
    if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) {
      respawnStar(s);
      continue;
    }
    // Closer stars are brighter and bigger. The nearest are drawn a sprite
    // pixel wide, so they keep their weight on the 4.0" as well.
    if (s.z > 0.6f) {
      display.drawPixel(px, py, 0x4208);  // dim gray
    } else if (s.z > 0.3f) {
      display.drawPixel(px, py, 0x8410);  // mid gray
    } else {
      display.fillRect(px, py, SPRITE_SCALE, SPRITE_SCALE, DISPLAY_WHITE);
      display.fillRect(px + SPRITE_SCALE, py, SPRITE_SCALE, SPRITE_SCALE, 0x8410);  // slight streak
    }
  }
}
