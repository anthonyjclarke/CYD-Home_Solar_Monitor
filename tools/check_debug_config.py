#!/usr/bin/env python3
"""Validate actual settings load/save and debug API parsing with host NVS stand-ins."""
from pathlib import Path
import subprocess
import tempfile


def check(root, library):
    source = (root / "src/web/monitor_web.cpp").read_text()
    start = source.index('    if (!d["debugLevel"].isUnbound())')
    stop = source.index('    if (!d["url"].is<String>()', start)
    validation = 'void validateDebug(JsonDocument& d, monitor::Config& c) {\n' + source[start:stop] + '}\n'
    with tempfile.TemporaryDirectory(prefix="cyd-debug-config-") as directory:
        temp = Path(directory)
        (temp / "debug_validation.inc").write_text(validation)
        executable = temp / "debug-config"
        subprocess.run([
            "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-DARDUINOJSON_ENABLE_ARDUINO_STRING=1", "-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0",
            "-DARDUINOJSON_ENABLE_ARDUINO_PRINT=0", "-DARDUINOJSON_ENABLE_PROGMEM=0",
            "-I", str(root / "tests/support"), "-I", str(root / "include"),
            "-I", str(library), "-I", str(temp),
            str(root / "tests/debug_config_harness.cpp"), str(root / "src/config/monitor_config.cpp"),
            "-o", str(executable),
        ], check=True)
        for mode in ("default", "legacy", "invalid-legacy", "off", "legacy-schema", "invalid-saved", "api"):
            subprocess.run([str(executable), mode], check=True)
    print("Debug settings: real config/API validation, legacy migration, off, saved reload and failed-write behavior passed.")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    check(root, root / ".pio/libdeps/cyd28/ArduinoJson/src")
