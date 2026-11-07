# Code audit and peer review – 7 November 2026

Reviewed source `0.9.0`, branch `main`, commit `cdcb226cfb42a60967e89e736b6d15d933c9c742`. The checkout was clean before review. This report supplements the historical [documentation review](DOCUMENTATION_REVIEW.md) and [release verification](RELEASE_0.9.0.md); it does not replace their dated evidence.

Peer-review recommendation: address the reproducible Improv memory faults before another public firmware release. Follow with bounded response handling and accurate unavailable/disconnected dashboard states. The architecture and existing regression coverage are a useful foundation, but passing desktop checks do not establish hardware acceptance. The current image also has very little room for enhancements.

This review changes contributor and development documentation only. Findings below remain open; no firmware fix is implied by their inclusion here.

---

## Scope and method

Inventoried 138 tracked files, including 4,615 lines of application/header/vendored C++ before this patch. Review covered service startup, task/snapshot ownership, energy conversion and freshness, history encoding/storage, configuration migration, display/touch/night rules, HTTP routes and upload handling, embedded dashboard, provisioning, build/embedding/preview tools, tests, dependency declarations, partition map, local workflow and project documentation. Artwork was checked through references and build cost; this was not a new visual redesign or mobile-browser acceptance pass.

Ran existing checks and a fresh firmware build, then used temporary targeted probes against extracted production functions to verify suspicious input/state transitions. Serial parser probes used AddressSanitizer and UndefinedBehaviorSanitizer; browser and Wi-Fi probes used host stand-ins. Those probes establish code behaviour, not a demonstrated ESP32 exploit or RF/device result. The temporary probes were not added as permanent regression coverage; each finding specifies the test that a corrective patch should retain.

Installed framework/library internals were compiled but not independently audited in full. The external reusable release workflow, hosted releases, external product claims and current dependency advisories were not revalidated. No credentials, live HA/inverter, serial port, board flash, publication or Git commit was used.

---

## Findings

`P1` means high-priority memory safety work before release. `P2` means a functional or resilience correction for the next maintenance work. `P3` means a lower-priority presentation correction. Locations refer to the reviewed commit, before documentation changes.

### F1 – P1: Improv accepts lengths beyond its receive buffer and nested payload

Locations: `lib/ImprovWiFi/src/ImprovWiFiLibrary.h:50`, `lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp:12`, `:255` and `:299`. See the [vendored implementation](../lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp).

The receive buffer contains 128 bytes. The advertised payload length is accepted without checking available storage, and `handleSerial()` writes at `_buffer[_position++]`. A long frame reaches index 128 before checksum handling. Separately, `parseImprovData()` reads `data[0]`, `data[1]`, the SSID length, the password length and both string spans without establishing each boundary first.

Reproduced in host probes using the actual extracted methods. An advertised 255-byte serial payload trips UndefinedBehaviorSanitizer at index 128. A two-byte Wi-Fi RPC payload `{1, 0}` trips AddressSanitizer when the parser reads beyond the provided array. The latter directly exercises the payload parser; the serial path passes a view into its larger buffer and can instead consume bytes outside the advertised payload. Malformed local USB input can corrupt memory or cause invalid credential processing; remote exploitation was not tested.

Correction: reject an excessive advertised length before storing payload bytes; validate the minimum RPC shape, SSID/password length-byte positions and complete spans before accessing them. Reset safely on rejection and preserve the existing next-frame-reset and reply-newline patches. The checksum is consumed separately, so calculate the limit from actual stored bytes rather than an assumed total-frame layout.

Acceptance: retain host sanitizer cases for empty/short payloads, every truncated string boundary, oversized frames, bad checksums and valid frames immediately following rejection. Also cover normal empty-password provisioning and installer identification. Physical provisioning acceptance remains necessary.

### F2 – P2: Optional dashboard tiles retain old values after data disappears

Location: `src/web/web_page.h:309` in the [dashboard](../src/web/web_page.h).

The Solar today, Tomorrow and Weather branches write only when an entity is configured or a usable fallback exists. Clearing today's entity or the location, or losing both tomorrow's forecast and weather, leaves previous text and pills intact. The next successful refresh then removes stale styling from all three tiles. An old measurement can therefore look current after a successful save or refresh.

Reproduced by running the extracted `refresh()` through a valid synthetic response and then a response with removed/unavailable optional data. All three old values remained, and the Tomorrow tile had no stale class. The existing JavaScript suites cover settings and history, not this transition.

