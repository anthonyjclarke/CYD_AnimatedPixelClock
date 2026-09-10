# CLAUDE.md — CYD_AnimatedPixelClock

Port of **AnimatedPixelClock** (Keralots, MIT, upstream v2.3.0) from ESP32-S3 +
2× 64×64 HUB75 panels to the ESP32 Cheap Yellow Display. Port version 1.0.0.

## Target hardware

Three envs, all plain `esp32dev`, 4 MB, no PSRAM: `esp32-cyd-24` and
`esp32-cyd-28` (ILI9341 320×240, backlight GPIO 21, SPI 55 MHz) and
`esp32-cyd-40` (ST7796S 480×320, backlight GPIO 27, SPI 27 MHz, BGR order).
Standard CYD SPI display pins; only the backlight GPIO differs. The 2.4″ ships
in resistive ("R") and capacitive ("C") revisions — see `HAS_RESISTIVE_TOUCH`.

## Rendering model — the central architectural decision

Clock styles draw into an **off-screen RGB565 `GFXcanvas16`** of
`CANVAS_WIDTH`×`CANVAS_HEIGHT` logical pixels, never onto the TFT directly.
`CydDisplay::display()` expands each logical pixel into a `DISPLAY_SCALE`
square block and pushes it. Canvas × scale equals the panel exactly:
160×120 @ ×2 on the 2.8″, 240×160 @ ×2 on the 4.0″. No letterboxing on either.

- Animation code addresses **`SCREEN_WIDTH` / `SCREEN_HEIGHT` only** (aliases of
  the canvas dims). Never write a raw panel coordinate in a clock style, and
  never assume 128×64 — that was the upstream HUB75 canvas.
- A new board is a new `[env:]` block, not a code change.
- `CydDisplay` deliberately exposes the *upstream HUB75 shim's* API so ported
  styles need no display edits. `waitForScanCompletion()` is an intentional
  no-op — TFT pushes are synchronous.

## Layout — `src/clocks/clock_layout.h`

Every style positions itself from these canvas-derived metrics, never from
literal coordinates. Two rules split the metrics:

- **Text scales with the canvas.** `DIGIT_TEXT_SIZE` holds the digit row at ~75%
  of the width on both boards (upstream's five digits filled 70% of 128 px).
- **Sprites do not.** Both boards render at ×2, so a logical pixel is the same
  physical size on each; sprite art therefore stays physically identical and the
  larger panel just shows more room. Scaling it means redrawing every sprite.

`CHAR_BAND` is the load-bearing constant: Mario bounces a digit by putting his
head against its underside, so the gap between the digit row and `GROUND_Y` must
stay exactly one sprite tall. The rest of the vertical composition flows from it.

Pac-Man (pellet grid), TRON (seven segments) and Bomberman (bricks) draw their
own digits and derive their own row geometry rather than using `DIGIT_X`.

## Never do these

- **Never push the whole frame unconditionally.** `display()` hashes each canvas
  row (FNV-1a) and pushes only changed rows. A full push is ~22 ms on the 2.8″
  and ~91 ms on the 4.0″ — the latter would cap that board near 11 fps. A shadow
  framebuffer was rejected: 37–75 KB against a ~200 KB free heap, where the 4.0″
  canvas alone is already 75 KB.
- **Never drop `tft.setSwapBytes(true)`** from `CydDisplay::begin()`.
  GFXcanvas16 stores host-order RGB565; TFT_eSPI pushes image arrays
  byte-for-byte by default. Without it yellow renders purple and red renders
  blue, while white and black — being palindromes — look perfectly fine.
- **Never define `TOUCH_CS` as a build flag.** Touch is driven by
  XPT2046_Touchscreen on its own VSPI instance; defining it makes TFT_eSPI claim
  the same chip select and both drivers fight over the bus.
- **Never unpin the platform.** `espressif32@6.12.0` (arduino-esp32 2.0.17) is
  required — upstream targets the 2.x API (`esp_task_wdt_init(timeout, panic)`).
  Unpinned resolves to the pioarduino 3.x fork and the build fails.
- **Never let the canvas rotate.** `GFXcanvas16` has its own `rotation`; it must
  stay 0 or the buffer layout stops being row-major and row hashing breaks.
  Landscape comes from `tft.setRotation(TFT_ROTATION)` on the panel instead.
- **Never narrow Tetris' well row back to `uint32_t`.** A 4 px cell over a
  160 px canvas is 40 columns, 60 on the 4.0″ — both past 32 bits. `TetRow` is
  `uint64_t`, and `TET_FULLROW` and `tet_clear_mask` widened with it.
- **Never restore `.github/FUNDING.yml`** from `archive/` — those sponsorship
  links are the upstream author's.

## Deliberate deviations from the global rules

Each is a considered exception, not an oversight:

- **ArduinoJson v7**, not v6 — upstream's ~4 k lines of web/settings code uses
  the v7 `JsonDocument` API; downgrading means rewriting all of it.
- **POSIX `TZ` + SNTP**, not ezTime — `timezones.cpp` drives the web UI's
  timezone selector, which ezTime would break.
- **Adafruit GFX 5×7 font**, not VLW. The digits are pixel art on a chunky
  canvas; a smooth font defeats the aesthetic. The VLW rule still applies to
  native-resolution text drawn outside the canvas.
- **`debugLevel` is `extern`**, not header-`static` — ~25 translation units, so
  a header `static` would give each its own copy and break `/api/debug`.

## Persistence

Settings live in NVS namespace `pixelclock`; touch calibration in `cydtouch`.
Neither may be hardcoded. The 384 KB `spiffs` partition is unused, retained so a
filesystem can be added later without repartitioning and losing settings.

## Archive

`archive/` holds upstream assets this port does not build — PC-metrics mode, the
visualizer, ambient screensavers and the `.pca` player, plus all HUB75 hardware
and release material. Nothing there is compiled; `archive/README.md` says what
restoring each would take. A pristine upstream copy sits outside the repo at
`PlatformIO/Projects/AnimatedPixelClock`.
