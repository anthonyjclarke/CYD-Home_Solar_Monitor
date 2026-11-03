#!/usr/bin/env python3
"""Run portable energy-model checks and preview the actual shared TFT layout."""
from pathlib import Path
import subprocess
import tempfile
import shutil

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="cyd-solar-check-") as directory:
    test = Path(directory) / "energy-tests"
    common = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(root / "include")]
    subprocess.run(common + [str(root / "tests/test_energy.cpp"), "-o", str(test)], check=True)
    subprocess.run([str(test)], check=True)
    renderer = Path(directory) / "preview"
    subprocess.run(common + [str(root / "tools/render_preview.cpp"), str(root / "src/ui/flow_layout.cpp"), "-o", str(renderer)], check=True)
    output = root / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    for scene, name in enumerate(["flow-preview", "export-preview", "discharge-preview", "night-preview", "stale-preview", "setup-preview"]):
        with (output / f"{name}.svg").open("w") as image:
            subprocess.run([str(renderer), str(scene)], stdout=image, check=True)
    print("Rendered six states from the shared TFT drawing code in docs/images/.")
    converter = shutil.which("rsvg-convert")
    if not converter and Path("/opt/homebrew/bin/rsvg-convert").exists():
        converter = "/opt/homebrew/bin/rsvg-convert"
    if converter:
        subprocess.run([converter, "-o", str(output / "flow-preview.png"), str(output / "flow-preview.svg")], check=True)
        print("Rendered the main preview PNG with rsvg-convert.")
