# CYD Home Solar Monitor – Rayhub Review and Build Brief

Historical research dated 3 November 2026; external product claims and linked sources were not revalidated in the 5 November documentation review. Current implementation status is in [REVIEW_AND_ROADMAP.md](REVIEW_AND_ROADMAP.md) and [DEVELOPMENT.md](DEVELOPMENT.md).

Reviewed on 3 November 2026. This is a requirements brief and provisional implementation sequence for a PlatformIO project. The initial research preceded confirmation of the installation. The current requirements, confirmed equipment and implementation sequence are in [BUILD_PLAN.md](BUILD_PLAN.md). The project directory was empty when first reviewed.

---

## Feasibility

A CYD 2.8-inch panel is a suitable target for a monitor with the main Rayhub functions. Rayhub specifies the same 320 × 240 resolution for its smallest display. Actual feature coverage depends on which measurements the SolarEdge installation exposes, particularly grid metering and battery data. The public site describes behaviour, not an open firmware implementation or a complete forecasting algorithm. This project would implement its own interface and logic.

The proposed baseline is a local, read-only SolarEdge monitor using the existing CYD scaffold. Live measurements would work without internet access while the local network remains available. Weather and initial time synchronisation would be separate dependencies.

---

## Rayhub product review

### Live dashboard and interaction

The main screen places solar, home, grid and an optional battery around an energy flow diagram. It shows power in kW, flow direction, battery percentage and a short indication of whether spare power is available. Daily solar energy appears above the diagram, with tomorrow's forecast alongside it. The site says readings are normally requested once per second, with slower polling where required by an inverter brand. These are published product claims; no physical Rayhub was tested. See the [main product page](https://rayhub.com.au/).

Tapping the daily total cycles through available solar, grid import/export and cost figures. Tapping the centre opens a forecast progress ring, returning to the flow screen after another tap or a timeout. Battery visibility follows the installation. A Sigenergy installation can additionally expose another inverter's contribution; this is not advertised as a generic SolarEdge capability. See the [product examples](https://rayhub.com.au/).

### Daily totals, prices and free hours

