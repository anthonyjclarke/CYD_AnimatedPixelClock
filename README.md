# CYD_AnimatedPixelClock

<!-- Update version badge when FIRMWARE_VERSION changes in include/config.h -->
![Version](https://img.shields.io/badge/version-1.3.0--dev-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-orange.svg)
![Board](https://img.shields.io/badge/CYD-2.4%22%20%7C%202.8%22%20%7C%204.0%22-yellow.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)
![Status](https://img.shields.io/badge/status-working%20on%20hardware-brightgreen.svg)

An animated retro-arcade clock — Mario, Space Invaders, Pac-Man, Snake, Tetris,
Asteroids, Dino Runner, Matrix Rain, TRON, Bomberman, Doom Fire and more — running on the
**ESP32 Cheap Yellow Display**. Clock style, sprite colours, brightness
scheduling, timezone and OTA updates are all configured from a built-in web
interface.

This is a port of [AnimatedPixelClock](https://github.com/Keralots/AnimatedPixelClock)
by **Keralots**, which drove two chained 64×64 HUB75 RGB matrix panels from an
ESP32-S3. It re-targets that work to the far cheaper and more accessible CYD,
which needs no wiring, no external panels and no separate 5 V supply.

---

## Origins & Credits

| Layer                          | Author         | Source                                  |
| :----------------------------- | :------------- | :-------------------------------------- |
| AnimatedPixelClock (origin)    | Keralots       | [Keralots/AnimatedPixelClock][upstream] |
| Clock styles, web UI, settings | Keralots       | Ported here substantially intact        |
| ESP32 CYD port                 | Anthony Clarke | [This repository][this-repo]            |

[upstream]: https://github.com/Keralots/AnimatedPixelClock
[this-repo]: https://github.com/anthonyjclarke/CYD_AnimatedPixelClock

Every clock animation, the configuration web interface and the settings model are
Keralots' work, used under the MIT licence and retained in `LICENSE`. This
repository contributes the CYD display layer, the board support, the touch, LDR
and RGB-LED integration, and the layout rework from a 128×64 matrix to the CYD's
larger canvas.

The device credits both as well. The web UI sidebar links to this repository and
reads "Based on AnimatedPixelClock by Keralots" with a link to the original, and
`/api/info` carries the same pair. Improv-Serial and mDNS, which have room for
one name, report `CYD_AnimatedPixelClock`.

Everything this port does differently from the original — hardware, removed
and added features, layout, defaults, settings — is listed in
[`DEVIATIONS.md`](DEVIATIONS.md).

---

## Port status

**v1.1.0 is the initial working release.** It is verified on a 320×240 ILI9341
CYD and a 480×320 ST7796S ESP32-32E: the display path (canvas, scaled blit,
colour order and row-change detection), touch on both wirings, the web UI, WiFi,
NTP and weather. See the [Roadmap](#roadmap) for what is still unverified.

| Area                                    | Status                                     |
| :-------------------------------------- | :----------------------------------------- |
| Board environments, partitions, config  | Complete                                   |
| CYD display layer (`CydDisplay`)        | Verified on 320×240 and 480×320            |
| Touch, own bus and shared bus           | Verified on 320×240 and 480×320            |
| Web UI                                  | In use on hardware; not every page tested  |
| Clock styles, all 15                    | Seen running; not each judged individually |
| RGB status LED                          | Verified on hardware                       |
| LDR auto-brightness                     | Built; not yet verified on hardware        |
| `Serial.print` to `DBG_*` conversion    | Complete                                   |
| Archive of out-of-scope upstream assets | Complete                                   |

Build sizes, all three well inside a 1.792 MB OTA slot:

| Environment    | Flash               | Static RAM       |
| :------------- | :------------------ | :--------------- |
| `esp32-cyd-24` | 1,412,753 B (77.0%) | 71,596 B (21.8%) |
| `esp32-cyd-28` | 1,412,753 B (77.0%) | 71,596 B (21.8%) |
| `esp32-cyd-40` | 1,405,413 B (76.6%) | 81,596 B (24.9%) |

The canvas is allocated from the heap on top of that: 38.4 KB on the 2.4″ and
2.8″, 76.8 KB on the 4.0″.

---

## Hardware

All three CYD variants are ESP32 boards with an integrated TFT and touch
controller. No external wiring is required.

| Spec      | CYD 2.4″ (`esp32-cyd-24`) | CYD 2.8″ (`esp32-cyd-28`) | CYD 4.0″ (`esp32-cyd-40`) |
| :-------- | :------------------------ | :------------------------ | :------------------------ |
| MCU       | ESP32 (ESP32-2432S024)    | ESP32 (ESP32-2432S028R)   | ESP32 (ESP32-32E)         |
| Display   | ILI9341 · 320×240 · SPI   | ILI9341 · 320×240 · SPI   | ST7796S · 480×320 · SPI   |
| Touch     | XPT2046 or CST820 †       | XPT2046, own SPI pins     | XPT2046, display's SPI ‡  |
| Flash     | 4 MB · no PSRAM           | 4 MB · no PSRAM           | 4 MB · no PSRAM           |
| Canvas    | 160×120 @ ×2              | 160×120 @ ×2              | 240×160 @ ×2              |
| SPI freq  | 55 MHz                    | 55 MHz                    | 40 MHz                    |
| Backlight | GPIO 21                   | GPIO 21                   | GPIO 27                   |
| LDR       | GPIO 34                   | GPIO 34                   | not populated             |
| RGB LED   | GPIO 4 / 16 / 17          | GPIO 4 / 16 / 17          | GPIO 22 / 16 / 17         |

† The 2.4″ ships in two revisions. **R** boards fit a resistive XPT2046 and work
as-is. **C** boards fit a capacitive CST820 on I2C, which is not supported —
build with `-DHAS_RESISTIVE_TOUCH=0` so the driver never claims those GPIOs.
Everything except tap-to-change-style works either way.

RGB LED pins are listed red / green / blue; the 4.0″ moves red to GPIO 22.

‡ On the 4.0″ the XPT2046 shares the display's SPI lines, so TFT_eSPI drives it
(`TOUCH_CS=33`); on the 2.4″ and 2.8″ it has dedicated pins and its own driver
on VSPI. Confirmed on an ESP32-32E.

### First flash on a 2.4″

The panel is the same 320×240 as the 2.8″, so the canvas and all fifteen clock
layouts are identical — only the board revision differs. If the display is wrong,
the symptom identifies the cause:

| Symptom                          | Cause                | Fix                                     |
| :------------------------------- | :------------------- | :-------------------------------------- |
| Garbled, shifted or blank        | ST7789, not ILI9341  | `-DST7789_DRIVER=1` in place of ILI9341 |
| Red and blue swapped             | Panel is BGR         | add `-DTFT_RGB_ORDER=TFT_BGR`           |
| Image fine, tapping does nothing | Capacitive "C" board | `-DHAS_RESISTIVE_TOUCH=0`               |
| Backlight always full            | Different BL GPIO    | change `-DTFT_BL=21`                    |

Display SPI pins are identical on all three boards: MOSI 13, SCLK 14, CS 15,
DC 2, MISO 12, no reset line. On the 2.4″ and 2.8″ touch has its own bus (CLK 25,
CS 33, MOSI 32, MISO 39, IRQ 36); on the 4.0″ it shares the display's.

---

## How it renders

Upstream drew into a 128×64 HUB75 canvas. The CYD panels are 320×240 and
480×320 — a different shape as well as a different size, so scaling the old
canvas up would have letterboxed it on all four sides.

Instead the clock styles draw into an off-screen RGB565 canvas whose size is a
build flag, and each logical pixel is painted as a `DISPLAY_SCALE` square block
when the frame is pushed. Canvas × scale equals the panel exactly:

```
esp32-cyd-24    160×120 canvas  ×2  →  320×240 panel
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
width. `DIGIT_TEXT_SIZE` holds that proportion at 75% on every board, so the
4.0″ gets a bigger clock rather than the same small one with more empty space
around it.

**Sprites are magnified, not redrawn.** Mario, the ghosts, the invaders and the
dino are fixed pixel art. `SPRITE_SCALE` expands each drawn pixel into a block at
draw time, so the art keeps its look at any size. It defaults to a third of the
digit text size (×1 on 160×120, ×2 on 240×160), which puts Mario at half to
two-thirds of the digit height. Only Mario uses it so far.

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
with Improv-Serial. Credentials are never stored in source. If a board's AP mode
is faulty, copy `include/secrets.h.example` to `include/secrets.h` (gitignored)
and fill in your network; it is picked up automatically.

---

## Using it

Once the clock is on WiFi it shows its address at boot, and it answers at
`http://pixelclock.local/` on most networks. Everything is configured there:
clock style, the Cycle All rotation, colours, the brightness schedule, timezone
and firmware updates. The timezone defaults to Central European, so set yours
first.

Tap anywhere on the screen to move to the next clock style; the choice is saved.
Hold a finger on the screen for a moment to start the screensaver, and again to
leave it.
Tapping and Cycle All both skip the Weather style until a location is set.

Set the weather location by searching for a city under the Weather style's
settings, then press **Save**; the place name is kept alongside its coordinates.
Weather is only fetched while it can be shown — with the Weather style selected,
or in a Cycle All rotation that includes it — so after a reboot on another style
the Weather screen takes a few seconds to fill in when you switch to it.

A screensaver — Space Invaders, Pac-Man, a starfield or an aquarium — can
replace the clock during set hours, or on demand from the Display page. Tap the
screen while it runs to see the clock for a minute.

A factory reset at `http://pixelclock.local/reset` erases every setting **and**
the WiFi credentials, and the device restarts as the `PixelClock-Setup` access
point.

---

## What this port leaves out

Upstream's PC-statistics mode, audio spectrum visualizer, "This is fine"
screensaver and custom `.pca` animation player are **not** built here. They are preserved
under `archive/` along with the HUB75 hardware assets and the upstream ESP32-S3
release binaries — see [`archive/README.md`](archive/README.md) for what each
folder holds and what reviving it would involve.

---

## Debug output

Everything goes through the leveled macros in `include/debug.h` — there are no
raw `Serial.print` calls. Set the level with `-DDEBUG_LEVEL=n` in the board env,
or at runtime via `POST /api/debug` with `level=n`.

| Level | Shows                                        |
| :---- | :------------------------------------------- |
| 1     | Errors only                                  |
| 2     | Adds warnings: failed fetches, WiFi drops    |
| 3     | Adds info – **the default**; state changes   |
| 4     | Adds verbose: backlight jitter, minute ticks |

At the default level a session looks like this:

```
[INFO] =======================================================
[INFO] CYD_AnimatedPixelClock 1.1.0
[INFO] Board    ESP32 CYD 2.8" (ILI9341)
[INFO] Canvas   160x120 @ x2 -> panel 320x240 (38400 bytes)
[INFO] Sprites  x1, character band 20px, digits 24x32
[INFO] Hardware touch XPT2046 on own VSPI, LDR GPIO34, RGB LED R4/G16/B17
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

- **Enemy sprites are still upstream's crude art.** Goombas, Spinies, Koopas, the
  mushroom and the star are 8×8-ish blocks at the old level of detail, and look
  out of place beside the 12×16 Mario. Idle encounters are what bring them on
  screen. The fix is the same as Mario's: character arrays in `mario_sprites.h`.
- **One 320×240 board has dead touch.** The same firmware works on a second
  board, so it is that unit's hardware: a capacitive "C" revision or a faulty
  XPT2046. The firmware ignores the impossible reads it produces instead of
  turning them into taps.
- **Not every web UI page has been walked through.** Saving settings and
  changing style are confirmed on hardware. The Network, Timezone and
  Maintenance pages, including OTA upload, have not been re-tested since three
  pages and ~300 lines of `PORTAL_JS` were removed during the port.
- **Not every clock style has been judged individually.** Mario, Space
  Invaders, Weather, Bomberman, Matrix Rain and Large have been seen running.
  Doom Fire, ported from upstream after 1.2.0, has only been built so far, as
  have the four screensaver effects.
  Bomberman's corridor spacing and TRON's approach waypoints are the loosest
  layout inferences and the most likely to need adjusting by eye.
- **LDR auto-brightness is unverified.** Its thresholds in `include/config.h`
  were estimated, not measured. The RGB status LED was confirmed on hardware in
  1.2.0; its boot self-test flashes red, green, blue.
- **No touch calibration UI.** Bounds are read from and written to NVS, but
  nothing captures them. Tap-to-change-style needs no accuracy, so this only
  matters if touch grows a real interface.

### Improvements worth making

- **Magnify the other styles' characters.** `SPRITE_SCALE` and the `CydDisplay`
  transform are in place, but only Mario uses them. Pac-Man's *digits* are a
  pellet grid and must not be scaled with him.
- **Scenery for other styles.** The bottom-up layout leaves a sky band above the
  digits in every style, and only Mario fills it.
- **Use the spare canvas in the text styles.** Standard and Large still centre a
  clock with room to spare.
- **Revive an archived subsystem.** The "This is fine" screensaver, the audio
  visualizer and PC-metrics mode are intact under `archive/`, written against the same
  Adafruit-GFX surface. Each would need the layout rework the clock styles had.
- **Tetris small-clock mode by default.** It gives a 25-row well instead of 11,
  a better showcase on this canvas, but it changes behaviour rather than sizing,
  so it stays opt-in.
- **Release binaries.** Tags publish source only. Upstream had a release
  pipeline and a web flasher; both are archived.

---

## Licence

MIT — see [`LICENSE`](LICENSE), which retains Keralots' original copyright
alongside the port's.
