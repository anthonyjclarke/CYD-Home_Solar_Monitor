# CYD Home Solar Monitor – Version 1 Build Plan

Historical plan dated 3 November 2026, annotated on 5 November 2026. The implementation has since reached source `0.9.0`; use [DEVELOPMENT.md](DEVELOPMENT.md), [AGENTS.md](../AGENTS.md) and [REVIEW_AND_ROADMAP.md](REVIEW_AND_ROADMAP.md) for current behavior. Equipment and sample readings below are historical installation context, not newly verified live data.

Originally updated from the owner's answers and two SolarEdge equipment screenshots. The first firmware implementation is now built and locally verified; see [README.md](../README.md) for setup and [HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md) for remaining commissioning checks. The accompanying [Rayhub review](RAYHUB_REVIEW_AND_BUILD_BRIEF.md) records the product research and template assessment.

---

## Confirmed installation

| Item                 | Confirmed detail                      |
| -------------------- | ------------------------------------- |
| Inverter             | SolarEdge SE6000, one inverter        |
| Household supply     | Three phase                           |
| Inverter connection  | Ethernet, local Modbus TCP            |
| Inverter IP          | `192.168.1.88`                        |
| DSP1                 | `1.210.1662`                          |
| DSP2                 | `2.52.629`                            |
| DSP3                 | Reported as not applicable            |
| Communications board | `3.2727.0`                            |
| Existing monitoring  | Home Assistant working well           |
| HA integration       | SolarEdge Modbus Multi                |
| Selected data route  | Home Assistant readings               |
| Home Assistant IP    | `192.168.1.254`                       |
| Metering             | Home/grid readings confirmed by owner |
| Meter count          | One shown in equipment screenshot     |
| Battery              | LG, labelled 9.8 kWh                  |
| Battery firmware     | `DCDC 7.5.6 BMS 1.9.6.0`              |
| Target display       | CYD 2.8-inch, landscape               |
| Working reference    | `CYD_AnimatedPixelClock`              |
| Initial priority     | Live energy flow                      |

Equipment serial numbers are unnecessary for implementation and are not reproduced here. The three-phase household supply does not establish the inverter's phase configuration. The battery's exact product variant and coupling arrangement remain unconfirmed. The portal's backup-ready label is not evidence that outage detection is available as a usable measurement.

---

## Version 1 outcome

Build a PlatformIO/Arduino application for a 320 × 240 landscape CYD. The current screen shows calculated solar generation, household demand, grid import/export and battery charge/discharge, with battery percentage. Inverter AC output remains in diagnostics. Version `0.2.0-dev` adds the confirmed inverter DC input to calculate solar from DC measurements. Label the grid direction explicitly and animate arrows only when the corresponding measurement is valid and above the fixed 30 W idle threshold.

The first milestone covers live power flow. The original plan deferred costs, tariffs, free-power schedules, forecasts, historical graphs and equipment control. Optional HA forecast/daily-total entities, Open-Meteo weather and persistent device/browser 24-hour history have since been implemented. Costs and equipment control remain deferred. Normal framework services still include Wi-Fi setup, persisted configuration, touch, brightness, diagnostics and local firmware updates.

The display must distinguish zero power, unavailable measurements, initial connection, stale source data and loss of network connection. Losing communication must not produce an off-grid claim. The current flow-screen History button opens stored trends; holding longer than two seconds opens device/setup information and the device code. Detailed source ages, units and signs are available in browser/serial diagnostics, not on that panel page.

---

## Data source decision

### Selected implementation – Home Assistant readings

The owner selected Home Assistant readings for version 1 and confirmed the existing integration is SolarEdge Modbus Multi. Keep that integration's inverter connection in place. Have the CYD obtain selected readings from Home Assistant through its local REST API. This avoids adding a broker solely for this project. It also gives us the same entity values already used by the owner's working monitoring setup.

The selected path is `SE6000 → SolarEdge Modbus Multi → Home Assistant local API → CYD`. The CYD will not open a Modbus session to `192.168.1.88` for version 1.

Home Assistant is at `192.168.1.254`. Use `http://192.168.1.254:8123` as an editable initial URL, assuming its standard port and direct local HTTP access; the owner supplied the address, not a verified scheme/port. No request to this host has been made. Configure authentication locally during setup; it is unnecessary for compiling the project or reviewing the plan.

