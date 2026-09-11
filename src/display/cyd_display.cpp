/*
 * CYD_AnimatedPixelClock - CYD TFT display shim
 *
 * See cyd_display.h for the frame model and the reasoning behind row hashing.
 */

#include "cyd_display.h"

#include <esp_heap_caps.h>

#include "debug.h"

namespace {

// One scaled output row, reused every push. PANEL_WIDTH is 320 or 480, so this
// is 640 or 960 bytes of .bss - cheaper than allocating per frame.
uint16_t scaledRow[PANEL_WIDTH];

// FNV-1a over a canvas row. Chosen over CRC32 for being a few instructions per
// byte with no lookup table; collision resistance is far beyond what row change
// detection needs.
inline uint32_t hashRow(const uint16_t *row, int count) {
  uint32_t h = 2166136261u;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(row);
  for (int i = 0; i < count * 2; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

}  // namespace

bool CydDisplay::begin() {
  tft.init();
  tft.setRotation(TFT_ROTATION);

  // GFXcanvas16 stores RGB565 in host (little-endian) order, but TFT_eSPI
  // defaults to _swapBytes = false and pushes an image array to the panel
  // byte-for-byte. The panel wants big-endian, so without this every pushed
  // pixel arrives byte-swapped: yellow (0xFFE0) lands as 0xE0FF and renders
  // purple, red renders blue, green renders red. White and black are
  // palindromes and look correct either way, which is what makes the fault
  // read as "some colours are wrong" rather than "the display is broken".
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);

  // Backlight on PWM so the upstream brightness schedule has something to drive.
  ledcSetup(BACKLIGHT_LEDC_CHANNEL, BACKLIGHT_LEDC_FREQ, BACKLIGHT_LEDC_BITS);
  ledcAttachPin(TFT_BL, BACKLIGHT_LEDC_CHANNEL);
  setBrightness8(backlight);

  // The panel is up and lit, so a failure below is visible. The canvas is
  // normally allocated at the top of setup(); try again in case begin() is ever
  // reached first. Without a canvas nothing the clock draws can reach the panel,
  // so paint it solid red rather than leave it blank - a blank screen and a
  // failed allocation otherwise look identical. (TFT_eSPI's GLCD font is not
  // loaded in this build, so there is no text to put on it.)
  if (getBuffer() == nullptr && !allocateBuffer()) {
    tft.fillScreen(TFT_RED);
    return false;
  }

  // Sanity-check the geometry rather than silently letterboxing or overdrawing.
  if (tft.width() != PANEL_WIDTH || tft.height() != PANEL_HEIGHT) {
    DBG_WARN("Panel is %dx%d but canvas x scale is %dx%d - check the board env",
             tft.width(), tft.height(), PANEL_WIDTH, PANEL_HEIGHT);
  }

  DBG_INFO("Display ready: canvas %dx%d @ %dx -> panel %dx%d (%u bytes heap)",
           CANVAS_WIDTH, CANVAS_HEIGHT, DISPLAY_SCALE, PANEL_WIDTH, PANEL_HEIGHT,
           (unsigned)CANVAS_BYTES);

  forceFullRepaint();
  return true;
}

void CydDisplay::forceFullRepaint() {
  fullRepaint = true;
}

void CydDisplay::display() {
  const uint16_t *canvas = getBuffer();
  if (canvas == nullptr) {
    return;
  }

  rowsPushed = 0;
  tft.startWrite();

  for (int y = 0; y < CANVAS_HEIGHT; y++) {
    const uint16_t *src = canvas + (size_t)y * CANVAS_WIDTH;

    const uint32_t h = hashRow(src, CANVAS_WIDTH);
    if (!fullRepaint && h == rowHash[y]) {
      continue;  // row is unchanged on the panel - nothing to send
    }
    rowHash[y] = h;
    rowsPushed++;

    // Expand this canvas row horizontally once...
    for (int x = 0; x < CANVAS_WIDTH; x++) {
      const uint16_t c = src[x];
      for (int k = 0; k < DISPLAY_SCALE; k++) {
        scaledRow[x * DISPLAY_SCALE + k] = c;
      }
    }
    // ...then repeat it vertically to complete the DISPLAY_SCALE square blocks.
    for (int k = 0; k < DISPLAY_SCALE; k++) {
      tft.pushImage(0, y * DISPLAY_SCALE + k, PANEL_WIDTH, 1, scaledRow);
    }
  }

  tft.endWrite();
  fullRepaint = false;
}

void CydDisplay::setBrightness8(uint8_t brightness) {
  backlight = brightness;
#ifdef TFT_BACKLIGHT_ON
  // TFT_BACKLIGHT_ON is HIGH on both CYD variants, so duty maps directly.
  ledcWrite(BACKLIGHT_LEDC_CHANNEL, brightness);
#else
  ledcWrite(BACKLIGHT_LEDC_CHANNEL, 255 - brightness);
#endif
  DBG_VERBOSE("Backlight -> %u", brightness);
}

// ---- Sprite magnification ---------------------------------------------------
// See cyd_display.h. The transform expands coordinates about an anchor and
// turns each source pixel into a scale x scale block, so fixed pixel art draws
// larger without any change to the art or to its hundreds of call sites.
//
// Every override below calls the GFXcanvas16 primitive explicitly rather than
// its own class's, which is what stops the transform recursing.

void CydDisplay::setSpriteScale(uint8_t scale, int16_t anchorX, int16_t anchorY) {
  spriteScale = scale < 1 ? 1 : scale;
  spriteAnchorX = anchorX;
  spriteAnchorY = anchorY;
}

void CydDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (spriteScale <= 1) {
    GFXcanvas16::drawPixel(x, y, color);
    return;
  }
  const int16_t tx = spriteAnchorX + (x - spriteAnchorX) * spriteScale;
  const int16_t ty = spriteAnchorY + (y - spriteAnchorY) * spriteScale;
  for (int16_t dy = 0; dy < spriteScale; dy++) {
    GFXcanvas16::drawFastHLine(tx, ty + dy, spriteScale, color);
  }
}

