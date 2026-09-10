# Archive

Assets from the upstream **AnimatedPixelClock** project by
[Keralots](https://github.com/Keralots/AnimatedPixelClock) that this CYD port does
not use. Nothing here is compiled, flashed, or referenced by the firmware — it is
retained for provenance, for reference while porting, and so any subsystem can be
revived without going back to the original repository.

A pristine, unmodified copy of the upstream project is also preserved outside this
repository at `PlatformIO/Projects/AnimatedPixelClock`.

---

## `hub75-hardware/`

Everything specific to the original 2 × 64×64 HUB75 RGB matrix build. The CYD has
an integrated ILI9341 or ST7796S panel and needs none of it.

| Item      | Was                                                          |
|:----------|:-------------------------------------------------------------|
| `bringup/`| HUB75 panel self-test sketch (`hello_matrix.cpp`)            |
| `img/`    | Photographs of the hand-soldered ESP32-S3 / HUB75 prototype  |
| `scripts/`| Node script generating the HUB75 wiring diagram              |

## `upstream-release/`

The upstream release pipeline and its published ESP32-S3 binaries — `release/`
(v2.0.0 – v2.3.0 firmware images and checksums), `release.py` and `result.json`.
These images are built for the ESP32-S3 and **cannot** run on a CYD.

## `upstream-docs/`

| Item                       | Was                                                        |
|:---------------------------|:-----------------------------------------------------------|
| `README.upstream.md`       | Original project README — the reference for feature parity  |
| `docs/`                    | GitHub Pages site, ESP Web Tools flasher, HUB75 wiring guide |
| `.github/`                 | Upstream author's `FUNDING.yml` sponsorship links            |
| `*.code-workspace`         | Upstream VS Code workspace file                             |

The `FUNDING.yml` is archived rather than kept because sponsorship links belong to
the upstream author and should not be served from a fork. Credit is given instead
in the project `README.md` and `LICENSE`.

## `pc-companion/`

The desktop companion application (`PC-Companion-App-v4/`, Windows and Linux) that
pushed live CPU/GPU/RAM/network statistics to the device over UDP, plus
`tools/gif2pca.py`, the GIF-to-`.pca` converter for the custom animation player.
Both belong to subsystems that are out of scope for this port.

## `upstream-src/`

Firmware modules removed from the build, listed with what would be needed to
restore them.

| Module              | Provided                                          | To restore                                                  |
|:--------------------|:--------------------------------------------------|:------------------------------------------------------------|
| `metrics/`          | PC statistics screens, UDP packet decoding         | Re-add UDP listener in `network/`, restore `displayStats()` calls in `main.cpp` |
| `viz/`              | Audio visualizer — oscilloscope, starfield         | Restore `vizShouldDisplay()` branch in the render loop        |
| `ambient/`          | Screensavers and the LittleFS `.pca` player        | Re-add a LittleFS partition and the `ambientActive()` branch  |
| `matrix_display.h`  | HUB75 DMA panel shim (Adafruit\_GFX wrapper)       | Superseded by `src/display/cyd_display.h` — reference only    |

Every one of these was written against the same Adafruit\_GFX call surface the CYD
display layer provides, so a revival is a scope decision rather than a rewrite.
They would, however, need the same 128×64 → 160×120 layout rework applied to the
clock styles.
