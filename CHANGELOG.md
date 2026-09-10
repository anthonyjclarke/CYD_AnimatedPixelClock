# Changelog

All notable changes to CYD_AnimatedPixelClock are recorded here.

This project is a port of [AnimatedPixelClock](https://github.com/Keralots/AnimatedPixelClock)
by Keralots (MIT) from ESP32-S3 + HUB75 RGB matrix hardware to the ESP32 Cheap
Yellow Display. Versioning restarts at `1.0.0` for the port; upstream had reached
`2.3.0` at the point this fork was taken.

---

## [Unreleased] 10-09-2026

Complete port. **Both board targets build**; nothing has been flashed or
verified on hardware. See **Port status** in `README.md`.

### Added

- Three PlatformIO board environments — `esp32-cyd-24` and `esp32-cyd-28`
  (ILI9341 320×240) and `esp32-cyd-40` (ST7796S 480×320) — sharing one
  `[common]` block.
- `HAS_RESISTIVE_TOUCH` build flag. The capacitive CYD revisions fit a CST820 on
  I2C using some of the same GPIOs as the resistive XPT2046, so setting it to 0
  compiles the touch module to stubs rather than driving those pins as SPI.
- `src/display/cyd_display.h` / `.cpp` — Adafruit-GFX canvas backed by TFT_eSPI,
  replacing the upstream HUB75 DMA shim. Presents the identical call surface
  (`clearDisplay` / GFX draws / `display`), so clock styles need no display edits.
- Per-row change detection in `CydDisplay::display()`. A full-frame push costs
  ~22 ms at 320×240/55 MHz and ~91 ms at 480×320/27 MHz; hashing each canvas row
  and pushing only changed rows keeps both boards usable. Costs 4 bytes per row
  rather than the 37–75 KB a shadow framebuffer would need.
- `include/config.h` — all user-tuneable constants, including canvas geometry,
  XPT2046 touch pins, LDR thresholds and RGB-LED pins.
- `include/debug.h` — standard leveled `DBG_*` macros. `debugLevel` is `extern`
  and defined once, not `static` in the header as in the single-file CYD
  projects; this project spans ~25 translation units, where a header `static`
  would give each file a private copy and break the runtime web control.
- `include/secrets.h.example` — template for the gitignored `include/secrets.h`.
- `partitions_custom.csv` — standard 4 MB dual-OTA layout, verified to map
  exactly to `0x400000` with no gaps or overlaps.
- `smoke/` and the `smoke-28` / `smoke-40` environments — a temporary
  compile-and-link check for the display layer, used to validate it before the
  rest of the tree compiled. Removed now that the full firmware builds.
- `archive/` with `archive/README.md` documenting every retained asset.
- `src/touch/` — XPT2046 on its own VSPI instance. Tap anywhere to advance the
  clock style; calibration bounds live in the `cydtouch` NVS namespace.
- `src/sensors/ldr.cpp` — rolling-averaged ambient light on GPIO 34 driving the
  backlight, layered under the existing dim/off schedule so a dark room cannot
  override the nightly-off window.
- `src/status_led/` — onboard RGB LED reporting AP-portal, disconnected and
  notification states. Common anode, so the PWM writes are inverted.
- "CYD hardware" card in the web UI for the three settings above, plus an
  auto-brightness floor. On the 4.0″ board, which populates neither the LDR nor
  the RGB LED, the controls say so rather than silently doing nothing.
- `/api/status` now reports `rowsPushed` and `canvasRows`, making the display
  layer's change detection measurable on real content.
- `src/clocks/clock_layout.h` — canvas-derived layout metrics. Every style
  positions itself from these instead of the literals upstream tuned for a
  128×64 panel, so a new board is a new `[env:]` block rather than another pass
  over fourteen files.

### Changed

- `Settings` reduced from 129 to 103 fields; the NVS namespace is renamed from
  upstream's `pcmonitor` to `pixelclock`, which no longer describes a mode this
  firmware has. Persistence is per-key, so removed fields simply orphan their
  keys — no migration needed, and no existing CYD installs to migrate.
- All 64 remaining `Serial.print*` call sites converted to leveled `DBG_*`
  macros, classified by message: 53 info, 20 warn, 7 error.
- Auto-brightness re-evaluates every 500 ms rather than riding the scheduled
  dimming check's one-minute throttle, which would have left the panel visibly
  lagging the room after a light was switched on.
- `isAnimationActive()` no longer bails out when the PC-metrics screen is up;
  that mode does not exist here, so a clock is always what is on screen.
- `/api/mode/clock` and `/api/mode/auto` are retained as no-ops for Home
  Assistant automations; `/api/mode/ambient` and `/api/mode/viz` are gone.
- **All 14 clock styles re-laid-out** for the larger canvas. Digit rows now hold
  upstream's ~70%-of-width proportion (75%) at whatever size the board needs;
  sprites keep their logical size, so they stay the same physical size on both
  panels. The vertical composition is anchored on the character band staying one
  sprite tall, because Mario bounces a digit from directly underneath it.
- Tetris' well grew from 5 rows to 11 (13 → 25 in small-clock mode). Its row
  mask had to widen from `uint32_t` to `uint64_t`: a 4 px cell over a 160 px
  canvas is 40 columns, and 60 on the 4.0″, both past the 32 bits upstream had.
  The full-row constant and clear mask widened with it.
- Snake's flow-field arena grew from 32×16 cells to 40×30 (60×40 on the 4.0″).
- Pac-Man, TRON and Bomberman draw their own digits — a pellet grid, seven
  segments and bricks respectively — so each derives its own row geometry
  rather than reusing `DIGIT_X`.
- Static RAM rose to 21.7% / 24.7% (from 20.0% / 20.1%), almost entirely the
  larger Snake flow-field and Tetris well arrays.
- Backlight PWM (`ledc`, GPIO 21 on the 2.8″, GPIO 27 on the 4.0″) now backs
  `setBrightness8()`, so upstream's scheduled dimming and nightly-off windows
  work unmodified against LCD hardware.
- Logical canvas is 160×120 on the 2.8″ and 240×160 on the 4.0″, each scaled ×2
  to fill its panel exactly. Upstream drew 128×64; clock-style layouts are being
  reworked to the larger canvas rather than letterboxed.

### Fixed

- **Colours rendered byte-swapped on the panel.** `GFXcanvas16` stores RGB565 in
  host order, but TFT_eSPI defaults to `_swapBytes = false` and pushes an image
  array to the display byte-for-byte, and the panel wants big-endian. Yellow
  (`0xFFE0`) arrived as `0xE0FF` and rendered purple; red rendered blue, green
  rendered red. White and black are palindromes so they looked correct, which
  disguised the fault as "some colours are wrong". `CydDisplay::begin()` now
  calls `tft.setSwapBytes(true)`. Found on hardware — the canvas side had been
  verified as host-order, but the consuming side never was.

### Removed

Moved to `archive/`, not deleted — see `archive/README.md` to restore any of it.

- PC-statistics mode (`src/metrics/`), its UDP transport and the Windows/Linux
  companion application.
- Audio spectrum visualizer (`src/viz/`).
- Ambient screensavers and the LittleFS `.pca` custom-animation player
  (`src/ambient/`), plus the `gif2pca.py` converter.
- HUB75 hardware assets: bring-up sketch, wiring diagram generator, prototype
  photographs, and the upstream ESP32-S3 release binaries and web flasher.
- Upstream `FUNDING.yml`; sponsorship links belong to the original author and
  should not be served from a fork. Credit is given in `README.md` and `LICENSE`.
- Web UI pages for the dropped subsystems — "Audio visualizer", "Display
  layout" and "Visible metrics" — along with the Ambient screensaver card and
  the ~300 lines of `PORTAL_JS` that drove them. Left in place they would have
  rendered as dead controls, and the metrics editor's unguarded startup code
  would have thrown and broken the whole portal.
