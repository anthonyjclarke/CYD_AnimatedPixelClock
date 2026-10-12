# Releases and web installer

`include/config.h::FIRMWARE_VERSION` is the version source. Stable releases are
tagged `vX.Y.Z` on `main`; `dev` uses a `-dev` suffix. Use a major version for
breaking settings/API changes. Release 2.0.0 changes runtime controls to POST.

Before release, run the [regression checks](../tests/README.md), update
`CHANGELOG.md`, `DEVIATIONS.md` and the README badge, then set the stable version.
Commit on `dev`, fast-forward `main`, tag that stable commit and push the
branches/tag. Never force-push or overwrite an existing release tag. After
publication, open the next development version on `dev`.

`.github/workflows/firmware.yml` builds all three boards, runs host tests and
installer validation, publishes release images/checksums, and deploys `_site`
to GitHub Pages on a stable tag. The tag must match the firmware version and be
on `main`. Branch builds create downloadable previews without publishing.

Release images come from CI only: local builds may include `include/secrets.h`.
Never upload local binaries as release assets. Verify the workflow succeeded,
all six board images plus `SHA256SUMS.txt` are present, and the live installer's
`index.json` and three manifests report the new version.

## Installer guarantees

- Keep `PROJECT_NAME` unchanged: Improv and manifest names must match for Update.
- Keep the partition layout unchanged unless an erase/migration is documented.
- Manifests list bootloader, partition table, `boot_app0` and application
  separately, using generated `flash_parts.json` offsets.
- Never put `firmware-merged.bin` in a manifest: its padding overwrites NVS.
  Merged images are for clean serial installs only; `/update` takes the app image.
- Versioned asset directories and manifest query strings avoid old cached images.
- `index.json` drives board labels, version, credits and the release-note link.

Private local preview:

```sh
pio run -e esp32-cyd-24 -e esp32-cyd-28 -e esp32-cyd-40
python3 tools/make_manifests.py
python3 tests/test_installer.py
python3 -m http.server 8765 --directory _site
```

Open `http://localhost:8765` in desktop Chrome/Edge/Opera. `_site` and binaries
are ignored by Git. Delete a local preview after use if it contains secrets.
