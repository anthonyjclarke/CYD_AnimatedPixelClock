#!/usr/bin/env python3
"""Validate assembled manifests and flash ranges; does not access a device."""
import argparse
import json
import sys
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from make_manifests import config_string, installer_envs  # noqa: E402


def check(site):
    cfg = (ROOT / "include/config.h").read_text()
    version = config_string(cfg, "FIRMWARE_VERSION")
    project = config_string(cfg, "PROJECT_NAME")
    index = json.loads((site / "index.json").read_text())
    assert index["version"] == version and index["project"] == project
    assert {b["env"] for b in index["boards"]} == {b["env"] for b in installer_envs()}
    assert index["releaseNotes"].endswith(
        "/blob/dev/CHANGELOG.md" if "-" in version else "/releases/tag/v" + version)
    for board in index["boards"]:
        url = urlsplit(board["manifest"])
        assert parse_qs(url.query)["v"] == [version]
        manifest = json.loads((site / url.path).read_text())
        assert manifest["name"] == project and manifest["version"] == version
        assert manifest["new_install_prompt_erase"] is True
        assert len(manifest["builds"]) == 1
        build = manifest["builds"][0]
        assert build["chipFamily"] == "ESP32"
        assert {p["offset"] for p in build["parts"]} == {0x1000, 0x8000, 0xE000, 0x10000}
        ranges = []
        for part in build["parts"]:
            path = Path(part["path"])
            assert not path.is_absolute() and ".." not in path.parts
            assert path.parts[:3] == ("firmware", version, board["env"])
            assert "merged" not in path.name
            data = (site / path).read_bytes()
            assert data
            start, end = part["offset"], part["offset"] + len(data)
            assert end <= 0x400000
            assert end <= 0x9000 or start >= 0xE000, "Installer would overwrite NVS"
            ranges.append((start, end))
            if path.name == "firmware.bin":
                assert version.encode() + b"\0" in data, "Stale application image"
        ordered = sorted(ranges)
        assert all(a[1] <= b[0] for a, b in zip(ordered, ordered[1:]))
    assert (site / "index.html").read_bytes() == (ROOT / "installer/index.html").read_bytes()
    print(f"PASS installer {version}: {len(index['boards'])} boards, versioned images, NVS preserved")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, default=ROOT / "_site")
    check(parser.parse_args().site)
