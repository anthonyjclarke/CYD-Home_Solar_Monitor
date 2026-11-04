#!/usr/bin/env python3
"""Parse the actual firmware history response using the pinned ArduinoJson.

Run after PlatformIO has installed project libraries. Only String and the HTTP
transport are host stand-ins; the route and ArduinoJson serializer are real.
"""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile


def check(root, library):
    source = (root / "src/web/monitor_web.cpp").read_text()
    handler = source[source.index("void getHistory() {"):source.index("void uploadChunk() {")]
    with tempfile.TemporaryDirectory(prefix="cyd-history-response-") as directory:
        temp = Path(directory)
        (temp / "history_handler.inc").write_text(handler)
        executable = temp / "history-response"
        subprocess.run([
            "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(root / "tests/support"), "-I", str(root / "include"),
            "-I", str(library), "-I", str(temp),
            str(root / "tests/history_response_harness.cpp"), "-o", str(executable),
        ], check=True)
        for mode in ("gaps", "full", "mixed", "no-clock", "explicit-hours", "bad-hours"):
            body = subprocess.check_output([str(executable), mode], text=True)
            if mode == "bad-hours":
                continue
            response = json.loads(body)
            assert response["schema"] == 1
            assert len(response["fields"]) == 13
            assert response["timezone"] == 'Test/Zone "quoted"'
            samples = response["samples"]
            assert len(samples) == (0 if mode == "no-clock" else 288)
            for i, sample in enumerate(samples):
                assert len(sample["v"]) == 13
                if i:
                    assert sample["slot"] - samples[i-1]["slot"] == 300
                gap = mode in ("gaps", "explicit-hours") or (mode == "mixed" and i % 7 == 0)
                if gap:
                    assert sample["t"] is None and sample["v"] == [None] * 13
                else:
                    assert sample["t"] == sample["slot"]
                    assert sample["v"][2:4] == [-1527.25, 3904]
                    assert (sample["v"][9] is None) == (mode == "mixed")
    print("Firmware history JSON: 288 full/gap/mixed rows, chunk boundaries, escaped metadata, no clock and hours validation passed.")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduinojson", type=Path, default=root / ".pio/libdeps/cyd28/ArduinoJson/src")
    args = parser.parse_args()
    if not (args.arduinojson / "ArduinoJson.h").exists():
        parser.error("ArduinoJson headers missing; run 'pio pkg install -e cyd28' or pass --arduinojson PATH")
    check(root, args.arduinojson)
