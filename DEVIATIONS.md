# Deviations from upstream

Every way CYD_AnimatedPixelClock differs from the project it is based on,
[AnimatedPixelClock][upstream] by Keralots.

This is a comparison, not a history. `CHANGELOG.md` records *when* something
changed; this file records *what is different today*. Fixes to the port's own
bugs are not deviations and do not appear here. Any change that makes the port
differ from upstream — or brings it back into line — updates this file in the
same commit.

[upstream]: https://github.com/Keralots/AnimatedPixelClock

| Item                       | Value                           |
| :------------------------- | :------------------------------ |
| Upstream release ported    | v2.3.0, released 09-09-2026     |
| Later upstream work ported | `f504bd4` Doom Fire, 13-09-2026 |
| Upstream last compared     | `main` on 15-09-2026            |
| Port version numbering     | Restarted at 1.0.0              |

---

## Hardware and platform

- **Target hardware.** Upstream drives two chained 64×64 HUB75 RGB matrix
  panels from an ESP32-S3. The port targets the ESP32 Cheap Yellow Display in
  three PlatformIO environments: 2.4″ and 2.8″ (ILI9341, 320×240) and the 4.0″
  ESP32-32E (ST7796S, 480×320).
- **Display driver.** Upstream uses the HUB75 DMA library behind a small
  Adafruit-GFX shim. The port's `CydDisplay` is an off-screen `GFXcanvas16`
  pushed to TFT_eSPI, exposing the same calls so the clock styles compile
  unchanged. It pushes only the rows that changed. `setBrightness8()` drives
  the backlight PWM instead of panel brightness, and `waitForScanCompletion()`
  is a no-op because a TFT push is synchronous.
- **Canvas.** Upstream draws 128×64. The port draws 160×120 on the 2.4″/2.8″
  and 240×160 on the 4.0″, each at 2×2 physical pixels, filling the panel
  exactly.
- **Platform.** The port pins `espressif32@6.12.0` (arduino-esp32 2.0.17), the
  2.x core upstream's code is written against. Unpinned, it resolves to 3.x and
  fails to build.
- **Flash layout.** The port uses a 4 MB dual-OTA partition table for the CYD:
  two 1.79 MB app slots and a 384 KB filesystem partition, currently unused.
- **Display SPI port.** The TFT runs on HSPI (`USE_HSPI_PORT`), leaving VSPI to
  the 2.4″/2.8″ touch controller. Upstream has no second SPI device.

---

## Added in the port

- **Touch.** Tapping the screen moves to the next clock style and saves it. The
  XPT2046 has its own VSPI bus on the 2.4″/2.8″ and shares the display's SPI
  lines on the 4.0″. Impossible reads are ignored rather than becoming taps.
  Calibration bounds live in the `cydtouch` NVS namespace. Upstream has no
  input device.
- **Auto-brightness.** The onboard LDR (GPIO 34, 2.4″/2.8″) drives the
  backlight, underneath the dim and off schedule. Upstream has no light sensor.
- **RGB status LED.** Amber while the setup portal is open, a red pulse while
  WiFi is down, a blue pulse for a notification, dark otherwise. It flashes
  red, green, blue once at boot. Red is GPIO 4 (22 on the 4.0″), green 16, blue
  17.
- **"CYD hardware" web UI card** for touch, auto-brightness and its floor, and
  the status LED. On a board without a part, the card says so.
- **Board identity.** The boot banner, web UI and `/api/info` name the board
  and display model.
- **Credits on the device.** The sidebar, page title and `/api/info`
  (`project`, `repository`, `basedOn`, `upstreamRepository`) name this port and
  "Based on AnimatedPixelClock by Keralots". Improv-Serial and the mDNS `model`
  record report `CYD_AnimatedPixelClock`.
- **Weather place name.** The place found by the location search is saved
  (`weatherPlace`, included in export and import) and shown after a reload.
  Upstream keeps only the coordinates. A search hit also marks the form unsaved.
- **Diagnostics.**
  - A boot banner reports version, board, canvas, sprite scale, fitted hardware
    and debug level.
  - A once-a-minute status line reports uptime, clock style, free heap and its
    low-water mark, rows pushed, NTP and WiFi.
  - Every clock-style change is logged with what caused it: touch, web UI, API
    or Cycle All. Taps are logged at INFO.
  - `/api/status` adds `rowsPushed`, `canvasRows` and `clockStyleName`.
- **`include/secrets.h`** is included automatically when present, supplying
  `SECRET_WIFI_SSID` / `SECRET_WIFI_PASS` as the hardcoded-WiFi fallback.

---

## Removed from the port

Everything below is archived under `archive/`, not deleted. `archive/README.md`
says what reviving each part would take.

- **PC-statistics mode**, its UDP transport and the Windows/Linux companion app.
- **Audio spectrum visualizer.**
- **Ambient screensaver**, the `.pca` custom-animation player and `gif2pca.py`.
- **HUB75 hardware material:** the bring-up sketch, wiring diagram generator,
  prototype photographs, ESP32-S3 release binaries and the web flasher.
- **Upstream's `FUNDING.yml`.** Its sponsorship links belong to the upstream
  author.
- **Web UI:** the Audio visualizer, Display layout and Visible metrics pages,
  the Ambient screensaver card, and the page script behind them.
