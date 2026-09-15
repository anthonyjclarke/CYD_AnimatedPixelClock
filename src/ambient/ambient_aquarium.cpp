/*
 * CYD_AnimatedPixelClock - Ambient: Aquarium
 *
 * Ported from AnimatedPixelClock by Keralots. Fish cruising at different
 * depths and speeds, rising bubbles, swaying kelp and a sandy floor. All motion
 * is dt-based.
 *
 * CYD port: the fish are magnified by SPRITE_SCALE like the clocks' characters,
 * and the stock follows the tank - more fish on a taller canvas, more bubbles
 * and kelp on a wider one - with depths and kelp spacing proportional rather
 * than upstream's fixed 128x64 coordinates.
 */

#include <math.h>

#include "ambient.h"
#include "../clocks/clock_layout.h"
#include "../display/display.h"

static constexpr int K = SPRITE_SCALE;

#define AQ_FLOOR_Y (SCREEN_HEIGHT - 2 * K)
// Upstream stocked 5 fish, 6 bubbles and 3 kelp stalks for 128x64. Each count
// follows the dimension it fills.
#define AQ_FISH (5 * SCREEN_HEIGHT / 64)     // 9 at 120 rows, 12 at 160
#define AQ_BUBBLES (6 * SCREEN_WIDTH / 128)  // 7 at 160 columns, 11 at 240
#define AQ_KELP (3 * SCREEN_WIDTH / 128)     // 3 at 160 columns, 5 at 240

struct AqFish {
  float x;
  int y;
  float speed;  // px/s, sign = direction
  uint16_t color;
};

struct AqBubble {
  float x, y;
  float speed;  // px/s upward
  float sway;   // phase
};

// Upstream's five fish, cycled to fill a bigger tank.
static const uint16_t FISH_COLORS[5] = {0xFC00, 0x07FF, 0xFFE0, 0xF81F, 0x87F0};
static const float FISH_SPEEDS[5] = {14.0f, -10.0f, 8.0f, -16.0f, 11.0f};
// Kelp heights as a share of upstream's 64 rows, cycled.
static const int KELP_H64[5] = {26, 18, 30, 22, 28};

static AqFish fish[AQ_FISH];
static AqBubble bubbles[AQ_BUBBLES];
static bool aqInit = false;
static unsigned long lastAqUpdate = 0;

static void respawnBubble(AqBubble& b) {
  b.x = random(4 * K, SCREEN_WIDTH - 4 * K);
  b.y = AQ_FLOOR_Y;
  b.speed = random(8, 18) * K;
  b.sway = random(0, 628) / 100.0f;
}

// A small fish: oval body, flapping triangle tail, one eye pixel. Drawn in
// sprite pixels around the fish's centre and magnified from there.
static void drawFish(const AqFish& f, bool tailUp) {
  int x = (int)f.x;
  SpriteScale mag(display, K, x, f.y);
  bool right = f.speed > 0;
  int bodyX = x;
  display.fillCircle(bodyX, f.y, 2, f.color);
  display.fillCircle(bodyX + (right ? 2 : -2), f.y, 1, f.color);
  int tailBase = bodyX + (right ? -3 : 3);
  int tailTip = tailBase + (right ? -3 : 3);
  display.fillTriangle(tailBase, f.y, tailTip, f.y - (tailUp ? 3 : 1),
                       tailTip, f.y + (tailUp ? 1 : 3), f.color);
  display.drawPixel(bodyX + (right ? 3 : -3), f.y - 1, DISPLAY_BLACK);
}

void ambientAquariumFrame() {
  unsigned long now = millis();
  if (!aqInit) {
    // Depths spread evenly between the surface and the kelp tops, as upstream's
    // 12..52 were on 64 rows.
    const int top = 12 * K;
    const int bottom = AQ_FLOOR_Y - 10 * K;
    for (int i = 0; i < AQ_FISH; i++) {
      fish[i].x = random(0, SCREEN_WIDTH);
      fish[i].y = top + i * (bottom - top) / (AQ_FISH - 1);
      fish[i].speed = FISH_SPEEDS[i % 5] * K;
      fish[i].color = FISH_COLORS[i % 5];
    }
    for (int i = 0; i < AQ_BUBBLES; i++) {
      respawnBubble(bubbles[i]);
      bubbles[i].y = random(10 * K, AQ_FLOOR_Y);
    }
    lastAqUpdate = now;
    aqInit = true;
  }

  float dt = (now - lastAqUpdate) / 1000.0f;
  if (dt > 0.1f) dt = 0.1f;
  lastAqUpdate = now;
  float t = now / 1000.0f;

  // Sandy floor with a few darker speckles.
  display.fillRect(0, AQ_FLOOR_Y, SCREEN_WIDTH, K, 0xCDAC);
  display.fillRect(0, AQ_FLOOR_Y + K, SCREEN_WIDTH, K, 0x8B44);
  for (int x = 6 * K; x < SCREEN_WIDTH; x += 17 * K) {
    display.fillRect(x, AQ_FLOOR_Y, K, K, 0x8B44);
  }

  // Kelp: wavy vertical stalks with leaf nubs, swaying in sync with depth.
  // The wave is indexed in upstream rows, so a taller stalk has the same
  // number of bends rather than more, tighter ones.
  const int leafStep = 5 * SCREEN_HEIGHT / 64;
  for (int k = 0; k < AQ_KELP; k++) {
    const int baseX = SCREEN_WIDTH * (2 * k + 1) / (2 * AQ_KELP);
    const int h = KELP_H64[k % 5] * SCREEN_HEIGHT / 64;
    for (int i = 0; i < h; i++) {
      int y = AQ_FLOOR_Y - 1 - i;
      float row64 = i * 64.0f / SCREEN_HEIGHT;
      int x = baseX + (int)(2.5f * K * sinf(t * 1.3f + k * 2.0f + row64 * 0.28f));
      display.fillRect(x, y, K, 1, 0x0560);
      if (i % leafStep == (leafStep * 3) / 5) {
        display.fillRect(x + (((i / leafStep) % 2) ? K : -K), y, K, K, 0x0560);
      }
    }
  }

  // Bubbles rise with a gentle sway and pop at the surface.
  for (int i = 0; i < AQ_BUBBLES; i++) {
    AqBubble& b = bubbles[i];
    b.y -= b.speed * dt;
    if (b.y < 2 * K) {
      respawnBubble(b);
      continue;
    }
    int bx = (int)(b.x + 2.0f * K * sinf(t * 2.0f + b.sway));
    display.fillRect(bx, (int)b.y, K, K, 0x861F);
  }

  // Fish cruise and wrap around off-screen.
  for (int i = 0; i < AQ_FISH; i++) {
    AqFish& f = fish[i];
    f.x += f.speed * dt;
    if (f.speed > 0 && f.x > SCREEN_WIDTH + 8 * K) f.x = -8 * K;
    if (f.speed < 0 && f.x < -8 * K) f.x = SCREEN_WIDTH + 8 * K;
    bool tailUp = ((now / 250) + i) % 2 == 0;
    drawFish(f, tailUp);
  }
}
