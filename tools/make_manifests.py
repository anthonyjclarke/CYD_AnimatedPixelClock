#!/usr/bin/env python3
"""Assemble the web installer site from finished PlatformIO builds.

Run from the project root after `pio run` has built every env:

    python3 tools/make_manifests.py [--out _site]

For each env in platformio.ini that has `custom_installer_label`, it copies
the flash parts listed in .pio/build/<env>/flash_parts.json (written by
tools/merge_bin.py) and writes an ESP Web Tools manifest. It also writes
index.json, which the installer page and the hub read, and copies
installer/index.html.

The manifest lists the parts separately and never the merged image. The
merged image fills the NVS partition with 0xFF, so an "Update" through it
would erase settings and WiFi. The manifest `name` is PROJECT_NAME, the same
string Improv reports. That match is what makes ESP Web Tools offer "Update"
rather than "Install".

Release checks (tag vs version, no -dev) belong to CI, not here, so the site
can be built and tried locally from a -dev build.
"""

import argparse
import configparser
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_H = ROOT / "include" / "config.h"


def config_string(text, name):
    m = re.search(r'\b%s\s*=\s*"([^"]*)"' % name, text)
    if not m:
        sys.exit("error: %s not found in %s" % (name, CONFIG_H))
    return m.group(1)


def config_define(text, name):
    m = re.search(r'#define\s+%s\s+"([^"]*)"' % name, text)
    return m.group(1) if m else ""


def installer_envs():
    ini = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
    ini.read(ROOT / "platformio.ini", encoding="utf-8")
    envs = []
    for section in ini.sections():
        if section.startswith("env:") and ini.has_option(section, "custom_installer_label"):
            envs.append({
                "env": section[len("env:"):],
                "label": ini.get(section, "custom_installer_label"),
                "hint": ini.get(section, "custom_installer_hint", fallback=""),
            })
    return envs


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default="_site", help="output directory (default _site)")
    out = ROOT / ap.parse_args().out

    cfg = CONFIG_H.read_text(encoding="utf-8")
    project = config_string(cfg, "PROJECT_NAME")
    version = config_string(cfg, "FIRMWARE_VERSION")

    envs = installer_envs()
    if not envs:
        sys.exit("error: no env in platformio.ini has custom_installer_label")

    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    boards = []
    for b in envs:
        build = ROOT / ".pio" / "build" / b["env"]
        parts_file = build / "flash_parts.json"
        if not parts_file.exists():
            sys.exit("error: %s missing - build %s first" % (parts_file, b["env"]))
        dest = out / "firmware" / b["env"]
        dest.mkdir(parents=True)

        parts = []
        for part in json.loads(parts_file.read_text()):
            src = Path(part["path"])
            shutil.copy2(src, dest / src.name)
            parts.append({"path": "firmware/%s/%s" % (b["env"], src.name),
                          "offset": part["offset"]})

        manifest = {
            "name": project,
            "version": version,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32", "parts": parts}],
        }
        name = "manifest-%s.json" % b["env"]
        (out / name).write_text(json.dumps(manifest, indent=2) + "\n")
        boards.append(dict(b, manifest=name))
        print("%-14s %s" % (b["env"], ", ".join("0x%x" % p["offset"] for p in parts)))

    index = {
        "project": project,
        "version": version,
        "repository": config_string(cfg, "PROJECT_REPO_URL"),
        "basedOn": {
            "project": config_string(cfg, "UPSTREAM_PROJECT"),
            "author": config_string(cfg, "UPSTREAM_AUTHOR"),
            "repository": config_string(cfg, "UPSTREAM_REPO_URL"),
        },
        "setupAp": config_define(cfg, "AP_NAME"),  # "" if the project has none
        "boards": boards,
    }
    (out / "index.json").write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n")
    shutil.copy2(ROOT / "installer" / "index.html", out / "index.html")
    print("%s %s -> %s (%d boards)" % (project, version, out.relative_to(ROOT), len(boards)))


if __name__ == "__main__":
    main()
