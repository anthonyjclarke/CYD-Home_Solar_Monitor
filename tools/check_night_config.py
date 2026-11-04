#!/usr/bin/env python3
"""Check real night settings load/save and optional API fields using host NVS stand-ins."""
from pathlib import Path
import subprocess
import tempfile


def check(root, library):
    source = (root / "src/web/monitor_web.cpp").read_text()
    start = source.index("    // New optional night fields")
    stop = source.index("    const char* totalKeys", start)
    validation = 'void validateNight(JsonDocument& d, monitor::Config& c, int start, int end) {\n' + source[start:stop] + '}\n'
    with tempfile.TemporaryDirectory(prefix="cyd-night-config-") as directory:
        temp = Path(directory)
        (temp / "night_validation.inc").write_text(validation)
        executable = temp / "night-config"
        subprocess.run([
            "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1", "-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0",
            "-DARDUINOJSON_ENABLE_ARDUINO_PRINT=0", "-DARDUINOJSON_ENABLE_PROGMEM=0",
            "-I", str(root / "tests/support"), "-I", str(root / "include"),
            "-I", str(library), "-I", str(temp),
            str(root / "tests/night_config_harness.cpp"), str(root / "src/config/monitor_config.cpp"), "-o", str(executable),
        ], check=True)
        for mode in ("default", "legacy1", "legacy2", "invalid", "api"):
            subprocess.run([str(executable), mode], check=True)
    print("Night settings: real migration, minute precision, saved reload, failed saves and API type/order/legacy validation passed.")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    check(root, root / ".pio/libdeps/cyd28/ArduinoJson/src")
