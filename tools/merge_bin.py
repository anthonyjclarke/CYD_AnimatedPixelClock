# PlatformIO post-build script: writes $BUILD_DIR/firmware-merged.bin, one image
# flashed at 0x0 that holds the bootloader, partition table, boot_app0 (otadata)
# and the app - a single-file CLEAN install for `esptool.py write_flash 0x0`.
#
# It ERASES SETTINGS. merge_bin fills the gap between the partition table and
# otadata with 0xFF, and that gap is the NVS partition (0x9000-0xdfff): WiFi,
# settings, touch calibration and the crash report all go. So the web
# installer's manifest does NOT use this file - it lists bootloader,
# partitions, boot_app0 and firmware.bin as separate parts, which leaves NVS
# alone on an update.
#
# boot_app0 is in both on purpose: a device last updated over the air may be
# running from app1, and writing only firmware.bin at 0x10000 leaves otadata
# pointing there, so the old version keeps booting.
#
# Not for the web UI's /update either - that takes firmware.bin. The merged
# image starts with bootloader padding and Update rejects it.
#
# Offsets come from FLASH_EXTRA_IMAGES and ESP32_APP_OFFSET, and flash mode and
# frequency from the builder's own helpers, so the result matches what
# `pio run -t upload` writes. Shared across CYD projects - see
# docs/WEB_INSTALLER_PLAN.md.

Import("env")  # noqa: F821 - provided by PlatformIO

from os.path import join


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    out = join(build_dir, "firmware-merged.bin")
    board = env.BoardConfig()

    images = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        images += [offset, env.subst(image)]
    images += [env.subst("$ESP32_APP_OFFSET"), str(target[0])]

    cmd = [
        '"$PYTHONEXE"', '"$OBJCOPY"',
        "--chip", board.get("build.mcu", "esp32"),
        "merge_bin", "-o", '"%s"' % out,
        "--flash_mode", "${__get_board_flash_mode(__env__)}",
        "--flash_freq", "${__get_board_f_flash(__env__)}",
        "--flash_size", board.get("upload.flash_size", "4MB"),
    ] + ['"%s"' % i if not i.startswith("0x") else i for i in images]

    if env.Execute(env.VerboseAction(" ".join(cmd), "Merging %s" % out)):
        env.Exit(1)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)  # noqa: F821