- **API:** `/api/mode/ambient` and `/api/mode/viz`. `/api/mode/clock` and
  `/api/mode/auto` remain as no-ops so existing Home Assistant automations still
  work.
- **Settings** for the removed modes. Their NVS keys are simply never read.
- **Doom Fire's skipped clear.** Upstream skips the render loop's
  `clearDisplay()` for style 17, to avoid a HUB75 scan flash. The CYD canvas is
  off-screen, so the port keeps the clear.

---

## Layout and rendering

- **Canvas-derived layout.** Upstream positions every style with literal
  coordinates for 128×64. The port positions them from
  `src/clocks/clock_layout.h`, derived from the canvas size.
- **Digit size.** Upstream digits are text size 3 (18 px each). The port uses
  size 4 on 160×120 and 6 on 240×160, keeping the time at ~75% of the width.
  Positions upstream tied to size 3 — digit collision boxes, pellet pitch, the
  weather degree sign — are derived from `DIGIT_TEXT_SIZE`.
- **Sprite magnification.** `SPRITE_SCALE` magnifies character art at draw time:
  ×1 on 160×120 and ×2 on 240×160 by default. Upstream draws sprites 1:1. Only
  Mario uses it so far.
- **Vertical composition.** Built bottom-up from the text rows, so the character
  band stays exactly one sprite tall.
- **Matrix Rain.** The rain grid is derived from the canvas; upstream's is fixed
  at 21×8 cells.
- **Tetris.** The well is 11 rows (25 in small-clock mode) instead of 5 (13),
  and its row mask is 64 bits instead of 32.
- **Snake.** The arena is 40×30 cells (60×40 on the 4.0″) instead of 32×16.
- **Dino Runner.** Its position is proportional to the width, not a fixed
  12 px.
- **Doom Fire.** The fire runs on 2×2-pixel cells — 80×60, or 120×80 on the
  4.0″ — instead of one cell per pixel, so its height settings count fire rows.
  The digits are drawn over it at full resolution.

---

## Clock styles

- **Mario.** A 12×16 NES sprite with four frames, drawn from character art in
  `mario_sprites.h`. Upstream draws an 8×10 block figure with no face. Skin and
  shoe colours are corrected, new colour slots cover hair, buttons and scenery,
  and optional World 1-1 scenery (`marioScenery`) fills the sky.
- **Cycle All.** The style picker calls style 9 "Cycle All"; upstream's picker
  says "Custom rotation".

### Defaults

| Setting                                   | Upstream | Port    |
| :---------------------------------------- | :------- | :------ |
| Mario idle encounters                     | off      | on      |
| Mario smooth animation                    | off      | on      |
| Space Invaders character                  | ship     | invader |
| Show date: Asteroids, Dino, Matrix, Snake | off      | on      |
| Asteroids rock count                      | 2        | 3       |
| Pac-Man patrol pellets                    | 8        | 10      |
| Snake body length                         | 8        | 10      |
| Pong paddle width                         | 20       | 25      |
| Space patrol speed                        | 0.5      | 0.7     |

Each changed because the larger canvas made upstream's value read as broken or
empty, not as a matter of taste.

---

## Settings and persistence

- **NVS namespace.** `pixelclock` instead of upstream's `pcmonitor`, with touch
  calibration separately in `cydtouch`. Settings are not migrated from upstream.
- **Factory reset** asks each module to clear its own namespace, so it erases
  touch calibration too.
- **Colour slot order.** After `COL_SCOPE_PEAK` the port has six Mario slots,
  then Doom Fire's four; upstream has Doom Fire's straight after
  `COL_SCOPE_PEAK`. Config export writes colours as an array indexed by slot, so
  colours from an upstream export land on the wrong slots in the port, and the
  reverse. Every other exported setting maps by name.
- **Settings struct.** The `Settings` struct lives in `src/config/globals.h`, and
  user-tuneable constants in `include/config.h`. Upstream keeps both in
  `src/config/config.h`.

---

## Web UI and API

- **Status readout.** The sidebar shows "Online · <clock style>" (or "display
  off"), and "Offline" when the device stops answering. Upstream's shows
  whether the PC companion is online.
- **Diagnostics box.** Upstream's animation-player lines are removed.
- **Wording.**
  - The Clock page offers a clock animation, not "the idle animation shown when
    your PC is asleep", under a "Clock style" heading rather than "Idle clock".
  - The schedule hint describes the backlight, not sparing the LEDs.
  - The display model names the TFT, not "HUB75 Matrix".
  - The factory reset warning lists touch calibration, not metric labels.
- **`/api/info`** reports `model` as `CYD_AnimatedPixelClock`.

---

## Build, logging and versioning

- **Logging.** All `Serial.print` calls are leveled `DBG_*` macros from
  `include/debug.h`, with the level settable at runtime through `/api/debug`.
- **Versioning.** The port restarted at 1.0.0; `UPSTREAM_VERSION` in
  `include/config.h` records the upstream release it came from.

---

## Keeping this file current

- Add, change or remove the entry in the same commit as the change it describes.
- When porting later upstream work, add its commit to the table at the top and
  record here anything adapted rather than copied.
- When comparing against upstream again, update "Upstream last compared":

```bash
gh api 'repos/Keralots/AnimatedPixelClock/commits?per_page=20' --jq '.[] | "\(.sha[0:7]) \(.commit.author.date[0:10]) \(.commit.message | split("\n")[0])"'
```
