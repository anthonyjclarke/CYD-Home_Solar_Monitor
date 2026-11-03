# Changelog

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

