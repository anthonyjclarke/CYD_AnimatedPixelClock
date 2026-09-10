/*
 * CYD_AnimatedPixelClock - CYD TFT display shim
 *
 * See cyd_display.h for the frame model and the reasoning behind row hashing.
 */

#include "cyd_display.h"

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
  // GFXcanvas16 allocates in its constructor at static-init time; a null buffer
  // here means the heap could not satisfy CANVAS_BYTES.
  if (getBuffer() == nullptr) {
    DBG_ERROR("Canvas allocation failed (%u bytes for %dx%d)",
              (unsigned)CANVAS_BYTES, CANVAS_WIDTH, CANVAS_HEIGHT);
    return false;
  }

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
