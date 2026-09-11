# CYD_AnimatedPixelClock

<!-- Update version badge when FIRMWARE_VERSION changes in include/config.h -->
![Version](https://img.shields.io/badge/version-1.1.0--dev-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange.svg)
![Board](https://img.shields.io/badge/CYD-2.4%22%20%7C%202.8%22%20%7C%204.0%22-yellow.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)
![Status](https://img.shields.io/badge/status-running%20on%20CYD%202.4%22-yellowgreen.svg)

An animated retro-arcade clock — Mario, Space Invaders, Pac-Man, Snake, Tetris,
Asteroids, Dino Runner, Matrix Rain, TRON, Bomberman and more — running on the
**ESP32 Cheap Yellow Display**. Clock style, sprite colours, brightness
scheduling, timezone and OTA updates are all configured from a built-in web
interface.

This is a port of [AnimatedPixelClock](https://github.com/Keralots/AnimatedPixelClock)
by **Keralots**, which drove two chained 64×64 HUB75 RGB matrix panels from an
ESP32-S3. It re-targets that work to the far cheaper and more accessible CYD,
which needs no wiring, no external panels and no separate 5 V supply.

---

## Origins & Credits

| Layer                              | Author          | Source                                                                             |
|:-----------------------------------|:----------------|:-----------------------------------------------------------------------------------|
| AnimatedPixelClock (origin)        | Keralots        | [Keralots/AnimatedPixelClock](https://github.com/Keralots/AnimatedPixelClock)      |
| Clock styles, web UI, settings     | Keralots        | Ported here substantially intact                                                    |
| ESP32 CYD port (display, hardware) | Anthony Clarke  | [anthonyjclarke/CYD_AnimatedPixelClock](https://github.com/anthonyjclarke/CYD_AnimatedPixelClock) |

Every clock animation, the configuration web interface and the settings model are
Keralots' work, used under the MIT licence and retained in `LICENSE`. This
repository contributes the CYD display layer, the board support, the touch, LDR
and RGB-LED integration, and the layout rework from a 128×64 matrix to the CYD's
larger canvas.

---

## Port status

**v1.1.0-dev, on `dev`. Latest release is [v1.0.0](../../releases/tag/v1.0.0), running on a CYD 2.4″.** The display path is confirmed on hardware:
canvas, scaled blit, colour order and row-change detection all work. The clock
layouts are derived arithmetic that has been seen booting but not yet judged
style by style, the 2.8″ env has run on hardware, and the 4.0″ has been flashed but its display fix is not yet confirmed. See the
[Roadmap](#roadmap) for what is unverified and what comes next.

| Area                                       | Status                                        |
|:-------------------------------------------|:----------------------------------------------|
| Board environments, partitions, config      | Complete                                      |
| CYD display layer (`CydDisplay`)            | **Verified on a 2.4″** — colour order included |
| `Settings` / globals restructure            | Complete                                      |
| Web UI trimmed to in-scope features         | Complete — not yet exercised in a browser     |
| `Serial.print` → `DBG_*` conversion         | Complete — 64 call sites                      |
| Touch, LDR, RGB LED modules                 | Complete — not yet verified on hardware       |
| Archive of out-of-scope upstream assets     | Complete                                      |
| Clock-style layout rework, all 14 styles    | Complete — boots, not yet judged style by style |
| Bring-up on 2.8″ / 4.0″                     | Not started                                   |

Build sizes, all three well inside a 1.792 MB OTA slot:

| Environment      | Flash                | Static RAM          |
|:-----------------|:---------------------|:--------------------|
| `esp32-cyd-24`   | 1,406,697 B (76.7%)  | 71,548 B (21.8%)    |
| `esp32-cyd-28`   | 1,406,697 B (76.7%)  | 71,548 B (21.8%)    |
| `esp32-cyd-40`   | 1,400,889 B (76.3%)  | 82,044 B (25.0%)    |

The canvas is allocated from the heap on top of that: 37.5 KB on the 2.4″ and
2.8″, 75 KB on the 4.0″.

---

## Hardware

All three CYD variants are ESP32 boards with an integrated TFT and touch
controller. No external wiring is required.

| Spec       | CYD 2.4″ (`esp32-cyd-24`) | CYD 2.8″ (`esp32-cyd-28`) | CYD 4.0″ (`esp32-cyd-40`) |
|:-----------|:--------------------------|:--------------------------|:--------------------------|
| MCU        | ESP32 (ESP32-2432S024)    | ESP32 (ESP32-2432S028R)   | ESP32 (ESP32-32E)         |
| Display    | ILI9341 · 320×240 · SPI   | ILI9341 · 320×240 · SPI   | ST7796S · 480×320 · SPI   |
| Touch      | XPT2046 or CST820 †       | XPT2046, own SPI pins     | XPT2046, display's SPI ‡  |
| Flash      | 4 MB · no PSRAM           | 4 MB · no PSRAM           | 4 MB · no PSRAM           |
| Canvas     | 160×120 @ ×2              | 160×120 @ ×2              | 240×160 @ ×2              |
| SPI freq   | 55 MHz                    | 55 MHz                    | 40 MHz                    |
| Backlight  | GPIO 21                   | GPIO 21                   | GPIO 27                   |
| LDR        | GPIO 34                   | GPIO 34                   | not populated             |
| RGB LED    | GPIO 4 / 16 / 17          | GPIO 4 / 16 / 17          | not populated             |

† The 2.4″ ships in two revisions. **R** boards fit a resistive XPT2046 and work
as-is. **C** boards fit a capacitive CST820 on I2C, which is not supported —
build with `-DHAS_RESISTIVE_TOUCH=0` so the driver never claims those GPIOs.
Everything except tap-to-change-style works either way.

‡ On the 4.0″ the XPT2046 shares the display's SPI lines, so TFT_eSPI drives it
(`TOUCH_CS=33`); on the 2.4″ and 2.8″ it has dedicated pins and its own driver
on VSPI. The 4.0″ wiring follows AuroraDemo_CYD on the same board and has not
yet been confirmed here.

### First flash on a 2.4″

The panel is the same 320×240 as the 2.8″, so the canvas and all fourteen clock
layouts are identical — only the board revision differs. If the display is wrong,
the symptom identifies the cause:

| Symptom                        | Cause                | Fix                                     |
|:-------------------------------|:---------------------|:-----------------------------------------|
| Garbled, shifted or blank      | ST7789, not ILI9341  | `-DST7789_DRIVER=1` in place of ILI9341  |
| Red and blue swapped           | Panel is BGR         | add `-DTFT_RGB_ORDER=TFT_BGR`            |
| Image fine, tapping does nothing | Capacitive "C" board | `-DHAS_RESISTIVE_TOUCH=0`              |
| Backlight always full          | Different BL GPIO    | change `-DTFT_BL=21`                     |

Display SPI pins are identical on both boards: MOSI 13, SCLK 14, CS 15, DC 2,
MISO 12, no reset line. On the 2.4″ and 2.8″, touch sits on its own bus: CLK 25, CS 33, MOSI 32,
MISO 39, IRQ 36.

---

## How it renders

Upstream drew into a 128×64 HUB75 canvas. The CYD panels are 320×240 and
480×320 — a different shape as well as a different size, so scaling the old
canvas up would have letterboxed it on all four sides.

Instead the clock styles draw into an off-screen RGB565 canvas whose size is a
build flag, and each logical pixel is painted as a `DISPLAY_SCALE` square block
when the frame is pushed. Canvas × scale equals the panel exactly:

```
esp32-cyd-28    160×120 canvas  ×2  →  320×240 panel
esp32-cyd-40    240×160 canvas  ×2  →  480×320 panel
```

That keeps the chunky pixel-art look intact on an LCD while giving the layouts
substantially more room than the original 128×64. Animation code addresses
`SCREEN_WIDTH` / `SCREEN_HEIGHT` only, so a new board is a new environment
rather than a code change.

### Laying out for a canvas, not a panel

Every style positions itself from `src/clocks/clock_layout.h`, which derives its
metrics from the canvas. Two things scale differently, and the split matters:

**Text scales.** Upstream's five digits filled 90 of 128 px — about 70% of the
width. `DIGIT_TEXT_SIZE` holds that proportion at 75% on both boards, so the
4.0″ gets a bigger clock rather than the same small one with more empty space
around it.

**Sprites do not.** Mario, the ghosts, the invaders and the dino are fixed pixel
art. Both boards render at ×2, so a logical pixel is the same physical size on
each — keeping sprites at their logical size keeps them the same physical size
too, and the larger panel simply shows more room around them. Scaling them would
mean redrawing every sprite.

The constraint that fixes the vertical composition is that Mario bounces a digit
by putting his head against its underside, so the gap between the digit row and
the character baseline has to stay exactly one sprite tall. Everything else —
date row, day row, play areas — flows from that.

The extra height goes where it is useful: Tetris' well grows from 5 rows to 11
(25 in small-clock mode), and Snake's flow-field arena from 32×16 cells to
40×30.

**Only changed rows are pushed.** A full-frame push costs about 22 ms on the
2.8″ and 61 ms on the 4.0″ at 40 MHz — the latter would cap that board near 16 fps.
`CydDisplay::display()` hashes each canvas row and sends only what changed,
which costs 4 bytes per row instead of the 37–75 KB a shadow framebuffer would
need against a ~200 KB free heap.

---

## Versioning

`FIRMWARE_VERSION` in [`include/config.h`](include/config.h) is the single source
of truth, surfaced at boot in the serial log, in the web UI header, in
`/api/info`, in the mDNS TXT record and to Improv-Serial — so a device can always
be asked what it is running.

Releases live on `main` and are tagged `vX.Y.Z`. Work happens on `dev`, whose
version carries a `-dev` suffix so a development build is never mistaken for the
release it will become. See the top of [`CHANGELOG.md`](CHANGELOG.md) for the
release procedure.

---

## Building

```bash
pio run -e esp32-cyd-24     # or -28 / -40
```

Upload and monitor:

```bash
pio run -e esp32-cyd-24 -t upload -t monitor
```

WiFi is provisioned through the `PixelClock-Setup` captive portal or over USB
with Improv-Serial. Credentials are never stored in source; see
`include/secrets.h.example` if you need to hardcode them for a board with a
faulty AP mode.

---

## What this port leaves out

Upstream's PC-statistics mode, audio spectrum visualizer, ambient screensavers
and custom `.pca` animation player are **not** built here. They are preserved
under `archive/` along with the HUB75 hardware assets and the upstream ESP32-S3
release binaries — see [`archive/README.md`](archive/README.md) for what each
folder holds and what reviving it would involve.

---

## Debug output

Everything goes through the leveled macros in `include/debug.h` — there are no
raw `Serial.print` calls. Set the level with `-DDEBUG_LEVEL=n` in the board env,
or at runtime via `POST /api/debug` with `level=n`.

| Level | Shows |
|:--|:--|
| 1 | Errors only |
| 2 | + warnings — failed fetches, WiFi drops, rejected input |
| 3 | + info — **the default.** State changes worth knowing about |
| 4 | + verbose — per-tick detail: taps, backlight jitter, minute changes |

At the default level a session looks like this:

```
[INFO] =======================================================
[INFO] CYD_AnimatedPixelClock 1.1.0-dev
[INFO] Board    ESP32 CYD 2.8" (ILI9341)
[INFO] Canvas   160x120 @ x2 -> panel 320x240 (38400 bytes)
[INFO] Sprites  x1, character band 20px, digits 24x32
[INFO] Hardware touch XPT2046 on own VSPI, LDR GPIO34, RGB LED GPIO4/16/17
[INFO] Debug    level 3 (1=err 2=warn 3=info 4=verbose)
[INFO] =======================================================
[INFO] Touch: tap at canvas(88,54) raw(2210,1875) pressure 412
[INFO] Clock style: Mario (0) -> Standard (1) [touch]
[INFO] Cycle All: Standard (1) -> Tetris (8) for 30s
[INFO] Backlight 255 -> 128 (50%)
[INFO] Status: up 02:14:07 | style Tetris (8) | heap 184320 free, 171008 min |
       rows 14/120 | NTP ok
[INFO]         WiFi MyNetwork  192.168.1.42  -58 dBm
```

The status line prints once a minute. `heap ... min` is the low-water mark since
boot, so a slow leak shows there while free heap still looks healthy, and
`rows n/120` is how many canvas rows the last frame actually pushed — a running
measure of whether the display's change detection is earning its keep.

---

## Roadmap

What to work on next, roughly in order of how much it would improve the thing on
a desk. Anything genuinely broken is listed as a bug and comes first.

### Known bugs and unverified areas

| Item | Detail |
|:--|:--|
| Enemy sprites are still upstream's crude art | Goombas, Spinies, Koopas, the mushroom and star are 8×8-ish blocks drawn at the old abstraction level. Beside the 12×16 Mario they look out of place, and enabling idle encounters is what makes them visible. Same fix as Mario: character arrays in `mario_sprites.h`. |
| First board's touch is dead | The same firmware works on a second board, so this is that unit's hardware — a capacitive "C" revision or a faulty XPT2046. Firmware now ignores the impossible reads it produces rather than turning them into taps. |
| Web UI never exercised in a browser | Three whole pages and ~300 lines of `PORTAL_JS` were removed during the port. All 172 placeholder tokens resolve, but that only proves a page compiles, not that it works. |
| Eleven clock styles unjudged | Only Mario and Space have been looked at properly on hardware. The rest boot and their geometry is bounds-checked, but nothing beyond that. Bomberman's corridor spacing and TRON's approach waypoints are the loosest inferences and the most likely to need adjusting by eye. |
| 4.0″ display and touch unconfirmed | First flash failed to allocate the canvas and showed nothing; fixed, but not yet seen on the panel. Touch there moved to the shared-bus backend, also unconfirmed. |
| LDR thresholds are estimates | `LDR_RAW_BRIGHT` / `LDR_RAW_DARK` in `include/config.h` were guessed, not metered. Auto-brightness will track the room but the endpoints may be wrong. |
| Touch calibration UI missing | Bounds are read from and written to NVS, but nothing captures them. Tap-to-change-style needs no accuracy, so this only matters if touch grows a real interface. |

### Improvements worth making

| Item | Detail |
|:--|:--|
| Magnify the other styles' characters | `SPRITE_SCALE` and the `CydDisplay` transform are in place; only Mario uses them. Pac-Man, the invader, the dino, Bomberman's hero and TRON's cycles still draw 1:1. Care needed — Pac-Man's *digits* are a pellet grid and must not scale. |
| Scenery for other styles | The bottom-up layout leaves a sky band above the digits in every style, and only Mario fills it. |
| Use the spare canvas in the text styles | Standard and Large still centre a clock with room to spare. |
| Revive an archived subsystem | Ambient screensavers, the audio visualizer and the PC-metrics mode are intact under `archive/`. Each was written against the same Adafruit-GFX surface, so reviving one is a scope decision plus the same layout rework the clock styles had. |
| Tetris small-clock mode | Now a 25-row well rather than 13. It is a much better showcase on this canvas than the 11-row default, but it is off by default because it changes behaviour rather than sizing. |
| OTA release binaries | `main` is tagged but publishes no artefacts. Upstream had a release pipeline and a web flasher; both are archived. |

---

## Licence

MIT — see [`LICENSE`](LICENSE), which retains Keralots' original copyright
alongside the port's.
