# PlatformIO post-build script: fails the build when firmware.bin leaves less
# than FLASH_MARGIN bytes free in the OTA app slot. The linker's "Flash: x%"
# line under-reports the image, so this checks the actual .bin that OTA and
# the web installer write. The slot size comes from the partition table.
#
# 64 KiB is a team policy margin (audit 7 Nov 2026), not a hardware limit.
# The embedded handbook is the largest single cost; see docs/DEVELOPMENT.md.
Import("env")  # noqa: F821 - provided by PlatformIO

import csv
import os

FLASH_MARGIN = 64 * 1024


def app_slot_size(env):
    path = os.path.join(env["PROJECT_DIR"], env.GetProjectOption("board_build.partitions"))
    with open(path) as f:
        for row in csv.reader(line for line in f if not line.lstrip().startswith("#")):
            cells = [c.strip() for c in row]
            if len(cells) >= 5 and cells[1] == "app" and cells[2] == "ota_0":
                return int(cells[4], 0)
    raise RuntimeError("No ota_0 app partition in " + path)


def check(source, target, env):
    size = os.path.getsize(str(target[0]))
    slot = app_slot_size(env)
    free = slot - size
    print("Flash budget: firmware.bin %d of %d bytes, %d free (minimum %d)" % (size, slot, free, FLASH_MARGIN))
    if free < FLASH_MARGIN:
        print("Flash budget: FAILED - shrink the image or the embedded handbook before shipping")
        os.remove(str(target[0]))  # so the next build re-checks instead of reusing it
        env.Exit(1)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check)  # noqa: F821