Home Assistant provides authenticated state reads for individual entities. Configure the instance URL, entity mapping and access token locally; do not place the token in source control or logs. The API token is not inherently restricted to read-only operations, even though this firmware would make read requests only. Use an appropriately limited account where supported. See the [Home Assistant REST API](https://contributors.home-assistant.io/core/internals/rest-api/).

This route depends on Home Assistant being available. SolarEdge Modbus Multi is the confirmed upstream integration; its actual reporting cadence and entity availability still need to be recorded. Polling Home Assistant faster cannot improve the inverter integration's measurement cadence. A broker or direct Modbus adapter is not required for the selected version 1 route.

### Independent operation – deferred alternative

If independence from Home Assistant is required, settle connection sharing before connecting the CYD directly. SolarEdge documents one TCP connection/session. Confirm Home Assistant's integration, configured port and unit ID; an existing shared Modbus proxy could serve both clients, while a new proxy would itself be a dependency. Do not stop Home Assistant, change inverter settings or open a competing inverter session merely to discover registers. See the [SolarEdge SunSpec note](https://knowledge-center.solaredge.com/sites/kc/files/sunspec-implementation-technical-note.pdf).

Port `502` or `1502` and unit ID `1` must not be assumed to match this installation. They are only needed for a direct connection or proxy configuration, not for a Home Assistant entity reader.

---

## Confirmed entity mapping

The owner supplied the following entity IDs, units, signs and sample states. They are reference data, not live readings fetched by this project. The implemented reader polls six source entities and derives a seventh solar value. The grid helpers are documented as possible future alternatives rather than requested redundantly every cycle.

### Primary readings

**Inverter AC output:** `sensor.solaredge_i1_ac_power`, in W. Supplied sample: `463`. Retain this as the `Inverter AC` diagnostic; it is not panel generation.

**Household consumption:** `sensor.home_power_usage`, in W. Supplied sample: `1990`. The owner describes its formula as inverter AC power minus signed grid-meter power. Read the established helper directly; use the formula to check consistency rather than silently replace it with asynchronously sampled values.

**Signed grid power:** `sensor.solaredge_i1_m1_ac_power`, in W. Positive means export; negative means import. Sample: `-1527`, displayed as `Import 1.527 kW`. Convert to the internal convention using `grid_import_signed_w = -raw_grid_w`.

**Signed battery DC power:** `sensor.solaredge_i1_b1_dc_power`, in W. Positive means charging; negative means discharging. Sample: `3904`, displayed as `Charging 3.904 kW`. Convert using `battery_discharge_signed_w = -raw_battery_w`. The owner reports `B_STATUS_CHARGE` for this sample. No battery-status entity ID was supplied, so this status is a sample cross-check rather than a configured dependency.

**Battery state of energy:** `sensor.solaredge_i1_b1_state_of_energy`, in %. Sample: `28.5`, displayed as `28.5%`. Do not infer this value from power or nominal capacity.

### Solar generation

The owner subsequently confirmed `sensor.solaredge_i1_dc_power`, with a new sample of `905.5 Watt`. This is the sixth source, separate from inverter AC. Calculate solar using `max(inverter_dc_w + raw_battery_charge_signed_w, 0)`. Internally battery is positive for discharge, so this becomes `max(inverter_dc_w - battery_discharge_signed_w, 0)`.

The same calculation and dependency checks feed the panel and browser. Both inputs must be valid, fresh and sufficiently close in time. No old screenshot value is combined with the newly supplied DC sample. Version `0.2.0-dev` preserves legacy five-entity settings and appends the new DC default. See the [integration's documented calculation](https://github.com/WillCodeForCats/solaredge-modbus-multi/wiki/Template-Sensors-for-Power-and-Energy#single-inverter-single-battery-single-import--export-meter).

### Alternative grid helpers

`sensor.grid_importing_now` supplies non-negative import power in W; the sample is `1527`. `sensor.grid_exporting_now` supplies non-negative export power in W; the sample is `0`. A future helper-pair adapter could convert using `grid_import_signed_w = import_w - export_w`, validating both readings and timestamps. Version 1 uses the signed raw meter; its single grid entity field does not accept an import/export pair.

### Power boundaries and sample verification

The sample satisfies the household helper: `463 - (-1527) = 1990 W`. On the AC side, inverter output `0.463 kW` plus grid import `1.527 kW` supplies household consumption `1.990 kW`. Battery charging is reported separately on the inverter's DC side. Do not add its `3.904 kW` again to the supplied household helper or treat it as additional household AC demand.

Adding inverter AC output and battery charging yields a rough mixed-boundary number and is not used by the firmware. The confirmed inverter DC entity now supplies the DC side of the documented solar calculation. Conversion losses and sampling times can still prevent an exact match with the SolarEdge display.

The grid value must represent the whole property's three phases, even if the inverter is single phase. Confirm that the meter measures the relevant connection point. If phase sensors are used, sum signed real power with the documented convention; do not sum absolute phase values. Avoid presenting one phase as the whole household.

Do not assume inverter AC power equals solar generation with a battery present. Prefer the established PV and household sensors used by the working dashboard. A household value derived from grid and inverter AC power may be appropriate once the topology and meter location are confirmed. Mixing battery DC readings into an AC balance without accounting for conversion losses can create misleading values.

Internally normalise power to W, grid power positive for import and battery power positive for discharge. Keep each value's validity, source and reporting time. Validate the conventions against an exporting period and a battery charging/discharging period. Do not use the displayed 9.8 kWh label to infer percentage; read state of charge from its source.

Distinguish the time the CYD retrieved an entity from the time the upstream integration reported it. An unchanged sensor value is not automatically stale. Select freshness checks using the integration's reporting behaviour and health signals; `last_changed` alone is insufficient.

---

## Reuse and project structure

Use `../CYD_Display_Test` as the service foundation and the owner's working `../CYD_AnimatedPixelClock` as the hardware reference. Its `esp32-cyd-28` environment uses ILI9341 at 320 × 240 landscape, native geometry 240 × 320, rotation `1` with USB on the right, backlight GPIO `21` and pinned `espressif32@6.12.0`. Retain the standard dual-OTA layout, vendored Improv library and shared installer metadata.

Carry over the working reference's explicit `USE_HSPI_PORT`: display pins are MOSI `13`, MISO `12`, SCLK `14`, CS `15`, DC `2` and reset `-1`, at 55 MHz writes. Touch uses its own VSPI bus on CLK `25`, MISO `39`, MOSI `32`, CS `33` and IRQ `36`, at 2.5 MHz. Do not enable TFT_eSPI's shared-bus `TOUCH_CS` path on this target. Reuse the reference's touch plausibility checks and calibration behaviour, adapting coordinates to this application's full-resolution UI rather than the clock's half-resolution pixel-art canvas. Verify calibration and display stability on hardware after building.

Reuse compatible NVS/brightness patterns from `../CYD_WordClock` and local configuration/status patterns from `../CYD_PongClock` and `../TheFlightWall_CYD`. Build without local credentials. Fix late Wi-Fi OTA activation and replace scaffold demo behaviours as part of the application adaptation.

| Implemented path                | Responsibility                 |
| ------------------------------- | ------------------------------ |
| `platformio.ini`                | Board and dependency settings  |
| `include/config.h`              | Identity and hardware defaults |
| `include/energy_model.h`        | Units, validity and data age   |
| `src/data/ha_source.cpp`        | Selected entity acquisition    |
| `src/config/monitor_config.cpp` | Persisted mapping and settings |
| `src/ui/flow_layout.cpp`        | Flow diagram and touch status  |
| `src/web/monitor_web.cpp`       | Local setup and diagnostics    |
| `src/app/app.cpp`               | Acquisition/UI orchestration   |
| `tools/merge_bin.py`            | Shared installer build output  |

Keep the acquisition interface separate from the energy model and renderer so a direct Modbus adapter can be added later without replacing the screen. Include a demo data mode to develop and review the screen before live configuration is available.

Use bounded requests and reconnect backoff. Target a configurable initial acquisition cycle around five seconds, adjusted to the confirmed upstream cadence. Fetch only mapped entities and avoid loading the entire Home Assistant state registry on every cycle. Keep a coherent snapshot where possible and flag excessive timestamp skew between individual readings. Rendering, touch and Improv must remain responsive during request timeouts.

---

## Build stages and acceptance

### Stage 1 – Entity and hardware mapping

The integration, data route, entity IDs, units, signs and working hardware reference are now supplied. The AC arithmetic has been checked against the owner's sample. This planning stage is complete for the inverter/home/grid/battery display. The inverter DC input has also been confirmed and solar calculation is implemented. Record upstream reporting cadence during live commissioning and verify meter coverage against the existing dashboard.

### Stage 2 – Compile and review the CYD dashboard

Adapt the existing scaffold and implement the flow screen using demo snapshots. Compile with PlatformIO and check the firmware fits the existing OTA app slot. Exercise import/export, battery charge/discharge, zero generation, missing battery data and disconnected states. Check layout and touch on the actual panel; a successful build alone does not validate hardware.

### Stage 3 – Connect the selected live source

Add local source configuration and the Home Assistant adapter. Validate numerical conversion, unavailable responses, authentication failures, request timeouts and reconnects. Compare all five displayed measurements with Home Assistant, accounting for acquisition timing, and verify that Home Assistant's inverter monitoring continues normally.

### Stage 4 – Hardware verification and installer preparation

Verify night operation, Wi-Fi loss, Home Assistant restart, stale upstream data, CYD restart and settings persistence. Test late Wi-Fi connection and local OTA. Verify a fresh browser installation and a subsequent settings-preserving update when a board is available. Prepare credential-free build outputs and the established CYD CI configuration; remote publishing requires a separate instruction.

At the original planning milestone, firmware compilation, portable checks and a synthetic browser fixture had passed without live reads, flashing or publishing. Since then, the project records a running panel with live HA data and releases through `0.8.0`; see `HARDWARE_VALIDATION.md` and `WEB_INSTALLER.md` for the actual observations and unchecked cases. The 0.9.0 source-release sweep performs local checks only; see [RELEASE_0.9.0.md](RELEASE_0.9.0.md).

---

## Implementation readiness

The first-version plan has sufficient information to implement the selected inverter/home/grid/battery flow display. No further planning questionnaire is required.

At commissioning, enter the Home Assistant token locally and adjust the initial URL if the installation uses a different port or HTTPS. Confirm actual source freshness, whole-property meter coverage, physical touch alignment and values during battery charging/discharging and grid import/export. These are live verification tasks, not reasons to delay implementation.

Calculated solar generation is now implemented from the confirmed inverter DC and battery entities. Physical installation and comparison against SolarEdge at matched timestamps remain commissioning tasks.
