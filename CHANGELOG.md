# Changelog

## [0.5.0-dev] 03-11-2026

### Added

- mDNS: the device answers at `http://solar.local/`, shared with the ArduinoOTA responder.
- `docs/index.html` handbook: install, first setup, Home Assistant setup, an annotated display guide, state gallery, full settings reference, freshness rules, serial diagnostics, troubleshooting and security notes, with web UI screenshots.

### Changed

- Centre layout re-laid for the full canvas: 48 px font 6 values on all four arms, full-length bolder chevrons, larger state text and a large battery gauge with its percentage.
- Cards layout uses font 6 values and drops the engineering subtitles.
- Removed the browser login (trusted LAN only). Changes still require the `X-CYD-Request` header to block cross-site requests; ArduinoOTA and the setup hotspot keep the device code.
- Preview renderer uses TFT_eSPI's real glyph widths for fonts 2, 4 and 6.

---

## [0.4.0-dev] 03-11-2026

### Added

- Weather from Open-Meteo for a configured location: current temperature and conditions, and tomorrow's maximum and conditions, fetched every 30 minutes over HTTPS verified against ISRG Root X1.
- Drawn weather icons (clear, partly cloudy, cloudy, fog, drizzle, rain, snow, storm) on both layouts.
- Browser **Location, timezone and weather** settings: place search (Open-Meteo geocoding, run in the browser), latitude/longitude, and a POSIX timezone filled from a built-in Olson table or entered manually.
- Battery gauge icon filled to state of energy, coloured by level, on both layouts.
- `[WEATHER]` serial diagnostics; browser shows local time and current/tomorrow weather.

### Changed

- Timezone is a setting applied immediately on save, instead of the fixed Australia/Sydney default.
- The centre layout shows state of energy beside the gauge on its own row, separate from "Charging"/"Supplying".
- Reader task stack raised to 16 KB for TLS.

---

## [0.3.1-dev] 03-11-2026

### Fixed

- Battery, state of energy and solar no longer go grey when the battery is idle and full. HA left the battery entities' `last_reported` unchanged for over ten minutes while the inverter kept reporting, so per-entity report age wrongly marked them stale and solar was rejected for timestamp skew.

### Changed

- Report age is now judged from the snapshot heartbeat (newest report among valid sources). Unavailable entities and a stalled integration are still rejected.
- Serial `[READ]` reasons include `fresh-unchanged`; `[POLL]` logs `heartbeat_age`; `[SOLAR]` labels the raw report skew.
- Removed the "Source times differ" panel and browser warnings; raw skew remains in diagnostics.

---

## [0.3.0-dev] 03-11-2026

### Added

- Optional **Solar energy today** and **Solar forecast tomorrow** kWh entities, read once a minute on the existing connection.
- Centre layout top row shows today's kWh (falling back to the clock) and "Tomorrow ~N kWh"; the cards layout title shows today's kWh.
- Browser dashboard shows both totals; `/api/status` reports them under `totals`.
- `[TOTAL]` serial diagnostics and host tests for kWh parsing and total freshness.

---

## [0.2.3-dev] 03-11-2026

### Added

- Light centre-flow panel layout: solar above, grid and battery either side and house below a central hub, with animated chevrons, a battery level bar, the clock and a plain-English status line ("Good time to use power" while exporting or charging a full battery).
- **Panel layout** browser setting to choose centre flow (default) or the original dark cards; saved settings without it get the centre layout.
- Preview renders of both layouts from `tools/check_host.py`.

---

## [0.2.2-dev] 03-11-2026

### Fixed

- Panel palette now uses exact RGB332 levels; the 8-bit frame buffer previously turned the navy cards dark green and the background black. A compile-time check enforces this.
- The touch that wakes a dimmed panel at night no longer also opens the setup page.
- The setup page is drawn into the frame buffer, removing its once-per-second flash.
- The "Source times differ" status now uses the same 10-second minimum skew limit as solar validity.

### Changed

- All six Home Assistant reads in a cycle share one keep-alive connection instead of six separate connections.
- The reader task copies settings only when they change, not every 100 ms.
- Power shows watts below 1 kW and two decimals above, on the panel and in the browser.
- The battery label beside state of energy is a setting (default `LG 9.8 kWh`).
- Freshness and skew-limit rules are shared helpers used by the panel, browser API and serial log.
- Timezone and NTP servers moved to `config.h`; the backlight is written only when its level changes.

---

## [0.2.1-dev] 03-11-2026

- Added serial poll diagnostics with HTTP status, timing, numeric values, units and freshness reasons.
- Added solar input/skew reporting and AC balance checks to explain mismatched readings.
- Added timestamped boot, configuration, Wi-Fi, clock, data-state and periodic heap summaries.
- Kept credentials and arbitrary response text out of serial output and centralised poll logging on the main loop.

---

## [0.2.0-dev] 03-11-2026

- Added inverter DC acquisition and calculated solar generation on the panel and browser dashboard.
- Kept inverter AC in diagnostics and routed the flow arrows through an inverter hub.
- Added `Watt`/`Watts` unit support and source validity, freshness and timestamp-skew checks for solar.
- Added a legacy settings migration retaining existing credentials and the first five mappings.
- Verified charging, discharging, grid charging, night, missing-input and skewed-input calculations with host checks.

---

## [0.1.0-dev] 03-11-2026

- Added the CYD 2.8-inch PlatformIO firmware and existing installer build contract.
- Added Home Assistant entity configuration and background live power acquisition.
- Added an AC/DC flow screen, source validity handling and synthetic demo scenes.
- Added a local browser dashboard, persisted settings and manual authenticated OTA.
- Added touch calibration, night dimming and Improv Wi-Fi provisioning.
- Verified the firmware build and portable energy-model checks; hardware commissioning remains pending.

