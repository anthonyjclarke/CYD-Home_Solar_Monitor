# CYD Home Solar Monitor – Claude context

## Target

ESP32-2432S028R (CYD 2.8″), ILI9341, landscape rotation `1` (USB right), no PSRAM. `espressif32@6.12.0` → Arduino core `2.0.17`, **C++17** (overrides the global C++11 rule). Single env `cyd28`.

## What it is

`0.2.2-dev`. Energy-flow display (solar / home / grid / battery + SOC) that reads a SolarEdge SE6000 + LG battery **via Home Assistant's REST API** (`/api/states/<entity>`), never the inverter directly. A background FreeRTOS task polls six entities; the main loop renders, handles touch, web UI, OTA and Improv. Not yet flashed to hardware – see `docs/HARDWARE_VALIDATION.md`.

---

## Hardware quirks (non-standard vs global CYD rules)

- All TFT_eSPI config is in `include/config.h`, force-included via `-include include/config.h` – **not** `build_flags`. Edit pins there.
- Display on HSPI (`USE_HSPI_PORT`, MISO `12`); touch on its own VSPI bus: CLK `25`, MISO `39`, MOSI `32`, CS `33`, IRQ `36`.
- `TOUCH_CS` must stay **undefined** – TFT_eSPI's shared-bus touch path breaks this board. The resulting compile warning is expected.
- Touch rejects `z < 350`, `z >= 4000` and rail coordinates (phantom-tap guard copied from `CYD_AnimatedPixelClock`).
- RGB LED is held off (active LOW, R4/G16/B17). Backlight is LEDC channel `0`; apps must not reuse it.
- Frame buffer is a 320×240 **8-bit (RGB332)** sprite. Every palette colour in `flow_layout.cpp` must be an exact RGB332 level (R/G steps of 255/7, B steps of 85); a `static_assert` enforces it. Off-level colours shift hue on hardware.
- Fonts are built-in GLCD 1/2/4 (known deviation from the VLW rule).

---

## Data-model rules – do not break

- Internal signs: grid **+import**, battery **+discharge**. HA sources are the opposite (grid +export, battery +charge); `energy::parseValue()` negates them.
- Solar = `max(inverter_DC − battery_internal, 0)`. **Never** derive solar from inverter AC; AC is diagnostic only.
- Never add battery DC into the AC household balance; `sensor.home_power_usage` is authoritative.
- Invalid/stale readings keep the last number (grey) – never silently become 0.
- Freshness uses HA `last_reported`, not `last_changed`. Unknown report time is labelled, not assumed fresh.
- Solar is invalid if DC/battery timestamps differ by more than `energy::skewLimitSeconds(poll)`. Use `energy::metricUsable()` for live/stale decisions – never re-derive it.
- `energy::Metric` indices 0–4 are persisted order – append only (`InverterDc` = 5). `Solar` is derived, not a source.
- `energy_model.h`, `diagnostic_helpers.h` and `flow_layout.cpp` must stay Arduino-free – they compile on the host in `tools/check_host.py`.

## Security rules

- HA token, Wi-Fi password, device access code, CA cert and HTTP bodies are never logged or returned by the API. Serial logs only the URL origin.
- Web mutations need Basic auth (`admin` / device code) **and** header `X-CYD-Request: 1`.
- HTTPS to HA requires a pasted PEM CA – no `setInsecure()` fallback.
- HA reads share one keep-alive `HTTPClient` per cycle. Do not re-enable `useHTTP10()` (it disables reuse); read bodies with `getString()` (handles chunking) and call `Connection::close()` on any failure after the request is sent.

---

## NVS

| Namespace       | Keys                                          |
|:----------------|:----------------------------------------------|
| `solar-monitor` | `access` (12-hex device code), `settings`     |
| `solar-touch`   | `limits` (4×`uint16_t` raw x0,x1,y0,y1)       |
| `cyd-core`      | `boot_count`, `device_name`, `debug_level`    |

`settings` is one JSON blob, `schema: 2` (six entities; optional `battery` label added in 0.2.2 without a schema bump). Loader still accepts `schema: 1` with five entities and appends the DC default – keep that migration.

## Build / release

- `firmware.bin` for browser/ArduinoOTA updates; `firmware-merged.bin` (from `tools/merge_bin.py`) erases NVS – USB clean install only.
- Partition layout must not change between releases (installer + OTA compatibility).
- CI uses `anthonyjclarke/cyd-web-installer` reusable workflow; `python3 tools/check_host.py` runs first and regenerates `docs/images/*.svg`.
- Timezone is a POSIX string (`APP_TZ_POSIX` in `config.h`, applied with `configTzTime`), not ezTime – intentional.

## Style

Two coexisting styles: scaffold services (`*_service.cpp`) use 2-space, one statement per line; app code (`app/`, `data/`, `ui/`, `web/`) is dense 4-space. Match the file you are editing.
