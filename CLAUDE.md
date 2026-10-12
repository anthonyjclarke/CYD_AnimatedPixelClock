# CLAUDE.md — CYD_AnimatedPixelClock

Port of **AnimatedPixelClock** (Keralots, MIT, upstream v2.3.0) from ESP32-S3 +
HUB75 to the ESP32 CYD. Uncompiled upstream assets: `archive/` (see its README);
a pristine upstream copy sits at `PlatformIO/Projects/AnimatedPixelClock`.

`FIRMWARE_VERSION` (`include/config.h`) is the only version source. Releases:
`main`, tagged `vX.Y.Z`; `dev` has a `-dev` suffix. Never release from `dev` or
tag with the suffix — it reaches serial, web UI, `/api/info`, mDNS and Improv.

## Target hardware

Three envs, all plain `esp32dev`, 4 MB, no PSRAM: `esp32-cyd-24`, `esp32-cyd-28`
(ILI9341 320×240) and `esp32-cyd-40` (ESP32-32E, ST7796S 480×320). Display on
HSPI's native pins. XPT2046 touch has its own VSPI pins on the 2.4″/2.8″ but
shares the display's SPI lines on the 4.0″ — see `TOUCH_CS`. RGB LED red is
GPIO 22 on the 4.0″ (4 elsewhere). Capacitive boards: `HAS_RESISTIVE_TOUCH=0`.

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

Every style positions itself from these canvas-derived metrics, never literals:

- **Text scales with the canvas.** `DIGIT_TEXT_SIZE` holds the digit row at ~75%
  of the width on any board, the proportion upstream had.
- **Sprite art is magnified, never redrawn.** It is fixed pixel work, so
  `SPRITE_SCALE` expands it at draw time — `CydDisplay::setSpriteScale`.

The stack is built bottom-up from the text rows: `CHAR_BAND` must stay exactly
one sprite tall, as Mario bounces a digit with his head. Pac-Man, TRON, Bomberman
and Doom Fire derive their own row geometry; Doom Fire's grid is 2×2 px cells.

## Never do these

- **Never push the whole frame unconditionally.** `display()` pushes only rows
  whose FNV-1a hash changed; a full push is ~61 ms on the 4.0″ (~16 fps).
- **Never allocate the canvas in a constructor.** Globals run before FreeRTOS adds
  the startup-stack regions to the heap; the 76.8 KB 4.0″ canvas failed there.
- **Never drop `tft.setSwapBytes(true)`.** GFXcanvas16 is host-order RGB565;
  without it yellow renders purple and red blue, while white and black look fine.
- **Never remove `USE_HSPI_PORT`.** TFT_eSPI defaults to VSPI, which the own-bus
  touch driver also claims — touch then fails intermittently.
- **Define `TOUCH_CS` only on shared-bus boards** (4.0″: TFT_eSPI drives touch).
  On the 2.4″/2.8″ TFT_eSPI would drive CS 33 alongside XPT2046_Touchscreen.
- **Never unpin the platform.** `espressif32@6.12.0` (arduino-esp32 2.0.17) is
  required — upstream uses the 2.x API; unpinned resolves to 3.x and fails.
- **Never let the canvas rotate.** Its `rotation` must stay 0 or the buffer stops
  being row-major and row hashing breaks. Landscape comes from `tft.setRotation`.
- **Never remove `WEATHER_PLAIN_HTTP` from the 4.0″ env.** Its canvas leaves
  ~55 KB internal RAM; a TLS handshake peaks near 56 KB, so HTTPS fails (`-1`).
- **Never narrow Tetris' well row back to `uint32_t`.** 40 columns at 160 px, 60
  at 240 — `TetRow` is `uint64_t`, with `TET_FULLROW` and `tet_clear_mask`.
- **Never drop the upstream credit.** Web UI and `/api/info` name this repo *and*
  "Based on AnimatedPixelClock by Keralots"; `LICENSE` keeps both copyrights.
- **Never change how the port differs from upstream without updating
  `DEVIATIONS.md` in the same commit** — added, removed or reworked behaviour.
- **Never put the Improv library back in `lib_deps`.** It is vendored in
  `lib/ImprovWiFi` with a parser fix; the registry copy drops every 2nd request.
  Its leading `'\n'` before each packet is required too: ESP Web Tools ignores
  a packet that does not start a line, and Connect then offers Install.
- **Never restore `.github/FUNDING.yml`** — those links are the upstream author's.

## Web installer and releases

Release images come only from `.github/workflows/firmware.yml` on a `v*` tag; a
local build compiles in `secrets.h`. Procedure: `docs/RELEASING.md`.

- **Never put `firmware-merged.bin` in a manifest.** `merge_bin` fills NVS
  (`0x9000`–`0xdfff`) with `0xFF`; an "Update" would wipe WiFi and settings.
- **`PROJECT_NAME` and the partition table are frozen.** ESP Web Tools offers
  Update (no erase) only when Improv's name equals the manifest `name`; a
  partition change needs an erase, stated in the release notes.

## Deliberate deviations from the global rules

- **ArduinoJson v7**, not v6 — upstream's ~4 k lines of web/settings code use it.
- **POSIX `TZ` + SNTP**, not ezTime — `timezones.cpp` drives the web UI selector.
- **Adafruit GFX 5×7 font**, not VLW. The digits are pixel art on a chunky canvas;
  the VLW rule still applies to native-resolution text outside the canvas.
- **`debugLevel` is `extern`**, not header-`static` — ~25 translation units, so a
  header `static` would give each its own copy and break `/api/debug`.

## Persistence

NVS namespace `pixelclock`; touch calibration in `cydtouch`; crash report in
`health`. Each is named only in its owning module, and a factory reset asks
each module to clear its own. The 384 KB `spiffs` partition is unused.
