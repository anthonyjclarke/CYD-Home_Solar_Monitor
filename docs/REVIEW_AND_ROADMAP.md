# Code review and roadmap

Updated 5 November 2026 against source `0.9.0` (base commit `dc1b676`). This is the current implementation summary. The original 3 November review targeted `0.2.1-dev`; its fixed findings and speculative version schedule no longer describe the code. Historical changes remain in [CHANGELOG.md](../CHANGELOG.md).

---

## Implemented

The reader polls six individual Home Assistant REST states and derives solar from inverter DC and battery power. It shares one keep-alive connection per cycle, copies configuration only on revision changes, and publishes snapshots under a mutex. Heartbeat-based upstream freshness keeps unchanged battery states live while still detecting a stalled integration. Optional daily solar and tomorrow forecast entities are read on a slower cadence.

Both RGB332-exact layouts, battery gauges, weather icons, the plain-language spare-power status, configurable battery label and timezone, buffered setup page, swallowed night-wake touch and change-only backlight writes are implemented. The centre layout pushes only the pipe region at 20 fps, with full frames at 4 fps. Built-in fonts 1/2/4 remain a deliberate readability decision; the font-6 redesign was reverted.

The browser includes live tiles, animated flow, a shared persistent 24-hour history chart, settings, diagnostics, manual app upload, footer artwork and embedded handbook. Open-Meteo weather uses verified TLS; location search runs from the browser. Device history and the 320 × 240 history screen are included in the 0.9.0 source release; real-board persistence, touch and OTA validation remain pending.

Saved serial debug levels and minute-rate default output, shared serial/CYD boot diagnostics and minute-precision dim/off/on schedules are implemented. Old dim-only profiles retain their behaviour; screen-off leaves data/history, serial, WebUI and OTA active. See the [0.9.0 verification record](RELEASE_0.9.0.md).

---

## Original review findings resolved

| Original finding          | Current implementation           |
| ------------------------- | -------------------------------- |
| Wrong panel colours       | RGB332 palettes/static asserts   |
| New connection per entity | One reused connection per cycle  |
| Config copied every tick  | Copy only on revision change     |
| Wake tap opens setup      | Consume dimmed-panel wake tap    |
| Setup page flicker        | Render through frame sprite      |
| Duplicated skew decisions | Shared model helpers             |
| Fixed battery label/TZ    | Persisted browser settings       |
| Every-loop PWM writes     | Write only on brightness change  |
| Three-decimal kW values   | W below 1 kW; two decimals above |

Connection reuse was selected instead of the proposed `/api/template` or WebSocket migration. The current implementation needs neither. Source and hardware constraints are in [AGENTS.md](../AGENTS.md).

---

## Remaining implementation observations

These are code-inspection findings, not new hardware results. They remain follow-up items for later work; the release sweep does not resolve them.

The optional browser tiles need transition coverage. In `src/web/web_page.h`, some update branches write Solar today, Tomorrow or Weather only while configured/fresh; there is no fallback resetting every tile when its entity/location is cleared or its forecast becomes unavailable. A previously shown value can therefore remain until reload. Also, a failed status refresh greys nodes/tiles but does not explicitly stop the flow animation; persistent history sampling now owns gaps independently of browser live-status requests.

The centre renderer replaces the supplied status with “All readings live” or the spare-power message whenever displayed metrics are usable, even when report time is unknown. The API exposes `reportTimeKnown` and serial diagnostics retain `source-age-unknown`; the centre status can hide that distinction. Do not describe the centre message alone as proof that upstream age was verified.

The vendored Improv parser needs bounds hardening. `handleSerial()` writes into a 128-byte buffer based on a frame's advertised length; `parseImprovData()` reads command/SSID/password offsets without checking each against the received payload length. Treat this as a USB-serial input risk and add malformed-frame regression coverage before changing provisioning. Preserve the existing parser-reset and leading-newline reply patches.

HTTP body size limits are checked after buffering when content length is unknown. They reject oversized content but do not provide a strict allocation ceiling. Consider bounded streaming before supporting less trusted endpoints. A configured HA token with no successful startup reads can also delay the first weather fetch because of the initial `haPublished` gate.

---

## Next validation priorities

Complete real-board installer smoke testing, settings-preserving update and update from `app1`, as tracked in [WEB_INSTALLER.md](WEB_INSTALLER.md). Verify invalid/interrupted uploads recover, token clearing and schema migration persist, and long-running Wi-Fi/TLS behavior is stable. Use [HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md); retain existing observations and add dates for new evidence.

Host assertions cover decoding, signs, timestamps, freshness, solar dependencies, history recovery, gestures, log cadence, startup status and night scheduling. Real serializer/config/API harnesses use ArduinoJson and host stand-ins; JavaScript tests exercise the embedded chart and settings event handling. Physical HTTP transport, NVS, serial provisioning, TLS and OTA still need board tests. The synthetic preview server is a development aid rather than device integration coverage.

---

## Deferred features

Daily grid import/export totals, tariffs/costs, free-power windows, forecast progress rings, QR setup, explicit grid-outage detection, signed automatic updates and verified rollback are not implemented. No version targets are assigned here. Direct Modbus and equipment control remain outside the selected read-only Home Assistant route.

The original Rayhub research is preserved as dated product context in [RAYHUB_REVIEW_AND_BUILD_BRIEF.md](RAYHUB_REVIEW_AND_BUILD_BRIEF.md). It is not a current external-product audit or an instruction to implement every feature.
