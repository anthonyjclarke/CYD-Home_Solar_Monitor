# Source release 0.9.0 – verification record

Prepared on 5 November 2026 on `dev`, from base commit `dc1b676`. This is a local source-release commit. No release tag, push, GitHub publication, installer update or device flash is included. The latest published release recorded before this preparation remains `0.8.0`.

---

## Included behaviour

Actual energy, battery, optional daily totals and weather share a rolling 24-hour history on the CYD and WebUI. The store uses 288 UTC five-minute snapshots and alternating verified LittleFS checkpoints every 15 minutes. Missing and powered-off intervals remain gaps; demo and unsynchronized-clock samples are excluded. Healthy storage can lose roughly 15 minutes of recent samples on sudden power loss. These are sampled readings, not interval averages or calculated energy totals.

History replaces the flow-screen Settings button. Hold longer than two seconds for Settings, release, then hold again there for calibration. The history JSON separator defect is fixed and covered using the real firmware handler and ArduinoJson String writer.

Display settings add saved serial log levels and dim-only, off-only or staged dim/off/on night schedules with minute precision. Default Info logs routine activity about once per minute. Serial and CYD share startup readiness diagnostics. Old profiles retain their hour-based dim-only behaviour. Display-off leaves acquisition, history, serial, HTTP and OTA running; the first wake contact is consumed and grants one minute of full brightness.

The existing control/API/OTA contracts, six-source order, configuration schema numbers and partition offsets/sizes are preserved. New configuration fields are optional for older clients. Dependencies remain pinned. No illustrative measurement has been imported into device history.

---

## Documentation and image sweep

Reviewed the README, changelog, handbook, agent/Claude guidance and every project Markdown guide. Corrected source identity, history ownership, metric statistics, settings/calibration gestures, night controls, test commands and preview counts. Historical research, release notes and the initial documentation-only review remain dated records; their earlier measurements are not current verification.

All 22 panel SVGs were regenerated from the shared renderer at 320 × 240, along with thirteen PNGs. Four boot states cover ready light/dark, waiting and warning; five history states cover solar, signed battery, dark, weather and empty. The twelve flow scenes and weather-only centre scene retain their actual renderer geometry and controls. Version text comes from `include/config.h`.

All browser captures were refreshed against the local synthetic fixture. They use a fixed public example date, a reserved HA address and visibly labelled Preview status. WebP encoding keeps text readable while reducing embedded flash use. The decorative footer artwork is unchanged. No physical panel photograph or live system reading is represented by these images.

The browser gallery includes [full dashboard](images/webui-dashboard-full.webp), [live tiles](images/webui-dashboard.webp), [flow](images/webui-flow.webp), [combined history](images/webui-chart.webp), [battery history](images/webui-history-metric.webp), [connection](images/webui-settings.webp), [Display](images/webui-display.webp), [night controls](images/webui-night-settings-preview.webp), [serial debug](images/webui-debug-settings-preview.webp), [location](images/webui-location.webp), [entities](images/webui-entities.webp), [diagnostics](images/webui-diagnostics.webp) and [Device & firmware](images/webui-device.webp). Superseded JPEG files and the redundant history/dashboard capture were removed.

---

## Desktop verification

All commands below passed on 5 November 2026. The host suite ran every integration harness with the installed pinned headers; the three real-handler/config harnesses also passed against a fresh isolated ArduinoJson 7.4.2 installation using the CI installation command. Dashboard JavaScript syntax and all Python tool sources parsed successfully. Local link/anchor checks and `git diff --check` passed. Run from the repository root:

```bash
python3 tools/check_docs.py
python3 tools/check_host.py
node tests/test_web_history.js
node tests/test_web_settings.js
pio run -e cyd28
```

Host tests use warnings as errors. The real history/configuration harnesses use pinned ArduinoJson 7.4.2 and host String, HTTP and NVS stand-ins. CI now installs those headers on a clean checkout and runs both browser regression suites as well as documentation/model checks. Physical storage, HTTP transport, TLS and OTA are outside these harnesses.

The final PlatformIO build reports 1,817,573 / 1,835,008 bytes of app flash (99.0%) and 74,800 / 327,680 bytes of static RAM (22.8%). The actual `firmware.bin` is 1,824,144 bytes, leaving only 10,864 bytes in an OTA slot after image overhead. The merged image and separate installer part map were generated; partition offsets and sizes match the previous layout. The dedicated-touch-bus `TOUCH_CS` warning is expected. Generated outputs remain ignored and are not release attachments.

Browser inspection confirmed desktop light/dark rendering, combined/single/categorical history selection, no reported console warnings/errors, synthetic Off=0 save, restoration of Info and staged times after reload, and all 27 handbook image references loading without a broken image or desktop horizontal overflow. No actual HA, CYD, USB port or production configuration was accessed. The browser viewport override did not produce a narrower viewport in this session, so mobile acceptance remains pending.

---

## Acceptance still needed

Keep the unchecked cases in [HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md) and [WEB_INSTALLER.md](WEB_INSTALLER.md) open. Physical acceptance includes power cuts during checkpoint writes, a 24-hour soak, NTP recovery, both panel themes and touch gestures, saved log levels and startup reports, dim/off/on across midnight/DST, runtime heap/latency and both OTA paths/app slots. The fresh installer, provisioned update and update from `app1` remain publication gates under `AGENTS.md`.

Flash-slot headroom is limited; rebuild after changing embedded help/assets. Local compilation does not prove runtime heap or flash endurance. Existing follow-up observations about optional stale browser tiles, flow animation after status failures, unknown source-age wording, USB parser bounds and response buffering remain in [REVIEW_AND_ROADMAP.md](REVIEW_AND_ROADMAP.md).
