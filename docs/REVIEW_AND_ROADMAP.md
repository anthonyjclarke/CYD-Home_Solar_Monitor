# Code Review and Roadmap – toward Rayhub parity

Reviewed 3 November 2026 against firmware `0.2.1-dev`, and against Rayhub's current public product and help pages. The build fits (`firmware.bin` 1.22 MB of a 1.79 MB slot) and `tools/check_host.py` passes. Nothing has been flashed to hardware yet.

---

## Overall assessment

The data layer is the project's strength. Sign conventions, the DC solar formula, freshness via `last_reported`, timestamp-skew rejection, secret-free logging and the host-testable model are more careful than most commercial firmware. The gap to Rayhub is almost entirely **presentation and daily-energy features**, plus a couple of efficiency issues in the acquisition path.

---

## Findings – fix first

### 1. Card colours will be wrong on the real panel

`app.cpp` renders into an 8-bit sprite, which TFT_eSPI stores as RGB332. The palette in `flow_layout.cpp` is built from dark navy greys that do not survive that quantisation.

| Colour   | Intended  | RGB332 result         |
|:---------|:----------|:----------------------|
| `bg`     | `#080f1b` | black                 |
| `panel`  | `#142236` | dark **green** (0,1,0) |
| `INV`    | `#243a50` | dark grey             |

The SVG/PNG previews render full 24-bit colour, so they hide this. Simplest fix: pick a palette whose values are exact RGB332 levels (R/G in steps of 36, B in steps of 85). Better fix: a 4-bit palette sprite (`setColorDepth(4)` + `createPalette()`) gives 16 exact RGB565 colours in half the RAM (38.4 KB), but rules out anti-aliased fonts. If smooth fonts are adopted (see §UI), use small 16-bit sprites per card instead of one full-screen buffer.

### 2. Six HTTP requests per cycle, each on a fresh TCP connection

`readEntity()` creates a new `WiFiClient`/`HTTPClient` per entity, so each 5-second cycle does six TCP handshakes (plus six TLS handshakes over HTTPS). It is also the root cause of the timestamp-skew machinery: with 1.5 s connect and read timeouts per request, the first and last readings can be well over 10 s apart when HA is slow.

Simplest fix: one `POST /api/template` request that renders all six states, units and `last_reported` values as a single JSON object – one connection, one coherent snapshot, the existing parser reused per field. I am not certain whether HA still restricts `/api/template` to admin users; verify with the owner's token before relying on it. The more ambitious alternative is the HA WebSocket API (`subscribe_entities`), which pushes changes and would match Rayhub's ~1 s refresh, at the cost of a new library and a persistent connection.

### 3. Worker copies the full config every 100 ms

`worker()` copies `monitor::Config` (token up to 2 KB, CA up to 6 KB, seven `String`s) under the mutex on every 100 ms tick, whether or not anything changed. That is ~10 heap allocate/free cycles per second for the device's lifetime – a fragmentation risk with TLS also allocating. Copy only when `revision` changes.

### 4. Wake tap also toggles the setup page

At night the backlight dims to 1/5. The tap that wakes it also flips `details`, so the user lands on the setup page. Rayhub swallows the waking touch. Fix in `app::update()`: if the panel was dimmed, restore brightness and consume the tap.

### 5. Setup page repaint flickers

`paintInfo()` does `fillScreen()` directly on the TFT once per second, which flashes visibly. Draw it into the existing sprite instead (the page is static apart from the IP).

---

## Findings – tidy-ups

- The solar skew limit `std::max<uint32_t>(10, pollSeconds*2)` is repeated 7 times across `app.cpp`, `monitor_web.cpp`, `serial_diagnostics.cpp` and `ha_source.cpp`. `app.cpp:138` uses `pollSeconds*2` *without* the 10 s floor, so the "Source times differ" status disagrees with solar validity at poll ≤ 4 s. One helper (e.g. `energy::skewLimit(poll)`) fixes both.
- The "usable/fresh" loop is likewise computed three times (panel, web, serial). A single `energy::usableMask()` would keep them consistent.
- `"LG 9.8 kWh"` is hard-coded in three places. Make battery label/capacity a setting, which also enables "time to full/empty".
- Timezone POSIX string and NTP servers are hard-coded in `main.cpp`; move to `config.h`.
- `delay(5)` in `loop()` – replace with `vTaskDelay(1)` or remove (rule: no `delay()` in loop code).
- `setBrightness()` is called (and `ledcWrite`) every loop pass; write only on change.
- `power()` always prints three decimals (`4.404 kW`). The fluctuating last digit is noise; Rayhub shows two. Show watts below 1 kW, `x.xx kW` above.

