# Web Installer Plan – CYD_AnimatedPixelClock (template for all CYD projects)

Goal: anyone with a CYD and Chrome/Edge opens a GitHub Pages URL, picks their
board, clicks **Install**, and ends up on WiFi with the clock running. Users
need no PlatformIO, drivers or esptool. This project is the pilot. Section 4
pulls out the parts that carry over to every other CYD repo.

---

## 1. How the reference installer works

`ootake0914-dotcom.github.io/Baiyosou` is a single static page on GitHub Pages:

- It loads `esp-web-tools@10` from unpkg, which supplies `<esp-web-install-button>`.
- It has one button per hardware variant (ILI9341, ST7789, inverted ILI9341),
  and each button has its own manifest.
- Each manifest has `chipFamily: "ESP32"` and one **merged** `.bin` at offset `0`.
- The binaries are served from the same Pages origin as the page.
- The version is hand-written (`1.0.0`), it has no Improv-Serial, and it never
  asks about erasing.

ESP Web Tools does the flashing, the progress UI, and the "Logs & Console" view.
Our part is the images, the manifest, and the hosting. We'll copy the
mechanism, but our version will improve on it in four ways: CI builds, a version
taken from `config.h`, Improv WiFi setup, and updates that don't erase settings.

I checked these ESP Web Tools 10 behaviours in `install-dialog.js`:

| Behaviour                    | Detail                                  |
|:-----------------------------|:----------------------------------------|
| Improv wait after install    | 10 s (`new_install_improv_wait_time`)   |
| Improv wait on plain Connect | 1.5 s                                   |
| "Update" vs "Install"        | Improv `firmware` == manifest `name`    |
| Update path                  | Same firmware – flashes **without** erase |
| New-install erase prompt     | Only if `new_install_prompt_erase: true` |

---

## 2. Code review – readiness of this project

### Already in place

- **Improv-Serial** (`src/network/improv_setup.cpp`) reports `PROJECT_NAME` and
  `FIRMWARE_VERSION`, and it has the redirect `http://{LOCAL_IPV4}/`. That gives
  the installer's "Configure WiFi" and "Visit device" steps.
- **Builds without secrets.** `config.h` pulls in `secrets.h` through
  `__has_include`, so CI builds a clean image without one.
- **Pinned platform** (`espressif32@6.12.0`), so CI builds are reproducible.
- **Single version source** (`FIRMWARE_VERSION`), with tag discipline already
  written down in `CLAUDE.md`.
- **Standard partitions.** `otadata` is at `0xe000` and app0 is at `0x10000`.
  The firmware is 1.42–1.46 MB in a 1.75 MB slot, which leaves about 370 KB.
- **Serial speed.** The 115200 baud setting matches the ESP Web Tools console.
- **Upstream precedent.** Upstream shipped an ESP Web Tools flasher (archived at
  `archive/upstream-docs/docs/flasher.js` and `archive/upstream-release/release.py`).
  Its board dropdown can be reused. Its merged-image layout can't, because of risk 11.

### Gaps and risks (most serious first)

1. **An install can leave the old firmware running.** Web OTA (`/update`) plus
   the boot-health rollback mean a device in the field may be running from
   `app1`. An installer that writes only `firmware.bin` at `0x10000` leaves
   `otadata` still pointing at `app1`, so the old version keeps booting and the
   "update" appears to do nothing. **Fix:** the installer must also write
   `boot_app0.bin` at `0xe000`, as a separate manifest part (see risk 11).
2. **Improv only runs on fresh devices.** It is armed only when no WiFi is saved.
   On a provisioned device, Connect times out after 1.5 s and
   `_isSameFirmware` is false. ESP Web Tools then offers **Install**, which
   includes the erase prompt, instead of **Update**. Users who say yes wipe the
   `pixelclock`, `cydtouch` and `health` NVS namespaces. **Fix:** keep a
   lightweight Improv responder running in `loop()` (Phase 2).
   *Needs a bench test:* if opening the port resets the CYD through the CP2102
   DTR/RTS circuit, the device won't reach `loop()` within 1.5 s. If so, keep
   first-boot Improv only, and have the page tell users to answer **No** to erase
   when updating.
3. **`improvCustomConnect()` blocks for up to 15 s** with `delay(100)`. That is
   fine inside the setup portal loop, but in `loop()` it would freeze the clock
   and could trip the 15 s task watchdog. If Improv becomes always-on, reset the
   WDT inside that wait, or shorten it.