Correction: assign a complete value, label, pill and freshness state for every optional tile on every response, including explicit unconfigured and unavailable fallbacks.

Acceptance: cover configured-to-cleared, fresh-to-invalid, forecast-to-weather fallback, loss of both sources and recovery without reloading the page.

### F3 – P2: Live polling has no deadline and keeps flow enabled on failure

Locations: `src/web/web_page.h:223`, `:294`, `:326` and `:334`.

The failure handler greys tiles/nodes but never disables the flow arms. A host transition from valid status to a rejected request leaves the previous arms enabled. The disconnected badge and continuing comets communicate conflicting states. Persistent history owns its own gaps; this finding concerns the live flow.

In addition, `api()` has no request deadline and `refresh()` has no in-flight guard. `busy` gates firmware upload, rather than status polling. The three-second timer can start another request while an earlier one is unresolved; the saved `last` timestamp is never used to expire the live presentation. Delayed responses can arrive out of order. This part is a static finding, not a measured browser timeout or device load test.

Correction: bound live requests with a deadline, allow one outstanding status poll, reject superseded responses and expire live state by elapsed time. Route errors/timeouts through one handler that disables all flow arms while retaining clearly stale last-known values.

Acceptance: use deferred promises to cover a hung request, overlapping timer ticks, late responses, explicit rejection and recovery. Check reduced-motion behaviour too.

### F4 – P2: Response size checks do not bound peak allocation

Locations: `src/data/ha_source.cpp:94` and `:158`; related inbound check at `src/web/monitor_web.cpp:69`. See the [reader](../src/data/ha_source.cpp) and [HTTP handlers](../src/web/monitor_web.cpp).

HA and weather responses with unknown content length are buffered by `getString()` before the 16 KiB/8 KiB post-read check. An oversized chunked response can exhaust or fragment heap before being rejected on this no-PSRAM target. The 12,288-character settings check similarly runs after `WebServer` has materialized the body; it is not a transport-level request allocation limit. These are static findings; no oversized request was sent to a physical device.

Correction: introduce a bounded, chunk-aware body sink or bounded streaming parser with explicit cancellation and connection cleanup. Handle absent, incorrect and excessive lengths. Apply any inbound transport limit before accepting the complete settings body where the pinned server permits it. Preserve HTTP/1.1 keep-alive and verified TLS behaviour.

Acceptance: test known-length and chunked oversized bodies, truncation, slow progress, failed allocation and a following valid request. Measure minimum heap and main-loop responsiveness on hardware.

### F5 – P2: Portal-assisted setup changes later outage behaviour

Location: `src/wifi_service.cpp:48` in the [Wi-Fi service](../src/wifi_service.cpp).

Initial portal entry checks `!everConnected`, but its retry branch checks only `portalStarted`. Successful connection stops the portal without clearing that flag. A device that needed the portal earlier in the same boot can reopen its access point during a later network outage. A boot that never used the portal behaves differently. Previous development guidance stated that later loss used reconnection alone.

Reproduced with the actual extracted `update()` and simulated manager/clock/Wi-Fi state: initial portal, successful connection, disconnection, then a retry interval reopened the portal even though `everConnected` was true. This establishes the branch behaviour, not real WiFiManager/RF performance.

Correction: decide and document the intended recovery policy. If initial-setup-only is intended, apply `!everConnected` to the retry path or clear its setup state after success. If automatic recovery portals are desired, implement that deliberately with a defined delay and visible status. Development documentation now describes current behaviour rather than assuming a fix.

Acceptance: cover saved-network success, portal-assisted success, initial portal timeout/retry and subsequent disconnect/reconnect for both boot paths.

### F6 – P2: A failing HA startup can prevent weather indefinitely

Location: `src/data/ha_source.cpp:273`.

With a token configured, the first weather request requires `haPublished`. That flag is set only after at least one source succeeds. If HA is unreachable, the token is invalid, or all mappings fail from boot, weather remains blocked despite valid Wi-Fi, clock and coordinates. The comment describes independent weather, but the initial gate can last for the entire outage. This is a static finding already noted in the earlier review.

Correction: keep the initial preference for publishing power first, but bound that preference by an elapsed-time deadline or an initial completed cycle. Continue to close HA before opening weather TLS and honour reader pause/cancellation.

Acceptance: verify weather can start with no token, an invalid token, all HA requests failing and normal HA startup; ensure the normal first power publication remains prompt.

