# CYD Home Solar Monitor

Version `0.2.2-dev` – a PlatformIO energy-flow display for the 2.8-inch CYD, using the owner's existing SolarEdge Modbus Multi integration in Home Assistant.

The firmware reads Home Assistant at the configurable default `http://192.168.1.254:8123`. It does not connect to the inverter at `192.168.1.88`. Home Assistant keeps its existing Modbus connection.

![Development preview using the supplied sample](docs/images/flow-preview.png)

The preview uses the shared TFT drawing code with approximate desktop font rendering. The values are supplied sample data, not a live hardware capture.

---

## What it displays

The landscape screen shows calculated solar generation, whole-house demand, grid import/export, battery charging/discharging and battery state of energy. The four net flows meet at an inverter hub. Solar and battery are DC readings; household demand and grid are AC readings. Arrows stop when the corresponding value is stale or unavailable.

Solar generation uses inverter DC power plus signed battery DC power, with battery charging positive and discharging negative in the source entities. Negative results are clamped to zero. This follows the single-inverter/battery calculation documented by [SolarEdge Modbus Multi](https://github.com/WillCodeForCats/solaredge-modbus-multi/wiki/Template-Sensors-for-Power-and-Energy#single-inverter-single-battery-single-import--export-meter).

Inverter AC output remains a separate diagnostic reading. It is never added to battery power to calculate solar. Household consumption continues to use the existing Home Assistant helper without adding battery charging again. The preview is synthetic: its `500 W` inverter DC and `3904 W` battery charging produce `4404 W` calculated solar; these values are not a new live site measurement.

Included features:

- Configurable Home Assistant entity mapping, intervals and stale threshold.
- A local browser dashboard, device diagnostics and firmware upload.
- A clearly labelled demo mode cycling import, export, discharge and night samples.
- Persisted settings, touch calibration, brightness and scheduled night dimming.
- Non-blocking initial Wi-Fi provisioning, reconnects and background HA requests.
- The existing CYD Improv-Serial, ArduinoOTA and browser-installer conventions.

Costs, forecasts, historical logging and equipment control are deferred.

---

## Build and flash

Open this folder in PlatformIO. The default environment is `cyd28`:

```sh
pio run
pio run -t upload --upload-port /dev/cu.YOUR_DEVICE
pio device monitor -b 115200 --port /dev/cu.YOUR_DEVICE
```

Replace the example serial port with your actual panel's port. These instructions flash a board; no board has been flashed during development.

The build pins `espressif32@6.12.0` with Arduino core `2.0.17`, uses C++17 and produces:

| Output                                 | Purpose               |
| -------------------------------------- | --------------------- |
| `.pio/build/cyd28/firmware.bin`        | App image / local OTA |
| `.pio/build/cyd28/firmware-merged.bin` | Clean USB install     |
| `.pio/build/cyd28/flash_parts.json`    | Browser-installer map |

The merged image starts at `0x0` and overwrites NVS, including Wi-Fi, settings, the device code and calibration. Use the app image for the browser update page. Keep `partitions_custom.csv` unchanged for subsequent releases.

No credentials or filesystem upload are needed to build. The page and fonts ship inside the app. Release images should be built by CI using the included shared CYD workflow; local builds are for development and bench installation.

---

## First setup

1. Power the CYD. If its saved Wi-Fi cannot connect during the first 20 seconds, it opens the `CYD-Solar-xxxx` hotspot. The password is the 12-character device code shown on the panel.
2. Join that hotspot and open its captive portal, or browse to `192.168.4.1`. Choose the home Wi-Fi and enter its password. The portal times out after three minutes, with bounded connection retries before reopening. Improv-Serial can also provision Wi-Fi over USB.
3. Once connected, tap the flow screen to view the device IP and access code. Open that IP in a browser. Sign in as `admin`, using the displayed code as the password.
4. In **Connection and display**, check the Home Assistant URL and enter a Home Assistant long-lived access token. Leave the prefilled entity IDs unless they have changed. Save settings.
5. Optionally set the **Battery label** shown beside the state of energy (up to 14 characters; default `LG 9.8 kWh`).
6. Readings appear after the next acquisition cycle. For an initial visual check without a token, enable **Demo mode** and save. Turn demo mode off when commissioning live data.

Create the HA token locally under the user's Home Assistant profile/security settings. The [Home Assistant REST API documentation](https://contributors.home-assistant.io/core/internals/rest-api/) describes authentication. Do not paste the token into chat or commit it. A blank token field preserves the saved token; **Clear saved Home Assistant token** removes it.

The firmware makes state-read requests only, although the HA token itself is not inherently limited to reads. The token is stored in device NVS, not returned by the device API and not logged. The default device web interface and local HA URL use HTTP on the trusted LAN. For an HTTPS HA endpoint, paste its trusted PEM CA certificate into the advanced settings; certificate verification is required, with no insecure fallback. HTTPS needs a synchronised clock. Do not forward this device's ports to the internet.

---

## Entity defaults and units

### Solar, inverter and household

`sensor.solaredge_i1_dc_power` is the sixth configured source, providing inverter DC power. The owner confirmed this entity with a `905.5 Watt` sample. The firmware combines each acquired DC value with battery power from the same acquisition cycle; it does not combine that sample with an older screenshot.

`sensor.solaredge_i1_ac_power` provides inverter AC power in W/kW and remains in the browser diagnostics. `sensor.home_power_usage` provides household power in W/kW; it remains the authoritative household helper rather than being recomputed from independently requested readings.

### Grid

`sensor.solaredge_i1_m1_ac_power` provides signed grid power. Its source convention is positive for export and negative for import. The internal/API convention is positive for import and negative for export. The supplied non-negative import/export helpers are not needed by this version; it uses the signed meter directly.

### Battery

`sensor.solaredge_i1_b1_dc_power` provides signed battery DC power. Its source convention is positive for charge and negative for discharge. Internally, positive means discharge and negative means charge. `sensor.solaredge_i1_b1_state_of_energy` provides percentage in `%`.

Power entities must report `W`, `kW` or a supported `Watt`/`Watts` spelling; energy totals such as `kWh`, non-numeric states, out-of-range percentages and unavailable values are rejected. Invalid reads retain previous numbers in grey with unavailable status. They never silently become zero.

---

## Touch, brightness and diagnostics

A short tap opens/closes the device setup page. From that page, hold for two seconds to enter four-corner touch calibration, then release and tap each cross once. Calibration is saved in NVS; a 45-second inactivity timeout exits the process. Implausible raw readings are rejected using the working AnimatedPixelClock pattern.

Night dimming defaults to `19:00–07:00` in `Australia/Sydney`, including daylight saving, once NTP synchronises. After a minute without touch, brightness is reduced to a fifth; touch restores the configured level for another minute. The touch that wakes a dimmed panel is consumed and does not also open the setup page. This version dims rather than switches the panel fully off.

The browser diagnostics show each reading's validity, receive age and upstream reporting time. The firmware uses HA's `last_reported` when available, not `last_changed`. If the upstream report time or device clock is unavailable, it explicitly labels source age unknown; successful HTTP reads alone do not prove fresh inverter measurements. A configurable initial stale threshold of 180 seconds accommodates slower upstream reporting. Timestamp skew across entity reads is also flagged. Solar requires both DC and battery inputs to be valid and fresh. If their report timestamps or receive times differ by more than the greater of 10 seconds or twice the configured read interval, solar is marked unavailable instead of mixing those samples. If one input has an unknown report time, the other input is still checked for stale reporting.

HA requests run on a separate task with bounded connection/read timeouts and failure backoff. Each cycle reads all six entities over one keep-alive connection, which keeps the readings close together in time. Display, touch and Improv processing run independently. The whole snapshot is published after the acquisition cycle, although individual HA entities can still originate from different source times.

### Serial diagnostics

Firmware `0.2.2-dev` prints diagnostics at `115200` baud with uptime timestamps. After installing the app update, open the PlatformIO serial monitor:

```sh
pio device monitor -b 115200
```

`[BOOT]`, `[BOARD]`, `[CFG]` and `[MAP]` identify the firmware, reset reason, hardware and active entity mappings. `[WIFI]` and `[CLOCK]` record connection/provisioning changes and NTP synchronisation.

Each published live poll produces a `[POLL]` summary and six `[READ]` lines with entity ID, HTTP status, request duration, numeric source value/unit, normalised value, validity, freshness, receive age and upstream report time. A negative HTTP code indicates a transport failure, `401` indicates HA authentication failure, and `404` generally indicates an incorrect entity ID. HTTP `0` means no HTTP response was obtained; the error field explains the stage. Arbitrary response strings and bodies are not printed.

`[SOLAR]` shows the DC and signed charging inputs, stored solar value, timestamp skew and rejection reason. If `valid=0`, a numeric field can be a retained previous reading rather than a newly accepted value. `report_pair_known=0` means skew between report timestamps cannot be verified. `[BALANCE]` checks inverter AC plus signed grid import against household demand when all three inputs are fresh; it does not mix battery DC into that AC balance.

`[DATA]` reports changes in validity/freshness masks, in bit order AC, home, grid, battery, SOC, DC and solar. A fully valid/fresh set is `0x7F`. `[HEALTH]` reports free/minimum heap, Wi-Fi signal and clock status every 30 seconds. Live poll blocks follow the configured poll/backoff interval, demo blocks are limited to once per five seconds, and a missing-token warning appears once on entering that mode.

The default debug level is `3` (info), set by `APP_DEFAULT_DEBUG_LEVEL` in `include/config.h`; `0` disables application logs, `1` retains errors and `2` retains errors/warnings. Diagnostic blocks are written from the main loop, not the acquisition task, so they cannot be inserted by that task into an Improv response. HA tokens, Wi-Fi passwords, device access codes, certificates and response bodies are not logged. The configured HA address is restricted to its origin in the log, omitting userinfo, paths and queries.

---

## Hardware reference

The hardware configuration follows the working `CYD_AnimatedPixelClock` 2.8-inch environment: ESP32-2432S028R, ILI9341, landscape rotation `1`, USB to the right, with PWM backlight on GPIO `21`.

| Signal          | GPIO / setting                 |
| --------------- | ------------------------------ |
| TFT SPI bus     | HSPI, explicit `USE_HSPI_PORT` |
| TFT MOSI / MISO | `13` / `12`                    |
| TFT SCLK / CS   | `14` / `15`                    |
| TFT DC / reset  | `2` / `-1`                     |
| Touch SPI bus   | Dedicated VSPI                 |
| Touch CLK/MISO  | `25` / `39`                    |
| Touch MOSI/CS   | `32` / `33`                    |
| Touch IRQ       | `36`                           |

TFT writes run at 55 MHz and touch at 2.5 MHz. TFT_eSPI's shared-bus `TOUCH_CS` setting is intentionally absent; XPT2046 is driven separately. Its resulting compile-time touch warning is expected. The screen uses a 76.8 KB, 8-bit sprite for coherent redraws; runtime heap use also includes network and TLS allocations.

---

## Firmware updates

Use the authenticated browser **Firmware update** section with the app `firmware.bin`. The device pauses HA acquisition while an update is in progress and restarts after a successful upload. ArduinoOTA also uses the device code as its password and becomes available even when Wi-Fi connects after boot.

This version provides manual local updates. Signed automatic downloads and validated rollback are not implemented. Dual app slots alone do not provide those behaviours.

### Updating an existing 0.1.0 device

Upload `.pio/build/cyd28/firmware.bin` through the existing device's firmware update page, then reload the browser page after the restart. Do not upload the merged image or erase the board for this upgrade.

The loader accepts the previous five-entity settings record, preserves the saved URL, token, five entity mappings and display settings, and supplies `sensor.solaredge_i1_dc_power` as the new sixth mapping. The next settings save writes schema 2. The partition layout and project identity are unchanged; actual preservation should be confirmed on the installed panel as part of commissioning.

---

## Verification

Run the portable decoding, sign, timestamp, freshness and rollover checks with:

```sh
python3 tools/check_host.py
```

The same command generates six SVG previews from the actual TFT layout code. Desktop font rendering is approximate; on-panel verification is still required.

For a synthetic browser preview, run:

```sh
python3 tools/preview_server.py
```

Open `http://127.0.0.1:8765`. This loopback fixture does not contact Home Assistant or the inverter and does not accept real credentials. It validates the embedded page's rendering and client behaviour, not the ESP32 HTTP server or hardware.

The PlatformIO build and host checks passed during development. Remaining checks are listed in [HARDWARE_VALIDATION.md](docs/HARDWARE_VALIDATION.md). Live polling, runtime heap behaviour, touch alignment, provisioning and firmware upload need a real panel with locally configured credentials.

---

## Design and reuse

The service scaffold, Improv library, merge script and installer conventions come from `CYD_Display_Test` and `cyd-web-installer`. The display/touch bus split and raw-reading guards follow `CYD_AnimatedPixelClock`. NVS configuration and local dashboard patterns follow the other existing CYD projects. Source projects have not been modified.

See [BUILD_PLAN.md](docs/BUILD_PLAN.md) for equipment details and [RAYHUB_REVIEW_AND_BUILD_BRIEF.md](docs/RAYHUB_REVIEW_AND_BUILD_BRIEF.md) for the original product research.