4. **Never publish locally built binaries.** A local build compiles in your own
   `secrets.h` WiFi credentials. Release images come only from CI, which has no
   `secrets.h`. Add a CI step that fails if `include/secrets.h` exists.
5. **The installer can't choose the board.** All three envs are
   `chipFamily: ESP32`, so ESP Web Tools can't tell them apart. The page must
   make the choice obvious (screen size, number of USB ports, a photo).
   Picking the wrong board gives a blank or garbled screen but won't brick
   anything, and reflashing with the right image fixes it.
6. **The merged image doesn't work over web OTA.** The merged `.bin` starts
   with bootloader padding, so `Update` rejects it with a magic-byte error.
   Name the files clearly: `*-firmware.bin` (installer part and web UI
   `/update`) and `*-merged.bin` (clean install with esptool, erases NVS).
7. **`-dev` builds must never reach Pages.** CI must fail the release if the tag
   ≠ `v` + `FIRMWARE_VERSION`, or if the version contains `-dev`.
8. **The partition table is now frozen.** Updates don't erase, so a later
   partition change needs a release note saying **Erase**, and
   `new_install_prompt_erase: true`.
9. **Serial debug output shares the port with Improv.** Improv framing ignores
   the stray text, so no change is needed. This is listed only so nobody
   "fixes" it.
10. **Browser and driver support.** Only Chrome, Edge and Opera on desktop
    support Web Serial, so Firefox and Safari won't work. Windows may need the
    CP210x or CH340 driver. The page should link to both.
11. **A merged image wipes NVS (found in Phase 1 testing).** `merge_bin` fills
    the gap between the partition table (`0x8000`) and `otadata` (`0xe000`)
    with `0xFF`, and that gap is the NVS partition. Flashing it erases WiFi,
    settings, touch calibration and the crash report. This was confirmed on a
    2.8″ board on 09-10-2026. **Fix:** the manifest lists four separate parts
    and never the merged image. The merged image is kept only as a "clean
    install" release download.

---

## 3. Build plan – this project

### Phase 0 – baseline ✅ 09-10-2026

The health work is committed and pushed to `dev` (`83b7a3c`). All three envs
build at about 79% of the app slot. Everything below happens on `dev`, and the
first live publish is the `v1.4.0` tag on `main`.

One-time GitHub setup, done 09-10-2026:

| Step                                           | Status |
|:-----------------------------------------------|:-------|
| Repo `anthonyjclarke/cyd-web-installer` created | Done   |
| Repo `anthonyjclarke.github.io` (hub) created   | Done   |
| This repo: Pages → Source = GitHub Actions      | Done   |

Pages URL: `https://anthonyjclarke.github.io/CYD_AnimatedPixelClock/`. It
serves nothing until the first release deploy.

### Phase 1 – merged image from PlatformIO (`tools/merge_bin.py`)

**Status 09-10-2026:** the script is in place and wired into all three envs
through `[common].extra_scripts`. In every env the merged image byte-matches
its parts at `0x1000` / `0x8000` / `0xe000` / `0x10000`, and both headers read
DIO, 4 MB, 40 MHz. It was flashed to a 2.8″ (ESP32-D0WD-V3) and booted
1.4.0-dev from `app0` with all peripherals up and Improv listening.

That same test showed the merged image wipes NVS (risk 11). The merged image
is therefore a clean-install download only. The installer uses separate
parts (Phase 3), and none of them needs this script. Still untested: a board
running from `app1`. To test it, do a web OTA, then install the parts and
confirm it boots `app0` with its settings intact.

Add an `extra_scripts = post:tools/merge_bin.py` to `[common]` and every env.
The script hooks a post-action onto `$BUILD_DIR/${PROGNAME}.bin` and runs the
bundled esptool (v4.9.0 is installed) with these arguments:

```
esptool.py --chip esp32 merge_bin -o $BUILD_DIR/firmware-merged.bin
  --flash_mode dio --flash_freq 40m --flash_size 4MB
  <FLASH_EXTRA_IMAGES pairs> $ESP32_APP_OFFSET $BUILD_DIR/firmware.bin
```

`FLASH_EXTRA_IMAGES` already lists the bootloader (`0x1000`), partitions
(`0x8000`) and `boot_app0.bin` (`0xe000`). I checked the builder for 6.12.0, and
it uses the same variable for normal uploads. Take the flash mode, frequency and
size from `env.BoardConfig()` rather than hardcoding them.

