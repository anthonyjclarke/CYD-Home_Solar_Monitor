#!/usr/bin/env python3
"""Check local documentation targets, handbook anchors and preview dimensions."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ERRORS = []


class Page(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.links = []
        self.ids = set()
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.add(attrs["id"])
        for name in ("src", "href"):
            if name in attrs:
                self.links.append(attrs[name])


def fail(path, message):
    ERRORS.append(f"{path.relative_to(ROOT)}: {message}")


def check_target(path, target):
    target = target.strip("<>")
    url = urlsplit(target)
    if url.scheme or url.netloc or target.startswith("/"):
        return
    resolved = (path.parent / unquote(url.path)).resolve() if url.path else path
    if not resolved.exists():
        fail(path, f"missing target {target}")
    elif url.fragment and resolved.suffix == ".html":
        if unquote(url.fragment) not in Page(resolved.read_text()).ids:
            fail(path, f"missing HTML anchor {target}")


def main():
    documents = [ROOT / "README.md", ROOT / "AGENTS.md", ROOT / "CLAUDE.md",
                 ROOT / "CHANGELOG.md", *sorted((ROOT / "docs").glob("*.md"))]
    for path in documents:
        text = path.read_text()
        # Ignore code fences when validating prose links/formatting.
        prose = re.sub(r"^```.*?^```[^\n]*", "", text, flags=re.M | re.S)
        for target in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", prose):
            check_target(path, target)
        previous_heading = 0
        for number, line in enumerate(prose.splitlines(), 1):
            if "\t" in line:
                fail(path, f"tab in prose line {number}")
            heading = re.match(r"^(#{1,6}) ", line)
            if heading:
                level = len(heading[1])
                if level > previous_heading + 1:
                    fail(path, f"heading level skipped near {line}")
                previous_heading = level
            if line.startswith("|"):
                if any(len(cell.strip()) > 45 for cell in line.strip("|").split("|")):
                    fail(path, f"table cell exceeds 45 characters: {line}")
    handbook = ROOT / "docs/index.html"
    page = Page(handbook.read_text())
    for target in page.links:
        check_target(handbook, target)
    images = sorted((ROOT / "docs/images").glob("*.svg"))
    for path in images:
        svg = ET.fromstring(path.read_text())
        if svg.get("viewBox") != "0 0 320 240":
            fail(path, "panel preview must have a 320 x 240 viewBox")
    version = re.search(r'#define FIRMWARE_VERSION "([^"]+)"',
                        (ROOT / "include/config.h").read_text())[1]
    for path in [ROOT / "README.md", handbook, ROOT / "CHANGELOG.md"]:
        if version not in path.read_text():
            fail(path, f"current version {version} absent")
    for path in sorted((ROOT / "docs/images").glob("boot-*.svg")):
        if f"v{version}" not in path.read_text():
            fail(path, "boot image version differs from firmware; regenerate previews")
    if ERRORS:
        print("\n".join(ERRORS), file=sys.stderr)
        return 1
    print(f"Documentation: {len(documents)} Markdown files, handbook local links/anchors, "
          f"{len(images)} panel SVGs and release identity passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