---

## Rayhub feature gap

| Rayhub feature                        | Here      | Effort | Approach                                 |
|:--------------------------------------|:----------|:-------|:-----------------------------------------|
| Live flow, stale grey, arrows stop    | Done      | –      | –                                        |
| Battery %, charge/discharge           | Done      | –      | –                                        |
| Night dimming                         | Dim only  | Low    | Add Off/Keep + swallowed wake tap        |
| Touch calibration                     | Done      | –      | –                                        |
| "kWh today" headline                  | Missing   | Low    | Read HA daily `utility_meter` sensors    |
| Tap totals: export / import kWh       | Missing   | Low    | Same sensors, tap zone top-left          |
| Spent / earned today                  | Missing   | Low    | Import/feed-in price settings × totals   |
| "Good time to use power" status       | Missing   | Low    | Rule on export or battery near full      |
| Free-power window + banner            | Missing   | Low    | Time window setting, banner, cost rule   |
| Tomorrow forecast (corner)            | Missing   | Low    | Read Forecast.Solar/Solcast HA entity    |
| Solar-vs-forecast ring view           | Missing   | Medium | Second screen, tap centre, 30 s return   |
| QR code / on-panel setup              | Partial   | Medium | QR to device URL and AP; keep browser UI |
| Off-grid banner                       | Missing   | Medium | Needs a reliable grid-status entity      |
| Auto update + signed + rollback       | Missing   | High   | GitHub release manifest; see note below  |
| Direct Modbus, auto brand detect      | Out       | High   | Not needed – HA is the chosen route      |

Using Home Assistant makes most of these *cheaper* than Rayhub's own implementation: HA already computes daily totals, and Forecast.Solar or Solcast provide forecasts without a learning model. Each is "add one or two configured entities, poll them every 60 s, render them".

Auto-update note: I am not certain that the prebuilt Arduino `2.0.17` bootloader has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` set, which `esp_ota_mark_app_valid_cancel_rollback()` depends on. Verify before designing rollback around it.

---

## UI – looking like a product

Current screen: four equal labelled cards and a small "INV" box. Rayhub's: a central home with icon nodes for sun, grid and battery, the day's kWh at the top, a forecast in the corner and a plain-English status line. Suggested changes, roughly in order of visual impact:

1. **Drop engineering labels from the panel.** "Calculated / DC", "Home demand / AC", "Whole-property meter" and "INV" belong in the browser diagnostics. Use "Solar", "Home", "Importing 1.53 kW", "Charging · 28%".
2. **Icons instead of title text.** Sun, house, pylon and battery drawn from TFT primitives (or 1-bit bitmaps in PROGMEM) at each node.
3. **Battery SOC gauge.** A filled battery outline coloured by level is read faster than `28.5%`.
4. **Smooth fonts.** NotoSans VLW at 2–3 sizes. This is the single biggest "hobby vs product" signal, and is already a project rule. Requires the per-card 16-bit sprite approach from finding 1.
5. **Better flow animation.** Several dots per path, spacing or speed scaled to power, dots fading when power is near zero, rather than one 3-px square stepping four positions.
6. **Top bar.** `25.6 kWh today` on the left, forecast or weather on the right, a small Wi-Fi/HA status dot instead of the `HA`/`WAIT` badge.
7. **Status line in plain English.** "Good time to use power", "Waiting for Home Assistant", "Can't reach Home Assistant" instead of "HA connected / source age unknown".
8. **Hide the IP from the main screen.** It is already on the tap-through page.

Render-check every change with `tools/check_host.py`, but judge colours on the panel only (finding 1).

---

## Suggested sequence

| Stage | Scope                                          | Version |
|:------|:-----------------------------------------------|:--------|
| 1     | Findings 1–5 and tidy-ups – **done in code** | `0.2.2` |
| 2     | Daily totals, prices, status line, free window | `0.3.0` |
| 3     | Visual redesign: icons, VLW fonts, SOC gauge   | `0.4.0` |
| 4     | Forecast entity and ring view                  | `0.5.0` |
| 5     | QR setup, Off night mode, update checks        | `0.6.0` |

Stage 1 is implemented in `0.2.2-dev` except finding 2, which uses connection reuse rather than `/api/template`. Commission it on hardware before building on it: the colour issue and real HA cadence will affect every later UI decision.