Verify the installer path (the separate parts, not the merged image) on a CYD
that was previously web-OTA'd. It must boot the new version and keep its
settings.

### Phase 2 – Improv on every boot

**Status 09-10-2026: done, tested on a 2.8″.** A Python probe that mimics
ESP Web Tools' Connect gets state + device info in about 100 ms in total. A
"Change WiFi" to a network that doesn't exist fails after 15 s without a
watchdog reset, the old network is back 1 s later, and it survives a reset.
Opening the port from pyserial on macOS did not reset the board. Chrome's
Web Serial still needs checking in Phase 3. Testing found a library bug that
dropped every second request. The library is vendored in `lib/ImprovWiFi` with
a one-line fix; 0.0.4 still has the bug. Upstreaming the fix is worth doing.

- Arm Improv right after `Serial.begin()` and pump it from `loop()` on a
  `millis()` cadence. On a provisioned device it only answers *device info* and
  *current state*.
- Keep the 3-minute window only for the first-boot credential flow, as it works
  today.
- Make the connect wait safe for the watchdog (see risk 3).
- Bench test: on a provisioned board, Connect should show **Update
  CYD_AnimatedPixelClock** and the current version.
- I'm not certain how `jnthas/Improv WiFi Library@0.0.2` answers
  `GET_CURRENT_STATE` once WiFi is already up. Read its source before relying on
  it.

### Phase 3 – installer page (`installer/`)

**Status 09-10-2026: built and tested on hardware.**
- The page (`installer/index.html`) and `tools/make_manifests.py` are in
  place, with ESP Web Tools pinned at 10.4.0. The manifests list four parts,
  using the offsets in each build's `flash_parts.json`.
- Served from localhost: all parts load for every board, switching boards
  swaps the manifest, there are no console errors, and the page fits 375 px in
  light and dark mode.
- A 4.0″ (`A4:F0:0F:68:95:5C`) went through the installer in a real browser:
  it was offered **Update**, installed, and came back on WiFi from `app0` with
  no crash recorded. The firmware size matches the 4.0″ build exactly.
- The `app1` case passed on the 2.8″ (`B0:CB:D8:DA:AE:8C`). It was web-OTA'd
  to `app1` (boot health confirmed it `valid` after 60 s), then given an
  **Update** through the installer in Chrome. It came back on `app0` and
  rejoined WiFi with no setup, so the NVS partition, which also holds the
  settings, was kept. Risks 1, 2 and 11 are closed on hardware.
- After an install, the reset reason is 3 (software). That comes from
  ESP Web Tools releasing the chip (esptool-js `after()`), not the firmware:
  an RTS reset gives one `POWERON_RESET` boot and no restart.
- Chrome keeps the port while the installer dialog is open. Close it before
  using a serial monitor.

- `installer/index.html`: a board picker with three cards (2.4″ / 2.8″ / 4.0″),
  each with a photo and ID hints. It loads `esp-web-tools@10.x.y` at a pinned
  exact version. Adapt upstream's `flasher.js`, which rebuilds the button on each
  board switch because ESP Web Tools caches the manifest.
- Generate manifests in CI instead of storing them. Use one per env:

```json
{
  "name": "CYD_AnimatedPixelClock",
  "version": "1.4.0",
  "new_install_prompt_erase": true,
  "builds": [{ "chipFamily": "ESP32",
    "parts": [
      { "path": "bootloader.bin",          "offset": 4096 },
      { "path": "partitions.bin",          "offset": 32768 },
      { "path": "boot_app0.bin",           "offset": 57344 },
      { "path": "esp32-cyd-28-firmware.bin", "offset": 65536 }
    ] }]
}
```

- **Never** use the merged image as a part, because it wipes NVS (risk 11).
  Offsets are decimal in ESP Web Tools manifests. The bootloader, partitions and
  `boot_app0` files are the same for every env, so publish one copy of each.

- `name` **must** equal `PROJECT_NAME`, or updates show up as new installs.
- Add the board label and the photo to each env as PlatformIO custom options
  (`custom_installer_label`, `custom_installer_image`). A new board is then
  still only a new `[env:]` block.

### Phase 4 – GitHub Actions release (`.github/workflows/release.yml`)

