# Changelog

All notable changes to CYD_AnimatedPixelClock are recorded here.

This project is a port of [AnimatedPixelClock](https://github.com/Keralots/AnimatedPixelClock)
by Keralots (MIT) from ESP32-S3 + HUB75 RGB matrix hardware to the ESP32 Cheap
Yellow Display. Versioning restarts at `1.0.0` for the port; upstream had reached
`2.3.0` at the point this fork was taken.

## Versioning

`FIRMWARE_VERSION` in `include/config.h` is the single source of truth. It is
surfaced at boot in the serial log, in the web UI header, in `/api/info`, in the
mDNS TXT record and to Improv-Serial, so a device can always be asked what it is
running. Semantic versioning: patch for fixes, minor for new behaviour, major
for a breaking settings or API change.

Releases live on `main` and are tagged `vX.Y.Z`. Work happens on `dev`, whose
`FIRMWARE_VERSION` carries a `-dev` suffix so a development build is never
mistaken for the release it will become. To cut a release: settle the
`## [Unreleased]` section under its version and date, drop the `-dev` suffix,
fast-forward `main`, tag, then open the next `-dev` cycle on `dev`.

---

## [1.2.0] 12-09-2026

Fixes found running 1.1.0 on hardware: the RGB status LED (wrong pins on the
2.4″/2.8″, never driven on the 4.0″), the weather degree sign, weather settings
that looked lost after a reboot, three settings that genuinely were, and web UI
text left over from upstream's PC-monitor mode. Confirmed on hardware.

### Changed

- **The RGB status LED flashes red, green, blue once at boot.** A connected
  clock leaves the LED dark by design, so there was no way to see whether it
  worked — and on every board it did not work as intended (below).

### Fixed

- **Weather: the degree sign sat on the last temperature digit.** Its position
  assumed 18 px per character, upstream's size-3 font, but the digits are size 4
  (24 px) on the 2.4″/2.8″ and size 6 (36 px) on the 4.0″ — so the overlap grew
  with the digit count and the board. It is now placed from where the digits
  actually end, one digit pixel clear of the last one (`WTEMP_UNIT_GAP`).
- **RGB LED red and blue were swapped on the 2.4″ and 2.8″.** Red is GPIO 4 and
  blue GPIO 17, as in the board pinouts and every other CYD project here; the
  pins came from a note with the two reversed, so the red "WiFi down" pulse
  would have shown blue.
- **The 4.0″'s RGB LED was treated as absent.** The ESP32-32E fits one — red on
  GPIO 22, green 16, blue 17 (LCDwiki E32R40T) — which the firmware never drove,
  the likeliest reason it glowed red at boot. It is now driven like the others,
  and its web UI setting takes effect.
- **The weather location looked lost after a reboot.** The coordinates were
  saved correctly — a device read back after a power cycle still had them — but
  the place name searched for was never stored, so the "Find your location" box
  came back empty with nothing saying where the clock was set. The name is now
  saved with the coordinates (`weatherPlace`, included in export and import) and
  shown as "Saved location" after a reload. Typing coordinates by hand clears it.
- **A location search did not mark the form unsaved.** Filling the coordinates
  from code fires no input event, so the save bar kept saying "All saved" and it
  was easy to leave without saving. A search hit now marks the form dirty and
  says to press Save.
- **Brightness, colon blink mode and Mario bounce height reset on every
  reboot.** Upstream's `saveSettings()` wrote them; the port dropped all three
  when trimming settings, so they were only ever written as first-boot defaults.
- **The web UI status readout always said "PC offline".** It showed whether the
  PC companion app was sending stats — a mode this port does not build — so with
  nothing ever sending `pcOnline` it read "PC offline · clock" with a grey dot,
  as if the device were down. It now reads "Online" with the selected clock style
  (or "display off"), and "Offline" if the device stops answering.
  `/api/status` gains `clockStyleName` for it.
- **Diagnostics showed "Storage: NaN / NaN KiB free" and "Animation: idle".**
  Both came from the archived `.pca` animation player, along with its playback
  and upload error lines; all four are removed.
- The factory reset warning listed "Metric labels & layout", which no longer
  exist, and omitted touch calibration, which a reset does erase. `/api/info`
  reported `model` as `AnimatedPixelClock`; it now uses the project name, as
  mDNS does.
- **The LED never showed amber for the setup portal.** The portal waits inside
  `setup()`, before `loop()` — the only place the LED was updated — ever runs.
  The portal callback now updates it.

---

## [1.1.0] 11-09-2026

**Initial working release.** Verified on a 320×240 ILI9341 CYD and a 480×320
ST7796S ESP32-32E: the display path, touch on both wirings, the web UI, WiFi, NTP
and weather. 1.0.0 ran on one board with touch that later stopped; this release
carries the fixes that got every board at hand working. What is still unverified
is listed in the [Roadmap](README.md#roadmap).

### Added

- **The device now credits this port and the original.** The web UI sidebar
  linked only to Keralots/AnimatedPixelClock, as if the device were running the
  original. It now names `CYD_AnimatedPixelClock` with a link to this repository,
  followed by "Based on AnimatedPixelClock by Keralots" linking upstream; the
  page title follows. `/api/info` gains `project`, `repository`, `basedOn` and
  `upstreamRepository`. Improv-Serial and the mDNS `model` record, which hold one
  name, report `CYD_AnimatedPixelClock`. All of it comes from the `PROJECT_*`
  and `UPSTREAM_*` constants in `include/config.h`.

- **A clock-style change is now logged from every route that can cause one.**
  Only the touch path logged it; the HTTP API and the Cycle All rotation changed
  the style silently, so a style that changed on its own left no trace. All
  three go through `applyClockStyle()`, which names the style and what asked for
  it. Cycle All logs separately because rotation deliberately does not write
  `settings.clockStyle`.
- `clockStyleName()` — logs and diagnostics say "Tetris (8)" rather than "8".
- **Touch taps are visible at the default debug level.** They were `DBG_VERBOSE`,
  so "is touch even working?" could not be answered without a rebuild. Now INFO,
  with canvas coordinates, raw reading and pressure.
- Once-a-minute status line: uptime, active style, free heap *and* the
  since-boot low-water mark (a slow leak shows there first), canvas rows pushed
  on the last frame, NTP state, WiFi SSID/IP/RSSI, and the LDR reading when
  auto-brightness is driving the backlight.
- Boot banner reporting version, board, canvas geometry and scale, sprite scale
  and character band, which hardware is fitted, and the active debug level — so
  a log excerpt identifies the build it came from.
- Backlight changes are logged, throttled so only a step worth noticing (~3%)
  is INFO. With auto-brightness the level is re-evaluated every 500 ms and the
  LDR average drifts constantly; logging every change would bury everything else.
- Minute-change animation trigger at verbose level, with the number of digits
  about to animate.

### Changed

- The 2.4″ reports itself as a 2.4″ in the boot banner, web UI and `/api/info`
  instead of borrowing the 2.8″'s name. It still defines `BOARD_CYD_28`, because
  it shares that board's LDR and RGB LED.

### Fixed

- **`include/secrets.h` was never included.** The README and the example file
  said to put hardcoded WiFi credentials there, but nothing included it, so
  following the instructions silently did nothing. `config.h` now includes it
  when present and maps `SECRET_WIFI_SSID` / `SECRET_WIFI_PASS` onto the
  hardcoded-WiFi settings. Checked with a throwaway `secrets.h`: its SSID is in
  the binary with the file and absent without it. The example no longer lists an
  OTA password and weather API key that nothing reads.
- **Web UI wording left over from upstream.** The Clock page offered "the idle
  animation shown when your PC is asleep" under an "Idle clock" heading, the
  Maintenance page gave the display model as "HUB75 Matrix", and the schedule
  hint spoke of sparing the LEDs. Each now describes this device.

- **The 4.0″ showed nothing: its canvas could not be allocated.** `CydDisplay`
  allocated its buffer in its constructor, which for a global runs before
  FreeRTOS has added the startup-stack regions to the heap. The 2.8″ canvas
  (38.4 KB) fitted there; the 4.0″ canvas (76.8 KB in one block, on a build with
  10 KB more static data) did not, so `begin()` returned before initialising the
  panel. The buffer is now allocated at the top of `setup()`. The log reports
  the largest free heap block either way, and if allocation ever fails again the
  panel is painted solid red instead of being left blank. Confirmed on a 4.0″
  ESP32-32E.
- **Touch on the 4.0″ was wired as if it were a 2.8″.** On the ESP32-32E the
  XPT2046 is on the display's SPI lines, not the dedicated CLK 25 / MISO 39 /
  MOSI 32 pins, so reads came back as zeros. The touch module now has two
  backends chosen by `TOUCH_CS`: XPT2046_Touchscreen on VSPI for the 2.4″ and
  2.8″, and TFT_eSPI's own touch support on the shared bus for the 4.0″, which
  now defines `TOUCH_CS=33`. Press and release thresholds match AuroraDemo_CYD
  on the same board. Confirmed on a 4.0″ ESP32-32E.
- **Impossible touch reads became phantom taps.** A controller that is not
  answering reads pressure 4095 with coordinates at 0 or full scale. Two boards
  did exactly that, and each time the read changed the clock style and saved
  it. Those reads are now rejected, with one warning per boot.
- A board that had never been calibrated logged "Touch calibration namespace
  unavailable" as a warning on every boot. That is the normal state, so it is
  now logged as info.
- The 4.0″ SPI clock is now 40 MHz, the speed TheFlightWall_CYD runs on the same
  board (was 27 MHz). Its BGR colour order is unchanged: BGR is also the ST7796
  default, so the two reference projects' configs never conflicted.
- The README hardware table's MCU row had two data columns under a three-board
  header since the 2.4″ was added, and named the 4.0″ an ESP32-2432S040 rather
  than the ESP32-32E actually in use. Timing figures in the docs and headers now
  reflect the 4.0″'s 40 MHz clock: a full-frame push is ~61 ms, not ~91 ms.
- **Touch shared an SPI peripheral with the display.** TFT_eSPI defaults to VSPI
  on the ESP32 unless `USE_HSPI_PORT` is set, and the XPT2046 driver claims VSPI
  too — so both were driving one peripheral, at 55 MHz and 2.5 MHz. That fails
  intermittently rather than outright, which is why touch worked for a while and
  then stopped as the draw pattern changed. `USE_HSPI_PORT` is now set, and it
  is the correct port anyway: MOSI 13 / MISO 12 / SCLK 14 / CS 15 are HSPI's
  native pins, so the display gets direct hardware mapping rather than the GPIO
  matrix. The comment in `touch.cpp` asserted the display "already owns HSPI" —
  an assumption written and never checked.
- **Saving from the web UI changed the clock style without logging it.** The
  debug work claimed every style-change route logged; the Save handler was a
  fourth route it missed, found when a hardware log showed Bomberman become
  Matrix Rain with no trace. It now logs the change, tagged `[web ui]`.
- **The clock picker called style 9 "Custom rotation"** while two hints on the
  same page, the logs, the README and upstream all called it "Cycle All". Now
  "Cycle All" everywhere, and the rotation card is headed to match.
- **Factory reset erased nothing.** `handleReset()` still opened the upstream
  `"pcmonitor"` NVS namespace, which this port renamed to `"pixelclock"` — so a
  reset cleared an empty legacy namespace, wiped the WiFi credentials, rebooted,
  and left every setting exactly as it was. Since a reset is the only practical
  way to pick up the v1.0.0 defaults audit, it would have failed silently at the
  moment it mattered most.
- Each module now clears its own namespace — `factoryResetSettings()` in
  `settings.cpp`, `touchClearCalibration()` in `touch.cpp` — so the name lives
  in exactly one place per namespace and the handler cannot drift from it again.
  Touch calibration is now cleared too, which a factory reset should always have
  done.

See the [Roadmap](README.md#roadmap) for what else is queued.

---

## [1.0.0] 11-09-2026

First release. The complete port, running on a CYD 2.4″.

Fourteen clock styles, the configuration web interface, OTA, WiFi provisioning,
scheduled dimming and the CYD hardware integration (touch, LDR, RGB status LED)
all build on three board targets. The display path — canvas, scaled blit, colour
order and row-change detection — is confirmed on hardware; the 2.8″ and 4.0″
targets are build-only, and the clock layouts beyond Mario and Space have been
seen running but not judged style by style. See **Port status** in `README.md`.

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
  sprite art is magnified at draw time rather than redrawn (see *sprite
  magnification* below). The vertical composition is anchored on the character band staying one
  sprite tall, because Mario bounces a digit from directly underneath it.
- Tetris' well grew from 5 rows to 11 (13 → 25 in small-clock mode). Its row
  mask had to widen from `uint32_t` to `uint64_t`: a 4 px cell over a 160 px
  canvas is 40 columns, and 60 on the 4.0″, both past the 32 bits upstream had.
  The full-row constant and clear mask widened with it.
- Snake's flow-field arena grew from 32×16 cells to 40×30 (60×40 on the 4.0″).
- Pac-Man, TRON and Bomberman draw their own digits — a pellet grid, seven
  segments and bricks respectively — so each derives its own row geometry
  rather than reusing `DIGIT_X`.
- Static RAM rose to 21.8% / 25.0% (from 20.0% / 20.1%), almost entirely the
  larger Snake flow-field and Tetris well arrays.
- Backlight PWM (`ledc`, GPIO 21 on the 2.8″, GPIO 27 on the 4.0″) now backs
  `setBrightness8()`, so upstream's scheduled dimming and nightly-off windows
  work unmodified against LCD hardware.
- Logical canvas is 160×120 on the 2.8″ and 240×160 on the 4.0″, each scaled ×2
  to fill its panel exactly. Upstream drew 128×64; the clock styles were
  re-laid-out for the larger canvas rather than letterboxed.

### Changed — Mario redrawn

- **Mario is a faithful 12×16 sprite instead of an 8×10 abstraction.** Upstream's
  figure had no face at all — the head was one solid red block and the torso one
  solid blue block, with no hair, eye, moustache, shirt or buttons — which is why
  it read as a red-and-blue blob rather than as Mario.
- The art now lives in `src/clocks/mario_sprites.h` as rows of characters, one
  character per pixel, so it can be read and edited in place rather than being
  buried in ~150 `fillRect` calls. Four frames: stand, two walk, jump.
  Left-facing mirrors at draw time rather than needing separate art.
- Six new colour slots the old figure had nowhere to put: hair, overall buttons,
  and four for the scenery below.
- Upstream's Mario palette was tuned for a blockier figure and does not survive
  a sprite with a face: skin `0xFDB8` rendered pink and shoes `0xA145` reddish.
  Now peach and brown, the latter matching the hair as in the original.

### Added — classic scenery

- `marioScenery` setting (default on) fills the sky band above the clock with
  World 1-1 furniture: two clouds drifting at different heights and speeds, a
  stepped hill, a bush, and the ground strip Mario walks on. Drawn before the
  digits and before Mario, so he passes in front of it as he does in the game.
- Everything is sized in sprite pixels and magnified by `SPRITE_SCALE`, so the
  scenery stays in proportion to Mario at any scale, and it skips the sky
  elements entirely when a large scale has left too little sky to hold them.

### Added — sprite magnification

- `SPRITE_SCALE` build option. Character art is fixed pixel work, so it cannot
  simply be redrawn larger, and drawn 1:1 against a digit row this much bigger it
  read as tiny.
  The magnification happens at draw time in `CydDisplay`, which overrides the
  three primitives every GFX shape funnels through and expands each drawn pixel
  into a `scale × scale` block about an anchor. The art and its ~150 call sites
  are untouched.
- Defaults to a third of the digit text size — ×1 on a 160×120 canvas, ×2 on
  240×160 — which puts the 12×16 Mario at half the digit height on the first and
  two-thirds on the second. Upstream's figure was 42% of its digit row. Override
  it per board env.
- `CHAR_BAND` and `MARIO_HEAD_OFFSET` derive from the scale, so a magnified
  character still reaches the underside of the digit row to bounce it; the `+4`
  in `CHAR_BAND` preserves upstream's 4 px of jump at any scale.
- A `static_assert` rejects a scale too large for the canvas: ×4 on 160×120 and
  ×6 on 240×160, where the digit row would start above the top edge.
- Only Mario's sprites are magnified so far. The other styles' characters still
  draw 1:1; the mechanism is in place for them.
- The vertical layout is now built bottom-up — text rows, then the character
  band, with whatever remains becoming sky. Sizing top-down let a larger
  `SPRITE_SCALE` push the date and day rows off the bottom of the canvas.

### Fixed — defaults audit

Upstream's defaults were tuned for a 128×64 panel where screen space was scarce.
On a canvas with 25% more width and 87% more height several read as "broken"
rather than "conservative". Audited in one pass:

- **Matrix Rain covered less than half the canvas.** `MX_COLS` was fixed at 21
  and `MX_ROWS` at 8 — a 126×64 px grid — so the right 21% and bottom 47% of a
  160×120 canvas had no rain at all. Both now derive from the canvas: 26×15
  cells here, 39×20 on the 4.0″.
- **Digit collision boxes were 20% too small** in Matrix, Asteroids and Dino.
  All three still used `16×21`, the size-3 glyph, where the digits are now size
  4 (20×28 inked) — so digit shatter, decode masking and digit swap did not
  cover the digits they targeted. Now `DIGIT_GLYPH_W` / `7 * DIGIT_TEXT_SIZE`.
- Asteroids' pellet pitch and the centred-digit positions in Matrix, Asteroids
  and Dino were likewise still size-3 constants.
- Dino's fixed screen position was an absolute 12 px; now proportional.

Defaults changed, each because the canvas moved rather than as a taste call:

| Setting                                   | Was | Now | Why                                |
| :---------------------------------------- | :-- | :-- | :--------------------------------- |
| Show date: Asteroids, Dino, Matrix, Snake | off | on  | Cost 16% of a 64 px panel, 8% here |
| Asteroids rock count                      | 2   | 3   | Play area is 2.3× larger           |
| Pac-Man patrol pellets                    | 8   | 10  | Patrol row is 25% wider            |
| Snake body length                         | 8   | 10  | Arena grew from 512 to 1200 cells  |
| Pong paddle width                         | 20  | 25  | Canvas is 25% wider                |
| Space patrol speed                        | 0.5 | 0.7 | Patrol span grew 36%               |
| Mario smooth animation                    | off | on  | Sprite now has four real frames    |

Left alone deliberately: Matrix density (the grid fix already raised it in
absolute terms), Tetris small-clock mode (a large behavioural change, not a
sizing one), Snake wall border, Asteroids transparency, TRON bike style and
Dino clouds — all genuine taste settings that the canvas change does not touch.

### Fixed

- **A `ColorSlot` could ship with no default.** `color_slots.h` declared
  `SPRITE_COLOR_DEFAULTS` extern *with* an explicit `[COL_COUNT]` bound, which
  completed the definition's type in `settings.cpp`, so `sizeof()` there always
  reported `COL_COUNT` however many values were actually listed — making the
  `static_assert` that guards them tautological. Six new slots were added with
  no defaults and the build passed. The extern is now unbounded, the assert is
  meaningful, and it was verified to fire by deleting an entry.
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