void CydDisplay::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (spriteScale <= 1) {
    GFXcanvas16::drawFastHLine(x, y, w, color);
    return;
  }
  const int16_t tx = spriteAnchorX + (x - spriteAnchorX) * spriteScale;
  const int16_t ty = spriteAnchorY + (y - spriteAnchorY) * spriteScale;
  for (int16_t dy = 0; dy < spriteScale; dy++) {
    GFXcanvas16::drawFastHLine(tx, ty + dy, w * spriteScale, color);
  }
}

void CydDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (spriteScale <= 1) {
    GFXcanvas16::drawFastVLine(x, y, h, color);
    return;
  }
  const int16_t tx = spriteAnchorX + (x - spriteAnchorX) * spriteScale;
  const int16_t ty = spriteAnchorY + (y - spriteAnchorY) * spriteScale;
  for (int16_t dx = 0; dx < spriteScale; dx++) {
    GFXcanvas16::drawFastVLine(tx + dx, ty, h * spriteScale, color);
  }
}

// ---- Canvas allocation ------------------------------------------------------
// See the constructor in cyd_display.h for why this is not done there. The
// largest free block is what decides success - total free heap can be well
// above CANVAS_BYTES while no single block is big enough - so it is logged on
// success as well as failure, to show how much headroom a board actually has.

bool CydDisplay::allocateBuffer() {
  if (buffer != nullptr) {
    return true;
  }

  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const size_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  buffer = static_cast<uint16_t *>(heap_caps_malloc(CANVAS_BYTES, MALLOC_CAP_8BIT));
  if (buffer == nullptr) {
    DBG_ERROR("Canvas allocation failed: need %u bytes in one block, largest free "
              "block is %u (of %u free)", (unsigned)CANVAS_BYTES,
              (unsigned)largest, (unsigned)freeBytes);
    return false;
  }

  // Let GFXcanvas16's destructor free it, as it would a buffer it allocated.
  buffer_owned = true;
  memset(buffer, 0, CANVAS_BYTES);
  DBG_INFO("Canvas allocated: %u bytes (largest free block was %u of %u free)",
           (unsigned)CANVAS_BYTES, (unsigned)largest, (unsigned)freeBytes);
  forceFullRepaint();
  return true;
}
