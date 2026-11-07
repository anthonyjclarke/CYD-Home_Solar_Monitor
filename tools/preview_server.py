#!/usr/bin/env python3
"""Loopback-only browser fixture for the embedded page; no HA/device access."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs
import json
import math
import re
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
# A real 24-hour /api/history capture from the owner's board (7 Nov 2026),
# used for the handbook chart screenshots. Falls back to synthetic data.
HISTORY = ROOT / "tools/fixtures/webui-history.json"
PAGE = (ROOT / "src/web/web_page.h").read_text().split('R"HTML(', 1)[1].rsplit(')HTML"', 1)[0]
VERSION = re.search(r'#define FIRMWARE_VERSION "([^\"]+)"', (ROOT / "include/config.h").read_text())[1]
# Fixed public demonstration date for the status fixture.
EPOCH = int(datetime(2026, 11, 5, 6, 10, tzinfo=timezone.utc).timestamp())
# Make the documentation fixture visibly distinct without changing firmware UI.
PAGE = PAGE.replace("live from Home Assistant", "synthetic documentation fixture")
PAGE = PAGE.replace("live?'Live'", "live?'Preview'")
CONFIG = {
    "url": "http://192.0.2.10:8123", "tokenSaved": False, "poll": 5,
    "stale": 180, "brightness": 180, "debugLevel": 3, "nightStart": 19, "nightEnd": 7,
    "nightDim": True, "nightMode": 2, "nightDimAt": 1140, "nightOffAt": 1380, "nightOnAt": 420, "demo": False, "ca": "", "battery": "LG 9.8 kWh", "layout": 1, "today": "sensor.solar_energy_today", "lat": -33.8679, "lon": 151.2073, "place": "Sydney, New South Wales", "tzName": "Australia/Sydney", "tz": "AEST-10AEDT,M10.1.0,M4.1.0/3", "forecast": "sensor.energy_production_tomorrow",
    "entities": ["sensor.solaredge_i1_ac_power", "sensor.home_power_usage",
                 "sensor.solaredge_i1_m1_ac_power", "sensor.solaredge_i1_b1_dc_power",
                 "sensor.solaredge_i1_b1_state_of_energy", "sensor.solaredge_i1_dc_power"],
}


class Handler(BaseHTTPRequestHandler):
    def send(self, value, content_type="application/json", code=200):
        data = json.dumps(value).encode() if content_type == "application/json" else value if isinstance(value, bytes) else value.encode()
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        route = urlsplit(self.path)
        if route.path == "/":
            page = PAGE
            if parse_qs(route.query).get("theme") == ["dark"]:
                # Fixture-only media override for deterministic visual QA.
                page = page.replace("@media(prefers-color-scheme:dark)", "@media(min-width:0px)")
            return self.send(page, "text/html")
        if route.path in {"/guide", "/guide/"}:
            return self.send((ROOT / "docs/index.html").read_text(), "text/html")
        if route.path.startswith("/guide/images/"):
            name = route.path[len("/guide/images/"):]
            path = ROOT / "docs/images" / name
            types = {".svg": "image/svg+xml", ".png": "image/png", ".webp": "image/webp"}
            if Path(name).name == name and path.suffix in types and path.is_file():
                return self.send(path.read_bytes(), types[path.suffix])
            return self.send({"error": "Not found"}, code=404)
        if self.path == "/api/config":
            return self.send(CONFIG)
        if self.path.startswith("/api/history") and HISTORY.is_file():
            return self.send(json.loads(HISTORY.read_text()))
        if self.path.startswith("/api/history"):
            # Synthetic full-day fixture with an outage and one missing metric.
            epoch = EPOCH
            end_slot = epoch - epoch % 300
            names = ["Inverter AC", "Home load", "Grid (+import)", "Battery (+discharge)",
                     "Battery level", "Inverter DC", "Solar generation", "Solar today", "Solar forecast",
                     "Temperature", "Weather code", "Tomorrow max", "Tomorrow sky code"]
            units = ["W", "W", "W", "W", "%", "W", "W", "kWh", "kWh", "C", "code", "C", "code"]
            samples = []
            for i in range(288):
                slot = end_slot - (287 - i) * 300
                solar = max(0, 4700 * math.sin(i / 288 * math.pi * 2))
                home = 1200 + 600 * math.sin(i / 18)
                battery = -1800 if solar > home else 800
                inverter_ac = solar + battery - 40
                grid = home - inverter_ac
                values = [inverter_ac, home, grid, battery, 60 + 30 * math.sin(i / 100),
                          solar + battery, solar, i / 12, 21, 20 + 4 * math.sin(i / 60), 2, 24, 61]
                if i == 220:
                    values[3] = values[6] = None
                gap = 100 <= i < 125
                samples.append({"slot": slot, "t": None if gap else slot,
                                "sourceTimeKnown": 0 if gap else 127,
                                "v": [None] * len(names) if gap else values})
            return self.send({"schema": 1, "clockKnown": True, "intervalSeconds": 300,
                              "durationSeconds": 86400, "start": epoch - 86400, "end": epoch,
                              "storageReady": True, "storageHealthy": True, "persistedThrough": end_slot - 600,
                              "demoExcluded": False, "timezone": CONFIG["tzName"],
                              "fields": [{"name": n, "unit": u} for n, u in zip(names, units)], "samples": samples})
        if self.path == "/api/status":
            readings = []
            names = ["Inverter AC", "Household", "Grid (+import)", "Battery (+discharge)",
                     "Battery level", "Inverter DC", "Solar generation"]
            # Coherent illustrative AC/DC snapshot, never read from a device.
            for i, n in enumerate([463, 1990, 1527, -3904, 28.5, 500, 4404]):
                readings.append({"entity": CONFIG["entities"][i] if i < 6 else None, "value": n,
                                 "name": names[i], "reportedAgeSeconds": 4,
                                 "derived": i == 6,
                                 "valid": True, "fresh": True, "hasValue": True,
                                 "unit": "%" if i == 4 else "W", "receivedAgeSeconds": 1})
            return self.send({"demo": False, "complete": True, "reportTimeKnown": False,
                              "skewSeconds": 0, "weather": {"configured": True, "place": CONFIG["place"], "valid": True, "fresh": True, "nowC": 23.5, "nowCode": 2, "tomorrowMaxC": 24, "tomorrowCode": 61, "ageSeconds": 60}, "localTime": "2026-11-05 17:10 AEDT", "totals": {"today": {"entity": CONFIG["today"], "value": 25.6, "valid": True, "fresh": True}, "tomorrow": {"entity": CONFIG["forecast"], "value": 21, "valid": True, "fresh": True}}, "skewLimitSeconds": 10, "battery": CONFIG["battery"], "message": "Browser fixture / synthetic data",
                              "project": "CYD-Home-Solar-Monitor", "version": VERSION,
                              "ip": "127.0.0.1", "uptimeSeconds": 120, "freeHeap": 100000,
                              "readings": readings})
        if self.path.startswith("/images/"):
            name = self.path.removeprefix("/images/") if hasattr(str, "removeprefix") else self.path[8:]
            if name not in {"flow-preview.svg", "export-preview.svg", "discharge-preview.svg",
                            "night-preview.svg", "stale-preview.svg", "setup-preview.svg"}:
                return self.send({"error": "Not found"}, code=404)
            return self.send((ROOT / "docs/images" / name).read_text(), "image/svg+xml")
        if self.path.split("?")[0] == "/assets/solar-footer.webp":
            return self.send((ROOT / "web/assets/solar-footer.webp").read_bytes(), "image/webp")
        self.send({"error": "Not found"}, code=404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        if self.path != "/api/config" or self.headers.get("X-CYD-Request") != "1" or length > 12288:
            return self.send({"error": "Fixture accepts settings only"}, code=400)
        try:
            request = json.loads(self.rfile.read(length))
        except ValueError:
            return self.send({"error": "Invalid JSON"}, code=400)
        # The fixture never saves, logs or authenticates with real credentials.
        for key in CONFIG:
            if key != "tokenSaved" and key in request:
                CONFIG[key] = request[key]
        self.send({"saved": True})


if __name__ == "__main__":
    print("Browser fixture: http://127.0.0.1:8765 (no device access)", flush=True)
    HTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
