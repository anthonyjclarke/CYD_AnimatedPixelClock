# Regression checks

Release 2.0.2: all three builds and both host suites passed; Weather loaded on a
4.0″ over HTTP (minimum free heap 86 KB vs 44.6 KB on failed HTTPS). Keep actual
board logs/measurements with future release verification rather than
accumulating phase plans.

## Automated

```sh
pio run -e esp32-cyd-24 -e esp32-cyd-28 -e esp32-cyd-40
python3 tests/host/run_clock_tests.py
python3 tools/make_manifests.py
python3 tests/test_installer.py
```

The host suite compiles production code with graphics/preferences/network/task/
updater stand-ins under ASan/UBSan for both canvases. It covers rotation colours,
settings/palette restoration, date/AM-PM, Matrix decoding through noon/midnight,
Weather variants/forecast bounds, partial-data parsing, fetch scheduling and
stale responses, JSON escaping, origin/method guards and OTA request callbacks.
It needs C++17 and the installed ArduinoJson headers in the 2.8-inch environment.

Installer validation checks version/identity, all board entries, offsets, asset
paths and files, application version strings, and preservation of the NVS region.
These tests do not replace real LCD, flash, TLS, FreeRTOS or USB verification.

## Hardware checklist

Test both canvas sizes; include a 2.4-inch board smoke check when available.
Export settings before updating and capture serial logs, `/api/info` and
`/api/status`. Use private fixture builds for rare weather conditions, restoring
normal source before OTA/release verification.

1. Update from the previous release without erasing WiFi/settings. Verify old
   palettes and backups, including missing/invalid new fields.
2. Compare every clock directly and in rotation; check touch, screensavers,
   notification overlays, dim/off scheduling and WiFi recovery.
3. Check Matrix grid/smooth fall, full/corner clock, all date/transparency/time
   choices, minute/hour/noon/midnight decoding and style changes mid-animation.
4. Check date/detail colours and hidden AM/PM, including Matrix masks and Doom
   outlines. Verify saved selections after reload/reboot/export/import.
5. Check Weather day/night/wind and all precipitation variants, both units,
   negative/large temperatures and all forecast layouts without clipping.
6. Check missing forecasts/hourly data, midnight/timezone labels, location
   changes during fetching, retry/recovery and disabled/ineligible screens.
7. Record TLS minimum heap and stack reclamation over repeated fetches. Run a
   two-hour Weather/Matrix rotation without resets, heap decline or UI stalls.
8. Verify POST controls from UI, scripts and Home Assistant; GET mutations must
   return 405 and foreign/opaque browser origins 403. Check export escaping.
9. Test malformed/empty/aborted/failed OTA requests without restart, followed
   by a successful correct-board OTA and confirmation on both canvas sizes.
10. Test browser installer first install, Update preserving settings/WiFi,
    recognition after OTA, board selection and release-note/version display.

See [API migration](../docs/API_MIGRATION.md) and
[release procedure](../docs/RELEASING.md) for durable contracts.
