#!/usr/bin/env python3
"""Run portable energy-model checks and preview the actual shared TFT layout."""
from pathlib import Path
import subprocess
import tempfile
import shutil

root = Path(__file__).resolve().parents[1]
# Uses the actual handler and ArduinoJson String writer once project libraries
# have been installed; portable model checks still run on a clean checkout.
arduinojson = root / ".pio/libdeps/cyd28/ArduinoJson/src/ArduinoJson.h"
if arduinojson.exists():
    from check_history_response import check
    check(root, arduinojson.parent)
    from check_debug_config import check as check_debug_config
    check_debug_config(root, arduinojson.parent)
    from check_night_config import check as check_night_config
    check_night_config(root, arduinojson.parent)
else:
    print("History response integration check needs PlatformIO libraries; run tools/check_history_response.py after pio pkg install -e cyd28.")
with tempfile.TemporaryDirectory(prefix="cyd-solar-check-") as directory:
    test = Path(directory) / "energy-tests"
    common = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(root / "include")]
    subprocess.run(common + [str(root / "tests/test_energy.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    subprocess.run(common + ["-O2", str(root / "tests/test_history.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    subprocess.run(common + [str(root / "tests/test_debug.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    subprocess.run(common + [str(root / "tests/test_boot.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    subprocess.run(common + [str(root / "tests/test_night.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    renderer = Path(directory) / "preview"
    subprocess.run(common + [str(root / "tools/render_preview.cpp"), str(root / "src/ui/flow_layout.cpp"), str(root / "src/ui/history_layout.cpp"), "-o", str(renderer)], check=True)
    output = root / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    # Layout 0 = cards (original names), layout 1 = centre flow (centre- prefix).
    for layout, prefix in enumerate(["", "centre-"]):
        for scene, name in enumerate(["flow-preview", "export-preview", "discharge-preview", "night-preview", "stale-preview", "setup-preview"]):
            with (output / f"{prefix}{name}.svg").open("w") as image:
                subprocess.run([str(renderer), str(scene), str(layout)], stdout=image, check=True)
    with (output / "centre-weather-preview.svg").open("w") as image:
        subprocess.run([str(renderer), "1", "1", "weather-only"], stdout=image, check=True)
    for layout, name in enumerate(["boot-dark-preview", "boot-preview"]):
        with (output / f"{name}.svg").open("w") as image:
            subprocess.run([str(renderer), "boot", str(layout)], stdout=image, check=True)
    for scene in ["waiting", "warning"]:
        with (output / f"boot-{scene}-preview.svg").open("w") as image:
            subprocess.run([str(renderer), "boot", "1", scene], stdout=image, check=True)
    print("Rendered six states of both layouts, a weather-only top row and the boot screen in docs/images/.")
    for scene in ["solar", "battery", "empty", "dark", "weather"]:
        with (output / f"history-{scene}-preview.svg").open("w") as image:
            subprocess.run([str(renderer), "history", scene], stdout=image, check=True)
    converter = shutil.which("rsvg-convert")
    if not converter and Path("/opt/homebrew/bin/rsvg-convert").exists():
        converter = "/opt/homebrew/bin/rsvg-convert"
    if converter:
        for name in ["flow-preview", "centre-flow-preview", "centre-export-preview", "centre-weather-preview", "boot-preview", "boot-dark-preview", "boot-waiting-preview", "boot-warning-preview", "history-solar-preview", "history-battery-preview", "history-empty-preview", "history-dark-preview", "history-weather-preview"]:
            subprocess.run([converter, "-o", str(output / f"{name}.png"), str(output / f"{name}.svg")], check=True)
        print("Rendered preview PNGs with rsvg-convert.")