### F7 – P2: Handbook helper recipes turn unavailable input into numeric power

Location: `docs/index.html:173` and `:180`, under the [HA setup recipes](index.html#ha).

Both templates use `float(0)` for required upstream entities without an availability condition. A missing grid reading can therefore produce household power equal to inverter output, and a missing DC/battery input can produce a misleading solar value. The CYD receives a numeric helper state and cannot recover the lost availability information. Integrating fabricated solar power can also contaminate daily energy totals. This follows directly from the supplied recipe; it was not tested against the owner's HA instance.

Correction: provide availability conditions for all required numeric inputs and explain how to configure them in the chosen HA helper/template route. Missing measurements should make the helper unavailable. Preserve a true zero when all inputs are available and the calculated result is zero. Check downstream integral/utility-meter treatment of outages as part of the recipe revision.

Acceptance: document/test each upstream entity becoming unknown or unavailable, then recovering, alongside valid zero, grid import/export and battery charge/discharge. Editing this repository must not silently alter deployed HA helpers.

### F8 – P3: Centre layout hides unknown upstream reporting age

Location: `src/ui/flow_layout.cpp:345` in the [shared renderer](../src/ui/flow_layout.cpp).

`app::update()` supplies an unknown-source-age status when receive-fresh values lack report time. The centre renderer replaces it with “All readings live” or the spare-power message whenever the displayed metrics are usable. The API and serial diagnostics retain the distinction. This is a static presentation finding, previously recorded, rather than a new freshness algorithm defect.

Correction: carry an explicit report-time-known flag into the view and preserve its warning independently of the spare-power message. Do not remove the existing idle-battery heartbeat behaviour to change wording.

Acceptance: render unknown-age, verified-age and stale scenes in both layouts and assert the visible status, including the spare-power case.

---

## Architecture, security and enhancement recommendations

### Preserve the working boundaries

The portable model/rendering split, core-0 network reader, revision-gated publication and shared device/browser history are sound boundaries to retain. Explicit history encoding, alternating verified banks, non-destructive mount failure and clock/gap rules show useful failure awareness. Existing tests cover signs, timestamps, rollover, migration and serialization; extending those patterns is preferable to replacing the framework or introducing a frontend bundler.

Refactor the large settings handler into a host-testable validation function only when making a related correction. Currently, extracted debug/night blocks establish their own behaviour but do not establish full-route validation, token handling, persistence failure or upload recovery. Similarly, a shared update coordinator should own HTTP and ArduinoOTA activity if update handling changes. `source::pause(true)` does not acknowledge that the worker is idle, and its weather branch can still be reached after cancelling an entity loop. Treat quiescence and exclusive ownership of the updater as design work requiring tests, not an already proven update failure.

When touching panel freshness, take the evaluation time after `source::snapshot()` in `app::update()`, as history and serial diagnostics already do. A worker publication newer than the function's incoming `now` can otherwise appear extremely old through unsigned receive-age subtraction for a frame. Preserve the rollover-safe subtraction itself.

### Reclaim flash before adding features

The fresh build uses 1,817,517 / 1,835,008 linker-reported app bytes (99.0%), but `firmware.bin` occupies 1,824,096 bytes. Actual OTA headroom is 10,912 bytes, about 0.6%. Static RAM is 74,800 / 327,680 bytes (22.8%); this does not measure task stacks, sprites, TLS or dynamic heap at runtime. The incremental cached build reported 1,817,573 app bytes and a 1,824,144-byte image, consistent with the historical release record. Build metadata/path differences mean these two local artifacts are not claimed to be byte-identical.

First inventory the cost of each embedded asset and remove redundant full-page captures or reduce their encoded size while preserving readable help. Consider making large optional help imagery a separate build choice. Add a CI budget for actual app-image bytes and a deliberate minimum margin; 64 KiB would be a reasonable initial team policy to evaluate, not a hardware requirement. Do not change deployed partition offsets to recover a few kilobytes.

### Make trust and data assumptions explicit

The unauthenticated HTTP interface and unsigned manual uploads are established trusted-LAN choices, not new vulnerabilities introduced by this patch. The mutation header prevents ordinary cross-origin browser writes; it does not authenticate LAN clients. Preserve token redaction, verified outbound TLS, disabled credential-bearing redirects and the absence of permissive CORS. Optional maintenance authentication or physically enabled updates are future design proposals with recovery requirements. Consider rejecting private-key PEM markers in the editable CA field, since that field is returned by the API.

The heartbeat takes the newest report across all six configurable sources. An independently updating helper or another integration can keep an older source usable. Preserve the known idle-battery fix, but document that assumption and evaluate an explicit heartbeat source/group if broader mappings are supported. This is a design limitation; source-specific freshness changes require migration and stalled-integration fixtures. Also validate persisted touch calibration before using it: the current restore checks byte length, whereas saving checks the coordinate spans. Avoid invalid equal endpoints reaching `map()`.

### Add useful capabilities within a measured budget

After the maintenance findings and flash work, useful small enhancements are clearer per-source error states in the dashboard, last-success age, checkpoint health/persisted-through visibility, and a redacted diagnostic export containing version, source reasons and memory statistics. Prefer exposing existing diagnostic state over new polling or persistence. Never export tokens, device codes, passwords or raw upstream bodies.

CSV history export and an optional battery-free layout are reasonable later proposals, with explicit schema and solar-input semantics. Daily import/export totals, tariffs and forecast-progress views need real HA energy entities and availability rules; do not estimate costs or integrate energy from sparse five-minute power snapshots. Direct inverter control remains outside this monitor's selected scope.

---

## Documentation changes and remaining improvements

Updated `AGENTS.md` with repository discovery, the full check sequence, actual image budgeting, transition/malformed-input coverage, evidence expectations and known limitations. Corrected the development reference's mistaken description of its first command and its unconditional later-outage portal claim. Added this report to the roadmap and an `Unreleased` changelog entry. Historical release and hardware records remain intact. The embedded handbook and firmware sources are unchanged, so F7 remains a recommended recipe correction.

Keep one current roadmap and separate dated audit/release records. For corrective PRs, link the finding ID and add the relevant acceptance evidence. The next handbook pass should address F7 and explicitly distinguish known upstream reporting time from transport freshness. A concise source/data diagram and a generated settings-field reference would help maintenance once those are drawn from a stable contract; avoid another manually maintained duplicate of the existing detailed API reference.

Release notes should continue to distinguish source preparation, publication and physical acceptance. Complete narrow/mobile browser inspection and keyboard/settings validation alongside device tests. Retain the existing hardware checklist rather than turning desktop successes into checked hardware cases.

---

## Validation and acceptance limits

The following checks passed on 7 November 2026:

- `python3 tools/check_host.py` – all five portable suites, all three installed-header integration harnesses, shared renderer compilation, 22 SVG previews and thirteen optional PNG previews. Generated tracked images remained unchanged.
- `python3 tools/check_docs.py` – local Markdown/HTML targets, handbook anchors, structure, preview dimensions and version identity; repeated after the documentation patch.
- `node tests/test_web_history.js` and `node tests/test_web_settings.js` – existing embedded history/settings checks. The local Node runtime was `26.13.0`; CI remains configured for Node 22.
- Extracted dashboard script passed `node --check`; the handbook has no inline script. All eight Python tools parsed successfully.
- `pio run -e cyd28` through the existing PlatformIO virtual environment – incremental build and a separate fresh build both passed. The fresh build compiled application, framework and libraries and generated the app image, merged image and part map. The undefined TFT_eSPI `TOUCH_CS` warning remains expected.
- Targeted review probes reproduced F1 memory errors, F2 retained values, F3 enabled flow after rejection and F5 portal state. Sanitizer failures are expected findings, not passing regression tests. These probes used extracted production functions and host stand-ins.

The normal build reused cached outputs. The separate fresh build used a temporary build directory and temporary PlatformIO core state pointing to the already installed toolchains, after the default global cache lock proved unwritable in the sandbox. It needed no dependency upgrade or approval escalation.

Before release, retain the open cases in [hardware validation](HARDWARE_VALIDATION.md) and [installer checks](WEB_INSTALLER.md): both OTA slots and update methods, invalid/interrupted uploads, settings/calibration retention, actual LittleFS power interruption, prolonged heap/latency behaviour, TLS and Wi-Fi recovery, Improv, touch, and night/DST boundaries. No physical or live-system acceptance was performed by this audit.

Suggested implementation order: fix F1 with permanent sanitizer regressions; fix F2/F3 together with dashboard transition tests; bound HTTP bodies and reclaim flash; resolve F5/F6 and helper availability; then close F8 and execute the hardware/installer acceptance plan. Feature expansion follows those corrections and a measured flash budget.