| Step | Action                                       |
|:-----|:---------------------------------------------|
| 1    | Trigger: tag `v*` on `main` (+ manual run)   |
| 2    | Fail if `include/secrets.h` exists           |
| 3    | Tag == `v`+`FIRMWARE_VERSION`, no `-dev`     |
| 4    | Cache `~/.platformio`, `pip install platformio` |
| 5    | `pio run` – all three envs                   |
| 6    | `tools/make_manifests.py` → `_site/`         |
| 7    | Copy page + parts (not merged) to `_site/`   |
| 8    | Release: app + merged bins, SHA256SUMS       |
| 9    | `upload-pages-artifact` → `deploy-pages`     |

- Binaries go to Pages as a **deployment artifact**. They are never committed,
  so `.gitignore`'s `*.bin` rule stands.
- Release-asset URLs can't feed ESP Web Tools directly because GitHub release
  downloads carry no CORS headers. Pages serves the bins from the same origin as
  the page.
- One-time repo setting: Settings → Pages → Source = **GitHub Actions**.
- Optional `build.yml` on push or PR to `dev`: steps 2–5 only, with no deploy.

### Phase 5 – hardware test matrix

| Case                                   | Expect                       |
|:---------------------------------------|:-----------------------------|
| Fresh install, erase, each board       | Improv WiFi → clock runs     |
| Update on provisioned device           | "Update", settings kept      |
| Device last updated via web OTA (app1) | New version boots            |
| Wrong board picked                     | Garbled; reflash recovers    |
| `*-firmware.bin` via web UI `/update`  | Updates normally             |
| Windows + macOS, Chrome + Edge         | Port found, flash completes  |

### Phase 6 – documentation

- `README.md`: an **Install** section at the top with the Pages link. Keep the
  PlatformIO route as the developer path.
- `DEVIATIONS.md`: the archived upstream flasher is revived, but built by CI.
  `CLAUDE.md` requires this entry in the same commit.
- `CHANGELOG.md`: an entry under `1.4.0`.
- `CLAUDE.md`: add two "never" rules: never publish local builds, and never
  change partitions without an erase note.

---

## 4. Template for all CYD projects

This is what each repo needs to provide in order to adopt the shared tooling:

| Requirement                         | Why                                   |
|:------------------------------------|:--------------------------------------|
| `FIRMWARE_VERSION` + `PROJECT_NAME` | Manifest version / same-firmware check |
| Builds with no `secrets.h`          | CI can build a shippable image        |
| Pinned `platform =`                 | Reproducible release builds           |
| One `[env:]` per board              | One manifest + button per board       |
| `custom_installer_label` per env    | Board picker text, no HTML edits      |
| Same partition table (4 MB dual-OTA) | One merge recipe fits all            |
| Improv-Serial (recommended)         | WiFi set in the installer dialog      |

The shared pieces should live **once**, in a repo such as
`anthonyjclarke/cyd-web-installer`:

- `merge_bin.py`, the PlatformIO extra script, copied into each project or
  fetched by CI.
- `make_manifests.py`, which reads `pio project config --json-output` and
  `config.h` and then writes the manifests and an `index.json` of boards.
- `installer/index.html` + `flasher.js`, a generic page driven by `index.json`,
  so no project needs its own HTML.
- `.github/workflows/cyd-release.yml`, a **reusable workflow**
  (`on: workflow_call`). Each project's `release.yml` becomes about 10 lines
  that call it with the env list.
- `improv_setup.{h,cpp}`, the Improv module from this project after Phase 2,
  for projects that don't have Improv yet.

Optional extra: a hub page at `anthonyjclarke.github.io` that lists every
project's installer, built from each repo's `index.json`.

Rollout order: pilot here (Phases 1–6), then extract the shared repo, then
adopt it in the next CYD project to prove the template. After that, roll it out
to the rest. Before each project adopts it, audit for local secrets, check the
partition table matches, and check `PROJECT_NAME` is stable. A rename later makes
every installed device look like "new firmware".

---

## 5. Decisions (09-10-2026)

| Question                | Decision                                 |
|:------------------------|:-----------------------------------------|
| Updates and erase       | Always-on Improv (Phase 2)               |
| Shared tooling          | Separate repo + reusable workflow        |
| Installer site(s)       | Hub page listing every project           |

**The hub and the project pages share an origin.** Every project page lives at
`anthonyjclarke.github.io/<repo>/`. The hub repo `anthonyjclarke.github.io`
serves the root of that same origin. The hub can therefore point
`<esp-web-install-button>` straight at each project's manifests without CORS,
and it never stores a binary itself. Each project still deploys its own Pages,
so a release in one repo doesn't need a rebuild of the hub. The hub reads each
project's `index.json` when the page loads.
