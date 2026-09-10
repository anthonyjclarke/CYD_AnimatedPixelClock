#pragma once

/*
 * CYD_AnimatedPixelClock - CYD TFT display shim
 *
 * Adafruit-GFX-compatible canvas backed by a TFT_eSPI panel. Replaces the
 * upstream HUB75 shim (archive/upstream-src/matrix_display.h) and presents the
 * identical call surface, so every clock style compiles unchanged:
 *
 *   clearDisplay() / <GFX draw calls> / display()
 *
 * ---- How a frame reaches the panel ----------------------------------------
 * Animation code draws into a CANVAS_WIDTH x CANVAS_HEIGHT RGB565 canvas held
 * in heap (GFXcanvas16). display() expands each logical pixel into a
 * DISPLAY_SCALE x DISPLAY_SCALE block and pushes it to the TFT. The canvas is
 * sized per board so scale x canvas fills the panel exactly - no letterboxing.
 *
 * ---- Why rows are checksummed ---------------------------------------------
 * A full-frame push is expensive on SPI: 320x240x2 bytes at 55MHz is ~22ms
 * (~45fps ceiling), and 480x320x2 at 27MHz is ~91ms - about 11fps, which would
 * make the 4.0" board unusable. Clock animations typically change a small
 * fraction of the screen per frame, so display() hashes each canvas row and
 * pushes only the rows that actually changed.
 *
 * A 32-bit FNV-1a per row costs 4 bytes/row (640 bytes at most) instead of the
 * 37-75KB a full shadow framebuffer would need - which matters, because the
 * 4.0" canvas alone is already 75KB of a ~200KB free heap. The tradeoff is that
 * a hash collision would leave one row stale until it next changes; at ~2^-32
 * per row per frame that is not a practical concern, and it self-corrects.
 */

#include <Adafruit_GFX.h>
#include <TFT_eSPI.h>

#include "config.h"

// The panel itself. Defined in display.cpp; the canvas pushes pixels into it.
extern TFT_eSPI tft;

class CydDisplay : public GFXcanvas16 {
public:
  CydDisplay() : GFXcanvas16(CANVAS_WIDTH, CANVAS_HEIGHT) {}

  // Bring up the panel, set landscape rotation and start the backlight PWM.
  // Returns false if the canvas allocation failed.
  bool begin();

  // Clear the drawing canvas. Does not touch the panel until display().
  void clearDisplay() { fillScreen(0); }

  // Panel-API alias kept so upstream display.cpp compiles unchanged.
  void clearScreen() { clearDisplay(); }

  // Scale the canvas onto the TFT, pushing only rows that changed.
  void display();

  // Force the next display() to repaint every row. Needed after anything that
  // writes the panel behind the canvas's back (boot screens, calibration UI).
  void forceFullRepaint();

  // Backlight level, 0-255. Replaces the HUB75 panel's setBrightness8() so the
  // upstream brightness scheduling in display.cpp works untouched.
  void setBrightness8(uint8_t brightness);

  // The HUB75 driver needed a scan-completion wait before reusing a buffer.
  // A TFT push is synchronous, so this is a no-op - kept so the call site in
  // main.cpp stays identical to upstream.
  void waitForScanCompletion() {}

  // Rows actually pushed by the last display() call. Exposed via /api/status
  // to make the change-detection win measurable on real content.
  uint16_t lastRowsPushed() const { return rowsPushed; }

  // ---- Sprite magnification -------------------------------------------------
  // Draw the sprite art larger without touching the art itself.
  //
  // Character sprites are fixed pixel work - Mario is 8x10 - drawn by hundreds
  // of individual fillRect and drawPixel calls. Multiplying coordinates at
  // every one of those call sites would be a huge, error-prone edit, so the
  // magnification lives here instead: while a scale is set, each drawn pixel
  // becomes a scale x scale block and coordinates are expanded about an anchor.
  //
  // The anchor is normally the sprite's feet, so a character grows upward from
  // the ground rather than sinking through it. Always pair with
  // clearSpriteScale() - anything drawn afterwards would otherwise be magnified
  // too, including the digits.
  void setSpriteScale(uint8_t scale, int16_t anchorX, int16_t anchorY);
  void clearSpriteScale() { spriteScale = 1; }

  // Primitive overrides that apply the magnification. Every Adafruit_GFX shape
  // and the text renderer funnel through these three, so nothing else needs to
  // know the transform exists.
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;

private:
  uint8_t spriteScale = 1;
  int16_t spriteAnchorX = 0;
  int16_t spriteAnchorY = 0;

  // FNV-1a hash of each canvas row as last pushed to the panel.
  uint32_t rowHash[CANVAS_HEIGHT] = {0};
  bool fullRepaint = true;
  uint16_t rowsPushed = 0;
  uint8_t backlight = 255;
};

// Magnify sprite art for the lifetime of the object, restoring 1:1 on the way
// out. Scoped rather than manual so an early return inside a draw function
// cannot leave the transform set and magnify the clock digits with it.
class SpriteScale {
public:
  SpriteScale(CydDisplay &d, uint8_t scale, int16_t anchorX, int16_t anchorY)
      : disp(d) {
    disp.setSpriteScale(scale, anchorX, anchorY);
  }
  ~SpriteScale() { disp.clearSpriteScale(); }

  SpriteScale(const SpriteScale &) = delete;
  SpriteScale &operator=(const SpriteScale &) = delete;

private:
  CydDisplay &disp;
};
