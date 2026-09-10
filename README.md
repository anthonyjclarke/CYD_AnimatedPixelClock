# CYD_AnimatedPixelClock

<!-- Update version badge when FIRMWARE_VERSION changes in include/config.h -->
![Version](https://img.shields.io/badge/version-1.0.0--dev-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange.svg)
![Board](https://img.shields.io/badge/CYD-2.8%22%20%7C%204.0%22-yellow.svg)
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

**Running on a CYD 2.4″.** The display path is confirmed on hardware: canvas,
scaled blit, colour order and row-change detection all work. The clock layouts
are still derived arithmetic that has been seen booting but not yet judged style
by style, and the 2.8″ and 4.0″ targets remain build-only.

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

Build sizes, both environments well inside a 1.792 MB OTA slot:

| Environment      | Flash                | Static RAM          |
|:-----------------|:---------------------|:--------------------|
| `esp32-cyd-28`   | 1,403,473 B (76.5%)  | 71,212 B (21.7%)    |
| `esp32-cyd-40`   | 1,397,645 B (76.2%)  | 81,052 B (24.7%)    |

The canvas is allocated from the heap on top of that: 37.5 KB on the 2.8″,
75 KB on the 4.0″.

---

## Hardware

Both CYD variants are ESP32 boards with an integrated TFT and XPT2046 resistive
touch controller. No external wiring is required.

| Spec       | CYD 2.8″ (`esp32-cyd-28`) | CYD 4.0″ (`esp32-cyd-40`) |
|:-----------|:--------------------------|:--------------------------|
| MCU        | ESP32 (ESP32-2432S028R)   | ESP32 (ESP32-2432S040)    |
| Display    | ILI9341 · 320×240 · SPI   | ILI9341 · 320×240 · SPI   | ST7796S · 480×320 · SPI   |
| Touch      | XPT2046 or CST820 †       | XPT2046 resistive         | XPT2046 resistive         |
| Flash      | 4 MB · no PSRAM           | 4 MB · no PSRAM           | 4 MB · no PSRAM           |
| Canvas     | 160×120 @ ×2              | 160×120 @ ×2              | 240×160 @ ×2              |
| SPI freq   | 55 MHz                    | 55 MHz                    | 27 MHz                    |
| Backlight  | GPIO 21                   | GPIO 21                   | GPIO 27                   |
| LDR        | GPIO 34                   | GPIO 34                   | not populated             |
| RGB LED    | GPIO 4 / 16 / 17          | GPIO 4 / 16 / 17          | not populated             |

† The 2.4″ ships in two revisions. **R** boards fit a resistive XPT2046 and work
as-is. **C** boards fit a capacitive CST820 on I2C, which is not supported —
build with `-DHAS_RESISTIVE_TOUCH=0` so the driver never claims those GPIOs.
Everything except tap-to-change-style works either way.

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
MISO 12, no reset line. Touch sits on its own bus: CLK 25, CS 33, MOSI 32,
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
2.8″ and 91 ms on the 4.0″ — the latter would cap that board near 11 fps.
`CydDisplay::display()` hashes each canvas row and sends only what changed,
which costs 4 bytes per row instead of the 37–75 KB a shadow framebuffer would
need against a ~200 KB free heap.

---

## Building

```bash
pio run -e esp32-cyd-28
```

Upload and monitor:

```bash
pio run -e esp32-cyd-28 -t upload -t monitor
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

## Licence

MIT — see [`LICENSE`](LICENSE), which retains Keralots' original copyright
alongside the port's.
