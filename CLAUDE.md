# CLAUDE.md — CYD_AnimatedPixelClock

Port of **AnimatedPixelClock** (Keralots, MIT, upstream v2.3.0) from ESP32-S3 +
2× 64×64 HUB75 panels to the ESP32 Cheap Yellow Display. Port version 1.0.0,
in progress — see *Port status* in `README.md`.

## Target hardware

Two envs: `esp32-cyd-28` (ESP32-2432S028R, ILI9341 320×240, backlight GPIO 21,
SPI 55 MHz) and `esp32-cyd-40` (ESP32-2432S040, ST7796S 480×320, backlight
GPIO 27, SPI 27 MHz, `TFT_RGB_ORDER=TFT_BGR`). Both are plain `esp32dev`, 4 MB,
no PSRAM. Standard CYD SPI display pins apply; only the backlight GPIO differs.

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
- `CydDisplay` deliberately exposes the *upstream HUB75 shim's* API
  (`clearDisplay` / `clearScreen` / `display` / `setBrightness8` /
  `waitForScanCompletion`) so ported styles need no display edits.
  `waitForScanCompletion()` is an intentional no-op — TFT pushes are synchronous.

## Never do these

- **Never push the whole frame unconditionally.** `display()` hashes each canvas
  row (FNV-1a) and pushes only changed rows. A full push is ~22 ms on the 2.8″
  and ~91 ms on the 4.0″ — the latter would cap that board near 11 fps. A shadow
  framebuffer was rejected: 37–75 KB against a ~200 KB free heap, where the 4.0″
  canvas alone is already 75 KB.
- **Never define `TOUCH_CS` as a build flag.** Touch is driven by
  XPT2046_Touchscreen on its own VSPI instance; defining it makes TFT_eSPI claim
  the same chip select and both drivers fight over the bus.
- **Never unpin the platform.** `espressif32@6.12.0` (arduino-esp32 2.0.17) is
  required — upstream targets the 2.x API (`esp_task_wdt_init(timeout, panic)`).
  Unpinned resolves to the pioarduino 3.x fork and the build fails.
- **Never let the canvas rotate.** `GFXcanvas16` has its own `rotation`; it must
  stay 0 or the buffer layout stops being row-major and row hashing breaks.
  Landscape comes from `tft.setRotation(TFT_ROTATION)` on the panel instead.
- **Never restore `.github/FUNDING.yml`** from `archive/` — those sponsorship
  links are the upstream author's.

## Deliberate deviations from the global rules

Each is a considered exception, not an oversight:

- **ArduinoJson v7**, not v6. Upstream's ~4 k lines of web/settings code is
  written against the v7 `JsonDocument` API; downgrading means rewriting it all.
- **POSIX `TZ` strings + SNTP**, not ezTime. Upstream `timezones.cpp` drives the
  web UI's timezone selector; ezTime would break that selector.
- **Adafruit GFX built-in 5×7 font**, not VLW. The clock digits are pixel art on
  a chunky canvas — a smooth font defeats the entire aesthetic. The VLW rule
  still applies to any native-resolution TFT text drawn outside the canvas.
- **`debugLevel` is `extern`**, not header-`static` as in the single-file CYD
  projects. ~25 translation units here; a header `static` would give each its
  own copy and silently break the `/api/debug` runtime control.

## Persistence

Settings live in NVS via `Preferences`. Touch calibration is stored there too and
must never be hardcoded; run the on-screen calibration when the namespace is
empty. The 384 KB `spiffs` partition is currently unused and retained only so a
filesystem can be added later without repartitioning and losing saved settings.

## Archive

`archive/` holds upstream assets this port does not build: PC-metrics mode, the
audio visualizer, ambient screensavers and the `.pca` player, plus all HUB75
hardware and release material. Nothing there is compiled. `archive/README.md`
says what each folder is and what restoring it would take. A pristine upstream
copy also sits outside the repo at `PlatformIO/Projects/AnimatedPixelClock`.

`smoke/` and the `smoke-28` / `smoke-40` envs are a temporary display-layer
compile check — delete both once the full firmware builds.
