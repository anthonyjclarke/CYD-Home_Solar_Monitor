#!/usr/bin/env python3
"""Loopback-only browser fixture for the embedded page; no HA/device access."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
PAGE = (ROOT / "src/web/web_page.h").read_text().split('R"HTML(', 1)[1].rsplit(')HTML"', 1)[0]
CONFIG = {
    "url": "http://192.168.1.254:8123", "tokenSaved": False, "poll": 5,
    "stale": 180, "brightness": 180, "nightStart": 19, "nightEnd": 7,
    "nightDim": True, "demo": True, "ca": "", "battery": "LG 9.8 kWh",
    "entities": ["sensor.solaredge_i1_ac_power", "sensor.home_power_usage",
                 "sensor.solaredge_i1_m1_ac_power", "sensor.solaredge_i1_b1_dc_power",
                 "sensor.solaredge_i1_b1_state_of_energy", "sensor.solaredge_i1_dc_power"],
}


class Handler(BaseHTTPRequestHandler):
    def send(self, value, content_type="application/json", code=200):
        data = json.dumps(value).encode() if content_type == "application/json" else value.encode()
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
            for i, n in enumerate([463, 1990, 1527, -3904, 28.5, 500, 4404]):
                readings.append({"entity": CONFIG["entities"][i] if i < 6 else None, "value": n,
                                 "derived": i == 6,
                                 "valid": True, "fresh": True, "hasValue": True,
                                 "unit": "%" if i == 4 else "W", "receivedAgeSeconds": 1})
            return self.send({"demo": True, "complete": True, "reportTimeKnown": False,
                              "skewSeconds": 0, "skewLimitSeconds": 10, "battery": CONFIG["battery"], "message": "Browser fixture / synthetic data",
                              "project": "CYD-Home-Solar-Monitor", "version": "0.2.2-dev",
                              "ip": "127.0.0.1", "uptimeSeconds": 120, "freeHeap": 100000,
                              "readings": readings})
        if self.path.startswith("/images/"):
            name = self.path.removeprefix("/images/") if hasattr(str, "removeprefix") else self.path[8:]
            if name not in {"flow-preview.svg", "export-preview.svg", "discharge-preview.svg",
                            "night-preview.svg", "stale-preview.svg", "setup-preview.svg"}:
                return self.send({"error": "Not found"}, code=404)
            return self.send((ROOT / "docs/images" / name).read_text(), "image/svg+xml")
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
