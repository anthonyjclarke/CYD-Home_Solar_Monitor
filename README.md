# CYD Home Solar Monitor

A home energy-flow display for the 2.8″ ESP32 "Cheap Yellow Display" (ESP32-2432S028R). It shows solar generation, house demand, grid import/export and battery power and charge as live, animated flows, read from **Home Assistant**. It was built for a SolarEdge SE6000 inverter with an LG battery, and works with any Home Assistant power sensors that follow the same conventions.

Version `0.6.0` · [Install in your browser](https://anthonyjclarke.github.io/CYD-Home_Solar_Monitor/) · [Handbook](docs/index.html) · [Changelog](CHANGELOG.md)

![Centre-flow layout](docs/images/centre-flow-preview.png)

---

## Features

- **Two panel layouts.** Centre flow (light, the default) has the four flows meeting at the inverter, with moving arrows showing direction. Cards (dark) shows the same readings as four cards.
- **Battery at a glance.** A gauge filled to the state of energy turns amber below 50% and red below 20%. The status line says when it's a good time to use power.
- **Optional extras:** today's solar kWh, tomorrow's solar forecast, and weather from Open-Meteo for a location you choose.
- **Browser dashboard** at `http://solar.local/`, covering:
  - live tiles and an animated flow diagram;
  - a 30-minute trend chart;
  - every setting, diagnostics and firmware update.
- **Honest readings.** Stale or missing values grey out and keep their last number instead of dropping to zero, and the status line gives the reason.
- **Quality of life:**
  - a boot screen with the IP address, Wi-Fi signal and Home Assistant host;
  - night dimming;
  - touch calibration;
  - POSIX timezone set from the browser.

The monitor never talks to the inverter. Home Assistant keeps its existing connection (here, SolarEdge Modbus Multi), and the CYD reads entity states from Home Assistant's local REST API every few seconds.

---

## Install

1. Open **[anthonyjclarke.github.io/CYD-Home_Solar_Monitor](https://anthonyjclarke.github.io/CYD-Home_Solar_Monitor/)** in desktop Chrome or Edge.
2. Plug the CYD in with a USB **data** cable, pick the board and click **Connect**.
3. On a new board, choose **Install** and agree to erase. Then use **Configure Wi-Fi** to send your network details over USB.
4. On a board already running this firmware, the page offers **Update** instead, which keeps Wi-Fi, settings and calibration.

Each [release](https://github.com/anthonyjclarke/CYD-Home_Solar_Monitor/releases) also carries `*-firmware.bin`, for the device page's **Firmware update**, and `*-merged.bin`, for a clean USB install, which erases all settings.

Without the browser installer, the panel opens a `CYD-Solar-xxxx` hotspot after 20 seconds with no Wi-Fi. Its password is the device code shown on screen.

---

## Setup

You need a Home Assistant **long-lived access token**: Profile → Security → Long-lived access tokens.

1. Browse to `http://solar.local/`, or tap the panel to see its IP address. There is no login, so keep the device on a trusted home network.
2. Under **Settings → Home Assistant**, enter the Home Assistant URL and the token, then save.
3. Under **Settings → Entities**, check the six power sensors.
4. Optionally:
   - search for your town under **Location & weather**;
   - add the daily-solar and forecast sensors;
   - choose a layout.

| Reading       | Default entity                           | Sign in Home Assistant   |
|:--------------|:-----------------------------------------|:-------------------------|
| Inverter AC   | `sensor.solaredge_i1_ac_power`           | diagnostic only          |
| Household     | `sensor.home_power_usage`                | positive                 |
| Grid          | `sensor.solaredge_i1_m1_ac_power`        | + export, – import       |
| Battery       | `sensor.solaredge_i1_b1_dc_power`        | + charge, – discharge    |
| Battery level | `sensor.solaredge_i1_b1_state_of_energy` | %                        |
| Inverter DC   | `sensor.solaredge_i1_dc_power`           | positive                 |

Solar is calculated as inverter DC less battery discharge, never from inverter AC, following the [SolarEdge Modbus Multi](https://github.com/WillCodeForCats/solaredge-modbus-multi/wiki/Template-Sensors-for-Power-and-Energy#single-inverter-single-battery-single-import--export-meter) single-battery template.

The **[handbook](docs/index.html)** covers the rest in detail:
- every setting;
- an annotated guide to the display and its states;
- how freshness is judged;
- serial diagnostics;
- troubleshooting.

Open it from a local clone or the GitHub file view.

---

## Hardware

An ESP32-2432S028R (CYD 2.8″) with an ILI9341 display in landscape, USB on the right, and an XPT2046 touch controller on its own SPI bus. No other wiring is needed.

---

## Building

Open the folder in PlatformIO (env `cyd28`, `espressif32@6.12.0`, Arduino core 2.0.17, C++17):

```bash
pio run -t upload
```

`python3 tools/check_host.py` runs the host-side model tests and renders the panel previews in `docs/images/`. `python3 tools/preview_server.py` serves the browser dashboard with synthetic data at `http://127.0.0.1:8765`.

No secrets are compiled in: the token and Wi-Fi live in the device's NVS. Release images are built by GitHub Actions on a `v*` tag, using the shared [cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer) workflow. Never publish a local build.

---

## Credits

- The service scaffold and installer tooling come from the author's `CYD_Display_Test` and [cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer).
- The display and touch bus handling come from `CYD_AnimatedPixelClock`.
- Weather is from [Open-Meteo](https://open-meteo.com/).

MIT licensed – see [LICENSE](LICENSE).
