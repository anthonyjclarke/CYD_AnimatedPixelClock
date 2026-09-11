# CLAUDE.md — CYD_AnimatedPixelClock

Port of **AnimatedPixelClock** (Keralots, MIT, upstream v2.3.0) from ESP32-S3 +
2× 64×64 HUB75 panels to the ESP32 Cheap Yellow Display.

`FIRMWARE_VERSION` in `include/config.h` is the only place the version lives.
Releases are on `main`, tagged `vX.Y.Z`; `dev` carries a `-dev` suffix. Never
release from `dev` or leave the suffix on a tagged commit — that version reaches
the serial log, web UI, `/api/info` and mDNS, so a wrong one is wrong five ways.

## Target hardware

Three envs, all plain `esp32dev`, 4 MB, no PSRAM: `esp32-cyd-24` and
`esp32-cyd-28` (ILI9341 320×240, backlight GPIO 21, SPI 55 MHz) and
`esp32-cyd-40` (ESP32-32E, ST7796S 480×320, backlight GPIO 27, SPI 40 MHz).
The display is on HSPI's native pins. Touch is an XPT2046 with its own VSPI pins
on the 2.4″/2.8″, but on the display's SPI lines on the 4.0″ (confirmed on an
ESP32-32E) — see `TOUCH_CS`. Capacitive boards: `HAS_RESISTIVE_TOUCH=0`.
RGB LED is red GPIO 4 / green 16 / blue 17, but red is GPIO 22 on the 4.0″. The
global CYD rule has red and blue reversed — do not copy it.

## Rendering model — the central architectural decision

Clock styles draw into an **off-screen RGB565 `GFXcanvas16`**, never onto the
TFT directly. `CydDisplay::display()` expands each logical pixel into a
`DISPLAY_SCALE` block; canvas × scale equals the panel exactly — 160×120 @ ×2 on
the 2.4″/2.8″, 240×160 @ ×2 on the 4.0″. No letterboxing on any board.

- Animation code addresses **`SCREEN_WIDTH` / `SCREEN_HEIGHT` only**. Never write
  a raw panel coordinate in a clock style, and never assume 128×64.
- A new board is a new `[env:]` block, not a code change.
- `CydDisplay` deliberately exposes the *upstream HUB75 shim's* API so ported
  styles need no display edits. `waitForScanCompletion()` is an intentional no-op.

## Layout — `src/clocks/clock_layout.h`

Every style positions itself from these canvas-derived metrics, never from
literal coordinates. Two rules split the metrics:

- **Text scales with the canvas.** `DIGIT_TEXT_SIZE` holds the digit row at ~75%
  of the width on any board, the proportion upstream had.
- **Sprite art is magnified, never redrawn.** It is fixed pixel work, so
  `SPRITE_SCALE` expands it at draw time — `CydDisplay::setSpriteScale`.

The vertical stack is built bottom-up from the text rows, because `CHAR_BAND`
must stay exactly one sprite tall: Mario bounces a digit with his head. Pac-Man,
TRON and Bomberman draw their own digits and derive their own row geometry.

## Never do these

- **Never push the whole frame unconditionally.** `display()` pushes only rows
  whose FNV-1a hash changed; a full push is ~61 ms on the 4.0″ (~16 fps).
- **Never allocate the canvas in a constructor.** Globals are constructed before
  FreeRTOS adds the startup-stack regions to the heap, and the 76.8 KB 4.0″
  canvas failed to allocate there. `allocateBuffer()` runs at the top of `setup()`.
- **Never drop `tft.setSwapBytes(true)`.** GFXcanvas16 is host-order RGB565;
  without it yellow renders purple and red blue, while white and black look fine.
- **Never remove `USE_HSPI_PORT`.** TFT_eSPI defaults to VSPI, which the own-bus
  touch driver also claims — touch then fails intermittently.
- **`TOUCH_CS` picks the touch wiring — define it only on shared-bus boards.**
  The 4.0″ sets `TOUCH_CS=33` so TFT_eSPI drives touch; on the 2.4″/2.8″ it would
  make TFT_eSPI drive CS 33 alongside XPT2046_Touchscreen.
- **Never unpin the platform.** `espressif32@6.12.0` (arduino-esp32 2.0.17) is
  required — upstream uses the 2.x API; unpinned resolves to 3.x and fails.
- **Never let the canvas rotate.** Its `rotation` must stay 0 or the buffer stops
  being row-major and row hashing breaks. Landscape comes from `tft.setRotation`.
- **Never narrow Tetris' well row back to `uint32_t`.** 40 columns at 160 px, 60
  at 240 — `TetRow` is `uint64_t`, with `TET_FULLROW` and `tet_clear_mask`.
- **Never drop the upstream credit.** The web UI and `/api/info` name this repo
  *and* "Based on AnimatedPixelClock by Keralots", from `PROJECT_*` and
  `UPSTREAM_*` in `include/config.h`; `LICENSE` keeps both copyrights.
- **Never restore `.github/FUNDING.yml`** — those links are the upstream author's.

## Deliberate deviations from the global rules

- **ArduinoJson v7**, not v6 — upstream's ~4 k lines of web/settings code use it.
- **POSIX `TZ` + SNTP**, not ezTime — `timezones.cpp` drives the web UI selector.
- **Adafruit GFX 5×7 font**, not VLW. The digits are pixel art on a chunky canvas;
  the VLW rule still applies to native-resolution text outside the canvas.
- **`debugLevel` is `extern`**, not header-`static` — ~25 translation units, so a
  header `static` would give each its own copy and break `/api/debug`.

## Persistence

NVS namespace `pixelclock`; touch calibration in `cydtouch`. Each is named only in
its owning module, and a factory reset asks each module to clear its own. The
384 KB `spiffs` partition is unused, kept so a filesystem can be added later.

## Archive

`archive/` holds upstream assets this port does not build — PC metrics, the
visualizer, ambient screensavers, the `.pca` player and all HUB75 material.
Nothing there is compiled; `archive/README.md` says what restoring each takes. A
pristine upstream copy sits at `PlatformIO/Projects/AnimatedPixelClock`.