Daily energy totals are shown when the inverter supplies the necessary information. Some grid totals only become available after the first midnight. Import cost and export income require user-entered prices. The configurable free-power window changes the banner and cost treatment. A gap at a tariff boundary can make a daily cost unavailable rather than justify an invented amount. These behaviours make measurement validity and midnight handling part of the design, not merely display formatting. See the [troubleshooting guide](https://rayhub.com.au/help/).

### Forecast

Rayhub requests weather for the configured area and learns a relationship between sunshine forecasts and measured daily generation. Initially it shows weather and temperature, then an estimated energy figure after enough suitable days. Its progress view compares generation with the day's forecast. Its forecast service depends on Rayhub infrastructure even though live readings are local. The public description does not provide enough detail to reproduce the estimator exactly. See the [forecast description](https://rayhub.com.au/#forecast) and [internet dependency FAQ](https://rayhub.com.au/#faq).

For this project, forecasting should be a separate stage. We can evaluate a documented external forecast source or implement an explicitly approximate local estimator after collecting data. Postcode or approximate coordinates are sufficient to begin discussing this; panel size, orientations, tilt and shading would improve a physical model if known.

### Display, setup and maintenance

The smallest Rayhub uses a 320 × 240 colour touchscreen and 2.4 GHz Wi-Fi. Setup is available through the display or a phone browser. It supports light/dark themes, adjustable brightness and scheduled night behaviour. Night choices include dimming, remaining lit, or sleeping with touch to wake. It uses a USB power adapter. See the [specifications](https://rayhub.com.au/specs/).

Rayhub supports touch calibration and distinguishes stale readings from live ones by greying values and stopping flow animation. A loss of inverter communication is different from a positively reported grid outage. See the [help page](https://rayhub.com.au/help/).

Its update system checks automatically, verifies signed releases and can revert an unsuccessful update. These are additional capabilities beyond the ArduinoOTA service currently in the local CYD scaffold. See the [security design](https://rayhub.com.au/security/).

### Connectivity and SolarEdge limits

Rayhub lists 13 inverter brands, with four verified in real homes. SolarEdge is listed as workshop tested, specifically without a SolarEdge battery. The connection guide also identifies the single Modbus connection constraint and the commonly used port `1502`. This is useful compatibility context, not proof that a particular battery installation will work. See the [SolarEdge connection section](https://rayhub.com.au/connect/#solaredge).

SolarEdge's current SunSpec note is version 3.3, October 2026. It documents one Modbus leader and a single TCP session. Modern configuration uses port `1502` by default; older LCD configuration documents `502`. The actual configured endpoint must therefore be recorded. Its current note specifies Ethernet on the inverter side; the CYD can still connect over Wi-Fi through the same LAN. Model, firmware and an existing successful read should resolve installation-specific connectivity. See the [SolarEdge technical note](https://knowledge-center.solaredge.com/sites/kc/files/sunspec-implementation-technical-note.pdf).

### Privacy and optional controls

Rayhub's ordinary live measurements are read locally, but the product also contacts services for time, weather, updates and health reporting. Daily energy sharing is enabled by default and can be disabled. A site check can upload diagnostics with consent. A self-built monitor does not require those reporting services. See the [privacy policy](https://rayhub.com.au/privacy/).

The site describes EV current control through OCPP as being tested, with limited charger support. It is distinct from inverter monitoring. Any EV display or control requirement needs its own charger details and scope. See the [EV section](https://rayhub.com.au/#ev).

---

## Reuse from existing CYD projects

### Primary scaffold

Use `../CYD_Display_Test` as the foundation. Its README explicitly identifies it as the reusable project scaffold. The reviewed configuration supplies the `cyd28` environment, Arduino framework, pinned `espressif32@6.12.0`, `TFT_eSPI`, dedicated `XPT2046_Touchscreen` touch bus, board profiles, backlight service, NVS settings, logging, WiFiManager, ArduinoOTA and vendored Improv provisioning.

Retain the shared installer conventions: `PROJECT_NAME`, `FIRMWARE_VERSION`, the existing dual-OTA partition layout, `tools/merge_bin.py`, app and merged firmware outputs, and builds that work without `secrets.h`. Use `CYD-Home-Solar-Monitor` as the proposed stable identity before the first release.

### Supporting patterns

| Existing project    | Pattern to reuse                   |
| ------------------- | ---------------------------------- |
| `CYD_Display_Test`  | Hardware and service foundation    |
| `CYD_WordClock`     | NVS settings, brightness and LDR   |
| `CYD_PongClock`     | Local status/config API patterns   |
| `CYD_BusStop_NSW`   | Separated data, display and web UI |
| `TheFlightWall_CYD` | Live web configuration/dashboard   |
| `cyd-web-installer` | Shared release and installer flow  |

Existing CYD project configurations and selected source files were inspected; this is not a complete audit of every historical project. Use compatible shared patterns rather than combine every project's dependencies. The existing scaffold already supports a compact `TFT_eSPI` application, so a move to LVGL is not currently justified.

### Adjustments identified before reuse

The scaffold enables ArduinoOTA only when Wi-Fi is connected during `begin()`. Add handling for a later connection. Its optional credentials connection and portal flow can occupy startup; preserve Improv responsiveness and make the monitor recover without blocking touch or rendering. Replace the scaffold's RGB colour cycle with deliberate status behaviour.

Calibrate touch on the actual panel and persist it. Preserve the known display setup until the board variant is confirmed. The scaffold's `cyd28` profile uses ILI9341; the product name alone is insufficient to identify every CYD revision.

The shared installer can overwrite a shipped filesystem image during updates. Keep settings and compact energy checkpoints in NVS, and define a separate retention strategy before adding history. Do not write every live sample to flash. Signed automatic downloads and tested rollback would require additional implementation; two OTA slots alone do not supply those behaviours.

---

## Current installation and implementation plan

The owner has confirmed one SolarEdge SE6000, a three-phase household supply, Ethernet Modbus TCP at `192.168.1.88`, working Home Assistant monitoring, home/grid readings, an LG battery labelled 9.8 kWh and a landscape CYD. Version 1 prioritises live energy flow; costs are deferred.

The owner subsequently confirmed SolarEdge Modbus Multi and selected Home Assistant readings for version 1. See [BUILD_PLAN.md](BUILD_PLAN.md) for the confirmed firmware versions, selected data route, six source entities plus derived solar, confirmed entity mapping and working hardware reference and build acceptance criteria. Direct inverter access has not been attempted.

The owner has now supplied Home Assistant at `192.168.1.254`, the power and battery entity mappings, and `CYD_AnimatedPixelClock` as the working hardware reference. The initial supplied generation entity was inverter AC output. The owner subsequently confirmed `sensor.solaredge_i1_dc_power`; firmware `0.2.0-dev` now uses that DC entity and signed battery DC power to calculate solar generation, retaining AC output in diagnostics.
