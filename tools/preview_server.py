#!/usr/bin/env python3
"""Loopback-only browser fixture for the embedded page; no HA/device access."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json
import math
import time

ROOT = Path(__file__).resolve().parents[1]
PAGE = (ROOT / "src/web/web_page.h").read_text().split('R"HTML(', 1)[1].rsplit(')HTML"', 1)[0]
CONFIG = {
    "url": "http://192.168.1.254:8123", "tokenSaved": False, "poll": 5,
    "stale": 180, "brightness": 180, "nightStart": 19, "nightEnd": 7,
    "nightDim": True, "demo": False, "ca": "", "battery": "LG 9.8 kWh", "layout": 1, "today": "sensor.solar_energy_today", "lat": -33.8679, "lon": 151.2073, "place": "Sydney, New South Wales", "tzName": "Australia/Sydney", "tz": "AEST-10AEDT,M10.1.0,M4.1.0/3", "forecast": "sensor.energy_production_tomorrow",
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
        if self.path == "/":
            return self.send(PAGE, "text/html")
        if self.path == "/api/config":
            return self.send(CONFIG)
        if self.path == "/api/status":
            readings = []
            names = ["Inverter AC", "Household", "Grid (+import)", "Battery (+discharge)",
                     "Battery level", "Inverter DC", "Solar generation"]
            # Synthetic values drift slowly so the page's trend chart has shape.
            k = time.time() / 20
            home = 1990 + 400 * math.sin(k)
            grid = 1527 + 900 * math.sin(k / 2)
            battery = -3904 + 600 * math.cos(k / 3)
            solar = home - grid + -battery
            for i, n in enumerate([solar - 40, home, grid, battery, 28.5, solar + battery, solar]):
                readings.append({"entity": CONFIG["entities"][i] if i < 6 else None, "value": n,
                                 "name": names[i], "reportedAgeSeconds": 4,
                                 "derived": i == 6,
                                 "valid": True, "fresh": True, "hasValue": True,
                                 "unit": "%" if i == 4 else "W", "receivedAgeSeconds": 1})
            return self.send({"demo": False, "complete": True, "reportTimeKnown": False,
                              "skewSeconds": 0, "weather": {"configured": True, "place": CONFIG["place"], "valid": True, "fresh": True, "nowC": 23.5, "nowCode": 2, "tomorrowMaxC": 24, "tomorrowCode": 61, "ageSeconds": 60}, "localTime": "2026-11-03 14:32 AEDT", "totals": {"today": {"entity": CONFIG["today"], "value": 25.6, "valid": True, "fresh": True}, "tomorrow": {"entity": CONFIG["forecast"], "value": 21, "valid": True, "fresh": True}}, "skewLimitSeconds": 10, "battery": CONFIG["battery"], "message": "Browser fixture / synthetic data",
                              "project": "CYD-Home-Solar-Monitor", "version": "0.9.0-dev",
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
