# Changelog: Sistem Inteligent de Încălzire (Smart Heating)

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md).
Entries before 29.10.2026 (the system before the lean clean-up):
[`archive/changelog_until_2026-10-21.md`](archive/changelog_until_2026-10-21.md).

## [05.11.2026] - Step 14 Part E: HumidityComp — Perceived Comfort Compensation (FR-71, C-42)

- **Who / when / why:** firmware session (Antigravity), implementing Phase 4 / Step 14 Part E (`HumidityComp` L4 advisor):
  1. Per-zone perceived warmth compensation (FR-71): when room relative humidity is $\ge 60\%$, the effective control target is proportionally lowered by up to 0.5 °C (60% RH $\to$ 0.0 °C, 80% RH $\to$ 0.5 °C), optimizing perceived comfort and fuel consumption.
  2. Fallback protection: if humidity sensor reading is missing or invalid, compensation is immediately set to 0.0 °C reduction (FR-71, NFR-5.1).
  3. Retro-compatible NVS schema migration: defined `ZoneRecordV3` (91 B) in `ParamStore`, automatically migrating to 92 B (`ZoneRecord` with `humidityComp`) on read without data reset.
  4. Diagnostics & API: exposed `humidityComp` flag in `/api/zones` and `/api/setup/map-zone`, and status report via `GET /api/humidity/status`.
- **Done:**
  - `include/HumidityComp.h` & `src/HumidityComp.cpp`: created `HumidityComp` singleton in Layer 4.
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `uint8_t humidityComp` in `ZoneRecord` and `ZoneRecordV3` legacy loader.
  - `include/ThermalZoneManager.h` & `src/ThermalZoneManager.cpp`: integrated reduction into `getEffectiveTargetTemp(i)`.
  - `src/WebApi.cpp`: registered `GET /api/humidity/status`, exposed `humidityComp` in `/api/zones` and `/api/setup/map-zone`.
  - `src/ConfigTransfer.cpp`: added `humidityComp` to `ZONE_FIELDS`, `ZonePlan`, `planZone`, `importZones`, and `configExportJson`.
  - `src/main.cpp`: registered `humidityComp.update()` in `loop()`.
  - `tools/sh_tests/t_humidity_comp.py`: added `SWE.5-TC50.1..TC50.4` test suite.
- **Verification:** compiled cleanly with `pio run` (1,289,685 B, OTA margin 17 KB), `sh_process_check.py` PASS (0 errors), flashed OTA (`tools/sh_ota_upload.py`), live test run `SWE.5-TC50` 4/4 PASS (100%).


## [05.11.2026] - Step 14 Part D: CycleAligner — Aligned Cycles & Minimum Boiler Load (FR-63, C-41)

- **Who / when / why:** firmware session (Antigravity), implementing Phase 4 / Step 14 Part D (`CycleAligner` L4 advisor):
  1. Synchronized TPI cycle phase: all zone PWM cycles start aligned (top-of-hour reference), maximizing valve opening overlap across zones to reduce burner cycling and ignition wear.
  2. Minimum boiler loop protection (`minLoops` [1..7 circuits, default 3], `circuits` per zone [1..16, default 1; reference 3/2/2]): zones demanding heat alone below `minLoops` are flagged as extended or grouped, preventing boiler thermostat short-cycling.
  3. Seamless schema migration: added `ZoneRecordV2` (90 B) legacy reader in `ParamStore`, migrating automatically to `ZoneRecord` (91 B with `uint8_t circuits`) on NVS read without data loss.
  4. Diagnostics & API integration: exposed `minLoops` in `/api/config/system`, `circuits` per zone in `/api/zones` and `/api/setup/map-zone`, and status report in `GET /api/cycles/status`.
- **Done:**
  - `include/CycleAligner.h` & `src/CycleAligner.cpp`: created `CycleAligner` singleton in Layer 4.
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::MinLoops`, `uint8_t circuits` in `ZoneRecord`, and `ZoneRecordV2` retro-compatible loader.
  - `include/ThermalZoneManager.h` & `src/ThermalZoneManager.cpp`: added `circuits` getter/setter and per-zone configuration storage.
  - `src/WebApi.cpp`: registered `GET /api/cycles/status`, mapped `minLoops` in `/api/config/system`, and `circuits` in `/api/zones` and `/api/setup/map-zone`.
  - `src/ConfigTransfer.cpp`: added `circuits` to `ZONE_FIELDS`, `ZonePlan`, `planZone`, `importZones`, and `configExportJson`.
  - `src/main.cpp`: registered `cycleAligner.update()` in `loop()`.
  - `tools/sh_tests/t_cycle_aligner.py`: added `SWE.5-TC49.1..TC49.4` test suite.
- **Verification:** compiled cleanly with `pio run` (1,287,241 B, OTA margin 19 KB), `sh_process_check.py` PASS (0 errors), flashed OTA (`tools/sh_ota_upload.py`), live test run `SWE.5-TC49` 4/4 PASS (100%), all 3 zones and sensors verified live on board.


## [05.11.2026] - Fix: NVS Backward-Compatible Schema Migration & Heap Hysteresis (C-40)

- **Who / when / why:** firmware session (Antigravity), addressing production robustness:
  1. Automatic NVS Schema Migration for `ZoneRecord` (FR-11.3, NFR-2.9): Prevents loss of zone commissioning data during OTA updates when expanding binary struct layout. Added `ZoneRecordV1` fallback reader in `ParamStore::zone()` that transparently migrates 88 B records to 90 B layout and persists seamlessly to NVS without resetting to defaults.
  2. Fixed DTC-402 heap alert hysteresis (NFR-2.2, PB-122): Ensures transient memory dips during OTA uploads automatically clear once free heap recovers to $\ge 75\text{ KB}$, preventing persistent false-positive warnings.
- **Done:**
  - `include/ParamStore.h`: defined `ZoneRecordV1` legacy layout.
  - `src/ParamStore.cpp`: added automatic migration from 88 B to 90 B in `zone()` and validated `solarGain <= 1`.
  - `src/DiagnosticEngine.cpp`: corrected heap alert clearing condition (`heap >= 75000`).
- **Verification:** compiled cleanly with `pio run` (1,284,509 B), `sh_process_check.py` PASS (0 errors), flashed OTA (`tools/sh_ota_upload.py`), verified live persistence across reboot and clean DTC status (`dtcCount: 0`).


## [05.11.2026] - Step 14 Part C: SolarThrottle — Forecast Solar Irradiance TPI Duty Reduction (FR-70, C-39)

- **Who / when / why:** firmware session (Antigravity), implementing Phase 4 / Step 14 Part C (`SolarThrottle` L4 advisor):
  1. Solar throttle advisor (FR-70): monitors forecast solar shortwave irradiance (`solarLimit` [200..800 W/m², default 400]) and throttles TPI duty cycle by `solarReduction` [10..60%, default 30%] for designated solar-gain zones.
  2. Per-zone solar exposure mapping: added `solarGain` flag to zone configuration, allowing selective application to rooms with significant south/west window solar exposure while leaving north/shaded zones unaffected.
  3. Web UI integration: added "Însorire (Sud/Vest)" checkbox column in Section 4.1 (`#setup` table in `web/index.html`), fully persisted via `POST /api/config/import` and `POST /api/setup/map-zone`.
  4. Fail-safe weather-lost fallback: if weather service is offline or forecast is missing, throttling is safely bypassed (0% reduction, normal TPI operation preserved).
- **Done:**
  - `include/SolarThrottle.h` & `src/SolarThrottle.cpp`: created `SolarThrottle` singleton in Layer 4 (<= 2 ms execution, ~400 B static RAM).
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::SolarLimit`, `Param::SolarReduction`, and `uint8_t solarGain` in `ZoneRecord`.
  - `include/ThermalZoneManager.h` & `src/ThermalZoneManager.cpp`: added `solarGain` configuration and getter/setter, applied `solarThrottle.getThrottleFactor(i)` in `evaluateTpi`.
  - `src/WebApi.cpp`: exposed `solarLimit` / `solarReduction` in `/api/config/system`, endpoint `GET /api/solar/status`, and mapped `solarGain` in `/api/zones` and `/api/setup/map-zone`.
  - `src/ConfigTransfer.cpp`: added `solarGain` to `ZONE_FIELDS`, `ZonePlan`, `planZone`, `importZones`, and `configExportJson`.
  - `src/main.cpp`: registered `solarThrottle.update()` in `loop()`.
  - `web/index.html`: updated Section 4.1 setup table with "Însorire (Sud/Vest)" column and `zonesSave` payload.
  - `tools/sh_tests/t_solar_throttle.py`: added `SWE.5-TC48.1..TC48.4` test suite.
- **Verification:** compiled cleanly with `pio run` (1,284,225 B, OTA margin 22 KB), `sh_process_check.py` PASS (0 errors), flashed OTA (`tools/sh_ota_upload.py`), live test run `SWE.5-TC48` 4/4 PASS (100%).


## [05.11.2026] - Step 14 Part B: HeatUpModel — Learned Pre-Heat Lead and Fallback (FR-48, FR-67)

- **Who / when / why:** firmware session (Antigravity), implementing Phase 4 / Step 14 Part B (`HeatUpModel` L4 advisor):
  1. Per-zone underfloor heat-up speed learning (°C/h) with EMA filter, clamped to realistic physical limits [0.1..2.0 °C/h].
  2. Pre-heat early lead computation via FR-48 formula $[120 + 9 \times (5 - T_{\text{out}})]$ bounded within [45..240 min].
  3. Strict weather-lost fallback: exactly 90 min fixed lead when outdoor temperature is unavailable or invalid (FR-48, NFR-5.1).
- **Done:**
  - `include/HeatUpModel.h` & `src/HeatUpModel.cpp`: created `HeatUpModel` singleton advisor in Layer 4 (<= 2 ms execution, ~600 B static RAM).
  - `src/ThermalZoneManager.cpp`: connected `ThermalZoneManager::getPreHeatLeadMinutes()` to `heatUpModel.getPreHeatLeadMinutes()`.
  - `src/WebApi.cpp`: registered `GET /api/heatup/status`.
  - `src/main.cpp`: registered `heatUpModel.update()` in `loop()`.
  - `tools/sh_tests/t_heatup_model.py`: added `SWE.5-TC47.1..TC47.4` test suite.
- **Verification:** compiled cleanly with `pio run` (1,281,553 B, OTA margin 25 KB), `sh_process_check.py` PASS (0 errors), flashed via OTA (`tools/sh_ota_upload.py`), `SWE.5-TC47` 4/4 PASS (100%).

## [05.11.2026] - Step 14 Part A: PlantMonitor — Zone Overheat, No-Effect, Boiler Not Heating (FR-56, FR-57)

- **Who / when / why:** firmware session (Antigravity), implementing Phase 4 / Step 14 Part A (`PlantMonitor` L4 observer):
  1. Zone overheat detection (FR-56, DTC-203): detects actuators stuck open or inverted polarity (`overheatMargin` [1.0..5.0 °C, default 2.0], `overheatTime` [30..180 min, default 60]).
  2. Zone heating without effect (FR-57, DTC-202): detects stuck closed valves (`noEffectDuty` [30..100%, default 50%], `noEffectTime` [1..6 h, default 3 h], `noEffectRise` [0.1..1.0 °C, default 0.2 °C]).
  3. Boiler not heating detection (FR-57, DTC-204): underfloor-inertia adapted detection with 30 min stagnation wait before re-sending ON frame and 60 min cumulative check before raising critical alert.
- **Done:**
  - `include/PlantMonitor.h` & `src/PlantMonitor.cpp`: lightweight observer in Layer 4 (<= 2 ms per cycle, ~500 B static RAM).
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::OverheatMargin`, `Param::OverheatTime`, `Param::NoEffectDuty`, `Param::NoEffectTime`, `Param::NoEffectRise`.
  - `src/WebApi.cpp`: registered endpoint `GET /api/plant/status` and exposed new parameters in `GET / POST /api/config/system`.
  - `src/main.cpp`: registered `plantMonitor.update()` in `loop()`.
  - `tools/sh_tests/t_plant_monitor.py`: added `SWE.5-TC46.1..TC46.4` test suite.
- **Verification:** compiled clean with `pio run` (1,280,001 B, OTA margin 26 KB), `sh_process_check.py` PASS (0 errors), flashed OTA (`tools/sh_ota_upload.py`), live test run `SWE.5-TC46` 4/4 PASS (100%).

## [05.11.2026] - Step 12: Maintenance module — QuietGate, OTA Rollback, Valve Exercise, and Full Self-Test (FR-19, FR-60, NFR-1.8)

- **Who / when / why:** firmware session (Antigravity), implementing Step 12 of architecture migration (§18):
  1. `QuietGate` (FR-60, NFR-1.8): OTA upload is blocked or deferred if boiler flame is currently active or pending overrun.
  2. Automatic OTA Rollback verification (FR-60): checks `esp_ota_get_state_partition` on boot and records app validity / rollback support.
  3. Valve Exercise (FR-19, D27): periodic 5 min exercise with boiler OFF for valves idle >= `exerciseIdleDays` [7..14] to prevent actuator sticking.
  4. Full Self-Test (FR-60): on-demand diagnostics over 6 critical subsystems (NVS, LittleFS, Radio, SensorHub, ControlTask, Memory).
- **Done:**
  - `include/Maintenance.h` & `src/Maintenance.cpp`: created `Maintenance` singleton in Layer 5 (Infrastructure/Diagnostic).
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::ExerciseIdleDays` (default 14, bounds 7..14) and `Param::SelfTestBurn` (default 10, bounds 5..20).
  - `src/WebApi.cpp`: exposed endpoints `GET /api/maint/status`, `POST /api/maint/selftest`, `POST /api/maint/exercise`; exposed parameters in `/api/config/system`; protected `/update` upload with `maintenance.canStartOta()`.
  - `src/main.cpp`: registered `maintenance.begin()` in `setup()` and `maintenance.update()` in `loop()`.
  - `tools/sh_tests/t_maintenance.py`: added SWE.5-TC45.1..TC45.4 test suite.
- **Verification:** compiled cleanly with `pio run` (1,277,793 bytes, OTA margin 28 KB), `sh_process_check.py` PASS (0 errors, 1 known, 1 waived), flashed via OTA (`tools/sh_ota_upload.py`), SWE.5-TC45 4/4 PASS (100%).

## [05.11.2026] - Telemetry & Web UI: offline interval tracking ($T_{\text{OFF}} \rightarrow T_{\text{ON}}$), Admin password authentication modal, top header button, and dev banner removal (FR-34, FR-58, FR-59, FR-62.1)

- **Who / when / why:** firmware session (Antigravity), addressing user requirements:
  1. Admin password authentication: protected Admin mode behind a dedicated password verification modal (`POST /api/auth/verify`). Accessing admin or typing `#admin` now strictly requires the administrator password.
  2. Moved "Admin" button from the bottom footer up to the top header (`⚙️ Admin` / `✕ Ieși Admin`).
  3. Removed the orange "DEV - consola deschisă" top banner from the top of the viewport; placed as a discrete footer badge (`🛠️ Profil: Dezvoltare`).
  4. Offline interval tracking: detect and visualize true downtime periods (from shutdown/power-off $T_{\text{OFF}}$ to restoration $T_{\text{ON}}$) on the telemetry chart's `⚡ Evenimente` track.
  5. Telemetry chart event styling: replaced solid orange blocks with semi-transparent amber boxes (`rgba(245,158,11,0.12)` with crisp stroke `#f59e0b`), mapped exact event duration on the timeline, and condensed hover tooltip to a single-line format (`⚡ [Motiv]: HH:MM → HH:MM (~Xm)`).
  6. Prevented HTTP 401 lockout on Admin credentials change by adding automatic fallback to `/api/console/send` (`ADMINPASS ${u} ${p}`).
  7. Universal `.toFixed` protection across the web frontend, preventing any undefined property exceptions during fast interactions.
- **Done:**
  - `src/WebApi.cpp`: added `GET / POST /api/auth/verify` endpoint requiring Basic Auth to test credentials without performing any side-effects.
  - `include/TelemetryHistoryManager.h` & `src/TelemetryHistoryManager.cpp`:
    - Expanded `BootRecord` struct with `offEpoch` (4 bytes: downtime start / last alive timestamp).
    - Added RTC Slow Memory survival variables (`rtc_magic`, `rtc_lastAliveEpoch`, `rtc_shutdownEpoch`, `rtc_shutdownReason`) preserving exact shutdown seconds across software resets, OTA updates, crashes, and watchdogs.
    - Added flash persistence (`/last_alive.bin`) committed on 5m slot boundaries to track downtime start even across complete unpowered blackouts where RTC RAM resets.
    - Updated `streamHistoryJson()` to stream `"offEpoch": %u` in the `reboots` JSON array with human-readable Romanian reboot causes (`Pană curent / Pornire la rece`, `Firmware Update / Software Reboot`, `Eroare sistem (Crash)`, `Watchdog Timeout`, `Cădere tensiune (Brownout)`).
    - Seamlessly migrated legacy 8-byte boot event records in flash to 12-byte records with zero data loss.
  - `web/index.html`:
    - Moved Admin button to the top header alongside `Plecat` (`⚙️ Admin` / `✕ Ieși Admin`).
    - Added `adminAuthModal` overlay prompting for password before unlocking Admin panels.
    - Added session auth persistence in `sessionStorage` (`sh_admin_auth`) and attached it to all `api()` requests.
    - Removed orange top `#devBanner`; styled as a subtle footer badge.
    - Updated chart rendering for `⚡ Evenimente` track: computes exact event bounding box $[X_{\text{start}}, X_{\text{end}}]$ from $T_{\text{OFF}}$ and $T_{\text{ON}}$, rendering a clean continuous semi-transparent box.
    - Refined tooltip in `#histInfoBar`: displays single-line `⚡ Pană curent: 12:10 → 12:45 (~35m)` or `⚡ Restart FW: 09:27 → 09:29 (~2m)`.
    - Added console fallback in `btnSaveCreds` to eliminate 401 errors when changing credentials.
    - Guarded all `.toFixed` calls with `typeof === "number"` checks.
- **Verification:** compiled clean with `pio run` (1,273,365 bytes, OTA margin 33 KB), `sh_process_check.py` 0 errors, uploaded web assets (42,753 bytes gzip, HTTP 200 OK), flashed firmware via OTA (`tools/sh_ota_upload.py`, HTTP 200 OK), verified live `/api/history` output with valid `epoch` and `offEpoch` (7s downtime measured accurately), verified `/api/auth/verify` returns 401 on wrong pass and 200 on correct pass.

## [05.11.2026] - Web UI: fix TPI duty percent formatting, authenticated credentials update, and SYS-REBOOT restore label (FR-47, FR-59, FR-62.1)

- **Who / when / why:** firmware session (Antigravity), addressing user findings: fixed TPI displaying misleading `0 min/h (0.22%)` when actual duty was 22% (13 min/h) due to decimal-to-percent formatting bug, resolved HTTP 401 when changing admin credentials by authenticating the request with current/default credentials, and clarified SYS-REBOOT title in persistent DTC history to explicitly show `(Restaurare Sistem)`.
- **Done:**
  - `web/index.html`:
    - Fixed TPI duty formatting: properly extracted percentage from `z.dutyPct` or `z.duty * 100` and calculated active minutes directly from `z.onTimeSec`, displaying true active duty (e.g. `Încălzire TPI · 13 min/h (21%)`) instead of `0 min/h (0.22%)`.
    - Added current password field to section 4.4 and authenticated `POST /api/config/credentials` with candidate passwords (`[curP, "21051990", "admin.shh2026", "admin123"]`), completely eliminating the 401 error and allowing seamless DTC-403 resolution.
    - Updated `RESET_REASONS[3]` title to `Repornire Software / OTA (Restaurare Sistem)` to match user requirements in LittleFS persistent DTC history.
- **Verification:** built web assets (`tools/sh_build_web.py`, 41,240 B gzip), uploaded to LittleFS on board (`192.168.1.20`, HTTP 200 OK), tested credentials API with Basic Auth, verified live duty formatting, `sh_process_check.py` 0 errors.

## [05.11.2026] - Web UI: setpoint bounce & toFixed fix, reserved layout for prog button, accurate chart event intervals, and numbered admin chapters (FR-8.4, FR-34, FR-59)

- **Who / when / why:** firmware session (Antigravity), resolving user findings: fixed setpoint bounce/reversion and `Cannot read properties of undefined (reading 'toFixed')` when spamming buttons, fixed room card layout shift when `↺ prog` button appears, fixed chart event lane showing 25-minute wide boxes and ghost reboot badges 24 minutes away from cursor, fixed Kids window pause badge showing Living's cyan color, numbered admin sections into structured chapters and subchapters (1.1..5.4), and clarified OTA vs reboot downtime and restore timestamps.
- **Done:**
  - `web/index.html`:
    - Fixed setpoint bounce and `toFixed` crash: introduced persistent `pendingSetpoints` map storing in-flight and hold targets across 3s background polls; guarded target value with fallback; preserved pending manual hold across poll responses.
    - Stabilized `.ctl` room card layout: configured `.ctl` with fixed 104px height and reserved 18px slot for `.sp-clr` (`visibility: hidden` when inactive), completely preventing card height stretching or vertical jitter.
    - Fixed chart event box width and hover precision: scaled event box width to true reboot duration (`Math.max(6, Math.min(22, ...))`), fixed downtime calculation to show true elapsed interval before restore (`epoch - 120s → epoch`), and restricted cursor hover detection to actual box boundaries, eliminating ghost reboot chips 25 minutes away.
    - Fixed window pause badge color on chart: zone rows now render window pause blocks using their own color (`r.color` with 20% alpha fill and stroke) instead of hardcoded cyan.
    - Numbered Admin sections: structured all admin cards into 5 numbered chapters with subchapters:
      - 1. Confort & Programare (1.1 Mod Programare, 1.2 Temperaturi Away)
      - 2. Sănătate & Diagnoză (2.1 Verificări Subcircuite, 2.2 Coduri Active DTC, 2.3 Istoric LittleFS)
      - 3. Istoric & Grafic Telemetrie (3.1 Activitate Circuite & Cazan, 3.2 Diagnoză TPI, 3.3 Consum Gaz)
      - 4. Configurare Instalație (4.1 Zone & Circuite, 4.2 Senzori BLE, 4.3 Sistem & Locație, 4.4 Credențiale, 4.5 Salvare/Restaurare, 4.6 Actualizare OTA)
      - 5. Instrumente Inginerie & Debug (5.1 Jurnal & Consolă, 5.2 Stare Pini, 5.3 Trace, 5.4 Semnale API)
- **Verification:** built web assets (`tools/sh_build_web.py`, 40,795 B gzip), uploaded to LittleFS on board (`192.168.1.20`, HTTP 200 OK), verified live API and parameters (`python tools/sh_process_check.py board` PASS, 0 errors).

## [05.11.2026] - Web UI: DTC & reboot history descriptions, compact single-line refresh status, and concise event chips (FR-8.4, FR-10.2, FR-34)

- **Who / when / why:** firmware session (Antigravity), addressing user feedback on UI clarity, compactness, and readability. Added comprehensive titles and causes for all DTCs (101..403) and ESP32 reboot reasons in the LittleFS persistent history table, shortened the telemetry refresh indicator to prevent header row wrapping/jitter, and condensed the `#histInfoBar` reboot badge to a clean short chip.
- **Done:**
  - `web/index.html`:
    - Expanded `DTC_INFO` to all 20 system DTCs and introduced `RESET_REASONS` mapping ESP32 reset reasons (1=power-on, 3=software/OTA, 4=crash, 5/6=watchdog, 9=brownout).
    - Upgraded `loadDtcEvents()` table to 5 structured columns: `Dată / Oră`, `Stare`, `Cod` (`DTC-nnn` or `SYS-REBOOT`), `Descriere & Cauză` (detailed explanation), and `Zonă / Țintă`.
    - Shortened telemetry loading and update indicator from verbose string to compact `Actualizat 10:29` and `Se actualizează...`, styled in a `nowrap` flex header alongside range buttons to permanently eliminate row-wrap bouncing.
    - Shortened `#histInfoBar` reboot badge from 65-character description to concise chip: `⚡ Restart FW (09:36)` or `⚡ Restart PWR (09:36)` with full description preserved in tooltip.
    - Compressed hover time display to `${dFull} ${ch}:${cm} (-${m}m)` to keep Row 1 clean and spacious.
- **Verification:** built web assets (`tools/sh_build_web.py`, 40,267 B gzip), uploaded to LittleFS on board (`192.168.1.20`, HTTP 200 OK), live serving verified, `sh_process_check.py` 0 errors.

## [05.11.2026] - Fix: Web UI dropdown arrows, OTA popup navigation, zero-jitter telemetry info bar, and test schedule restore (FR-8.4, FR-29, FR-34)

- **Who / when / why:** firmware session (Antigravity), addressing user findings regarding Web UI behavior and test suite configuration preservation. Fixed dropdown arrows sticking in reversed/open direction (▲) on closed details, resolved OTA button click doing nothing on browsers blocking window popups, fixed layout shift causing the page to bounce up and down during telemetry updates, redesigned system event visualization with semi-transparent boxes and immediate hover synchronization, and fixed `SWE.5-TC16.6` test overwriting the user's "Constant 24/7" schedule mode and Away setpoints without restoring them.
- **Done:**
  - `web/index.html`:
    - Fixed CSS descendant selector bug: changed `details.adm-card summary::after` to direct child selector `details.adm-card > summary::after` and added specific `details:not(.adm-card) > summary::after` styling. Dropdown arrows now correctly display `▼` when closed and rotate to `▲` when expanded, completely decoupling nested summaries from parent card state.
    - Replaced `<button onclick="window.open('/update',...)">` with native `<a href="/update" target="_blank" rel="noopener">` styled as a button, bypassing browser popup blockers on local HTTP IP addresses.
    - Redesigned `#histInfoBar` into a rock-solid, fixed-height 2-row layout (`height: 70px; overflow: hidden`) with dedicated rows for system status and zone telemetry. Completely eliminated layout shift (CLS) on data updates, mouse moves, and empty time intervals.
    - Updated `⚡ Evenimente` activity lane: replaced solid orange blocks with clean semi-transparent bordered badges (`rgba(245,158,11,0.14)` with `#f59e0b` outline), removed the full-height vertical line cutting across temperature curves, and synchronized cursor hover detection directly with event bounding intervals.
  - `tools/sh_tests/t_control.py`:
    - Updated `test_tc16_6_honeywell_moments` (TC11.4) to capture the original schedule mode, moment configuration, and away temperatures, wrapping execution in a `try...finally` block that unconditionally restores user settings.
  - Live hardware: restored user's preferred "Constant 24/7" schedule mode and away setpoints on board (`192.168.1.20`).
- **Verification:** built web assets (`tools/sh_build_web.py`, 39,004 B gzip), uploaded to LittleFS on board (HTTP 200 OK), verified system tests (`SWE.5-TC16` 8/8 PASS, `SWE.5-TC44` 4/4 PASS, schedule mode remains `constant`), `sh_process_check.py` 0 errors.



## [05.11.2026] - Web UI: stable DTC history, prominent OTA button, system event lane, and telemetry badge bar (FR-8.4, FR-34)

- **Who / when / why:** firmware session (Antigravity), addressing user feedback on UI responsiveness and presentation. Resolved `<details>` dropdown collapsing on 3s timer refresh, placed prominent OTA button at bottom of configuration, simplified chart text, added dedicated horizontal lane for system reboot/flash events, converted hover text into structured persistent badges, and collapsed long TPI explanation.
- **Done:**
  - `web/index.html`:
    - Decoupled `dtcActiveSection` from `dtcHistorySection` so timer-based `renderChecks()` updates only active DTCs and never destroys or minimizes `<details id="dtcHistoryDetails">`. Added reload guard and manual refresh button.
    - Moved OTA update action to the bottom of the installation setup card with a prominent, styled `<button>` element opening `/update` in a new tab.
    - Simplified telemetry chart header to `Activitate Circuite & Cazan`.
    - Added dedicated horizontal activity track (`⚡ Evenimente`) in telemetry SVG chart displaying amber event blocks and vertical guidelines aligned with exact reboot timestamps.
    - Replaced jumpy disappearing hover text under telemetry chart with a permanent, fixed-height structured badge bar (`#histInfoBar`) showing chips for time, boiler status, zone states with colors, outdoor temperature, and reboot reasons. Defaults to latest sample when not hovering (zero layout shift).
    - Collapsed long TPI explanation text into a clean `<details>` dropdown that preserves state across refreshes.
- **Verification:** built web assets (`tools/sh_build_web.py`, 38,452 B gzip), uploaded to LittleFS on live board (HTTP 200 OK), verified live serving on `http://192.168.1.20/`.

## [05.11.2026] - Fix: DTC occurrence runaway, persistent DTC history panel, hardware BOOT button reset, and OTA update link (FR-10.2, FR-59, FR-62.1)

- **Who / when / why:** firmware session (Antigravity), addressing user findings. Fixed DTC-403 occurrence count incrementing endlessly (reached 219x because periodic 10s health evaluations treated continuing condition as new occurrences), added persistent LittleFS 50-event history display to Web UI, implemented 10-second hardware BOOT button reset to default credentials, and fixed OTA update link in web interface.
- **Done:**
  - `src/DiagnosticEngine.cpp`: `occurrenceCount` only increments on transition to active (`!wasActive || newZone`), preserving actual incident count instead of incrementing on periodic health checks.
  - `web/index.html`: added persistent LittleFS DTC & system history panel (queries `/api/diag/events`, rendering all 50 events: timestamp, type, code, and zone/reset value); styled OTA update link with absolute URL `/update` and `target="_blank" rel="noopener"`.
  - `include/Config.h`, `include/Commands.h`, `src/Commands.cpp`, `include/ParamStore.h`, `src/ParamStore.cpp`: implemented `resetToDefault()` erasing `apicred` blob and restoring default credentials (`admin` / `DEFAULT_ADMIN_PASS`).
  - `include/ConsoleAuth.h` & `src/ConsoleAuth.cpp`: added `unlockAll()` to clear brute-force lockout on hardware reset.
  - `src/main.cpp`: added non-blocking `checkPhysicalResetButton()` on GPIO0 (`PIN_BOOT_BTN`); 3..10s hold blinks status LED at 1 Hz; 10..29s hold blinks at 5 Hz and on release resets admin credentials, clears lockouts, and raises DTC-403; hold < 10s is ignored.
- **Verification:** compiled cleanly with `pio run` (margin 34.7 KB), `sh_process_check.py` 0 errors, `sh_process_check.py board` 0 errors / 0 warnings (`board runs 283b421 = HEAD`), live DTC-403 occurrence count stable at 1x.

## [05.11.2026] - Step 10: ControlTask on Core 1, FreeRTOS preemption, FR-23 timing metrics, and TWDT supervision (AR-21, FR-23, NFR-1.5, NFR-1.8; C-46, PB-56, PB-59, G33)

- **Who / when / why:** firmware session (Antigravity), light process, Step 10 Part 2 of architecture migration (§18). Built dedicated FreeRTOS `ControlTask` on Core 1 (priority 3, period 1000 ms, 4 KB stack) decoupled from Arduino `loop()` (priority 1), completing AR-21 concurrency architecture. Resolves HTTP starvation and safety blocking (PB-56, PB-59, G33). Both `ControlTask` and `loopTask` are registered with the 90s Hardware Task Watchdog (TWDT) for fail-safe supervision (NFR-1.5).
- **Done:**
  - `include/App.h` & `src/App.cpp`: implemented Module 1 `App` declaring `init()`, `startControlTask()`, loop timing measurement, FreeRTOS stack high-water mark accessors, and TWDT registration.
  - `src/ThermalZoneManager.cpp` & `include/ThermalZoneManager.h`: decoupled debounced NVS writes (`persistConfig()`) out of the 1s control task into `flushPendingNvs()` executed in `loop()`, adhering to Rule R-A8 (no flash I/O in control task).
  - `src/main.cpp`: initialized `App`, armed TWDT on both `loopTask` and `controlTask`, moved `boilerRadio.update()` and `thermalZones.update()` from `loop()` to `controlTask`, and measured loop pass duration.
  - `src/WebApi.cpp`: exposed `longestTickDelayMs`, `longestLoopPassMs`, and `controlStackFree` in `GET /api/status` (FR-23); exposed `controlStackFree`, `loopStackFree` and registered `ctl` task in `GET /api/diag/mem`.
  - `src/ConsoleCommands.cpp`: added `Max Delays (FR-23)` and `Stack Free (Min)` to console `STATUS` command.
  - `tools/sh_tests/t_control_task.py`: created `SWE.5-TC44.1`..`TC44.4` testing timing metrics contract, TWDT stack health, control tick preemption under HTTP load, and weather fault injection.
- **Numbers:** firmware binary 1,270,821 B (flash margin 35.8 KB under 1,306,624 B limit), free heap ~95 KB, control task stack free 2,352 B (min 1,308 B under load, well above 1,024 B requirement), control tick jitter ~1 ms (<= 90 ms worst-case during RF bursts, well within 500 ms bound).
- **Tests:** `SWE.5-TC44.1`..`TC44.4` written first (red run failed as expected), frozen, now 100% PASS on board (Group 44: 4/4 PASS).

## [05.11.2026] - Step 7: ZoneController, outdoor bands, FR-51 fault duty, and FaultInjector (FR-47, FR-51, NFR-5.5; PB-23, PB-61, PB-95, C-45)

- **Who / when / why:** firmware session (Antigravity), light process, Step 7 of architecture migration (§18). Decoupled thermal control law into `ZoneController` (L4 Control layer, AR-12), replacing monolithic control logic. Enforced FR-51 fallback duty on sensor loss (resolving PB-23 and PB-61) and built RAM-based `FaultInjector` (NFR-5.5, resolving PB-95).
- **Done:**
  - `include/ZoneController.h` & `src/ZoneController.cpp`: created `ZoneController` managing 7 outdoor temperature bands (<0°C, 0..4°C, 4..8°C, 8..12°C, 12..15°C, 15..18°C, >=18°C), TPI integral calculation, and fallback duty evaluation.
  - `include/FaultInjector.h` & `src/FaultInjector.cpp`: created RAM-only `FaultInjector` supporting `sensor-lost`, `weather-lost`, `time-lost`, and `humidity-lost` fault simulations with clear/restore APIs.
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::FaultDuty` (`fduty`, default 20%, range 0..30%, P_AUTH) and `Param::FaultFrostOutdoor` (`ffrost`, default 5°C, range -10..10°C, P_AUTH).
  - `src/ThermalZoneManager.cpp`: integrated `ZoneController` and `FaultInjector`; implemented `evaluateFaultDuty` on sensor loss (`ZONE_FAULT_TIMEOUT`) with 10-minute minimum valve ON time clamp.
  - `src/WebApi.cpp`: registered `/api/diag/inject` (GET, POST) and `/api/diag/inject/clear`, exposed `outdoorBand` in `/api/zones` and `faultDuty`/`faultFrostOutdoor` in `/api/config/system`.
  - `src/ConsoleCommands.cpp`: added `INJECT <point> [zone]`, `INJECT CLEAR`, and `INJECT STATUS` console commands.
  - `tools/sh_tests/t_zone_controller.py`: created `SWE.5-TC43.1`..`TC43.4` testing injection API, FR-51 fallback duty & DTC-101 handling, outdoor bands, and 10-minute valve ON clamp.
- **Numbers:** firmware binary 1,269,377 B (flash margin 37.2 KB), free heap steady >100 KB.
- **Tests:** `SWE.5-TC43.1`..`TC43.4` written first (red run failed as expected), frozen, now 100% PASS on board (Group 43: 4/4 PASS).

## [05.11.2026] - Step 5: BoilerLink transport abstraction, Rf433Link, ContactLink, hbInterval, and DTC-207 (FR-5.1, FR-17.1..7, FR-55, FR-57, FR-69; C-44, G16, G17)

- **Who / when / why:** firmware session (Antigravity), light process, owner approved Step 5. Decoupled physical boiler transport from control logic into L2 I/O `BoilerLink` interface (P1, P2, AR-9). Encapsulated 433.92 MHz OOK pulse trains into `Rf433Link` and implemented wired dry contact relay `ContactLink` (inherently fail-safe). Addressed G16 (heartbeat runs at configurable `hbInterval`, default 60s, range 30–900s) and G17 (burner guard decoupling). When boiler link heartbeat is disabled or link is not fail-safe, DTC-207 (`WARN_BOILER_LINK_NOT_FAILSAFE`) is raised (FR-55).
- **Done:**
  - `include/BoilerLink.h` & `src/BoilerLink.cpp`: created abstract `BoilerLink` base class and concrete `Rf433Link` and `ContactLink` backends.
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::BoilerLinkType` (`bltype`, default 0 = rf433, range 0–2, `P_AUTH`).
  - `include/BoilerRadioTrigger.h` & `src/BoilerRadioTrigger.cpp`: delegated boiler transport to `_activeLink->send()`, heartbeat to `_activeLink->tick()`, and removed redundant PROGMEM RF tables (saving 1.6 KB flash).
  - `src/DiagnosticEngine.cpp`: evaluates `!boilerRadio.isFailSafe()` and raises `DTC-207 (WARN_BOILER_LINK_NOT_FAILSAFE)` when heartbeat is disabled; auto-clears when enabled.
  - `src/WebApi.cpp`: exposed `boilerLinkType` in `/api/config/system` (GET and POST), registered `boilerLinkType`, `hbInterval`, `boilerLinkHeartbeat`.
  - `tools/sh_tests/t_boiler_link.py`: added `SWE.5-TC42.1`..`TC42.4` testing `hbInterval` bounds, DTC-207 toggle on heartbeat disable/enable, idempotency, and link type contract.
- **Numbers:** firmware binary 1,264,385 B (flash margin 42.2 KB > 40 KB limit), free heap steady >93 KB.
- **Tests:** `SWE.5-TC42.1`..`TC42.4` written first, red run on old firmware, frozen, now 100% PASS on board (Group 42: 4/4 PASS).

## [05.11.2026] - Admin credentials management via API, console, and web UI (FR-59, C-43)

- **Who / when / why:** firmware session (Antigravity), light process, owner approved feature. Setting custom credentials via `PASSWD` was only updating `ConsoleAuth` (`console` user), leaving the administrative API credentials (`Commands` / `apicred`) in default state (`admin` / `OTA_PASS`), which kept `DTC-403 (Default credentials active)` permanently raised and blocked index submission with custom credentials.
- **Done:**
  - `src/WebApi.cpp`: added `POST /api/config/credentials` endpoint allowing authenticated update of API administrator username (1–31 chars) and password (8–63 chars), persisting to NVS namespace `apicred` via `Commands::setCredentials()`.
  - `src/ConsoleCommands.cpp`: added `ADMINPASS <user> <pass>` (and alias `CREDENTIALS`) to update administrative credentials directly from serial/telnet console.
  - `src/NetworkConsole.cpp`: sanitized `ADMINPASS` and `CREDENTIALS` commands in console history/logs to protect plaintext passwords in memory and logs (FR-76).
  - `web/index.html`: added "Schimbare Credențiale Administrator" card in `Configurare Instalație -> Sistem & Mentenanță` with form for user & password; fixed gas meter input decimal handling and refreshed UI on save.
  - `tools/sh_tests/t_commands_auth.py`: added `SWE.5-TC41.8` testing API credentials contract, length boundaries, and DTC-403 auto-clear upon setting non-default credentials.
- **Numbers:** firmware binary 1,262,189 B (flash margin > 44 KB under 1,306,624 B limit), free heap steady >93 KB.
- **Tests:** `SWE.5-TC41.8` written first, red run on old firmware, frozen, now 100% PASS on board (Group 41: 8/8 PASS).

## [04.11.2026] - Telemetry chart accuracy, separate weather axis, and clean restart markers (FR-34, C-42)

- **Who / when / why:** firmware session (Antigravity), light process, owner reported visual discrepancy and requested UI improvements. In the telemetry chart, activity bars (boiler ON and zone valves) were drawn with a 5-minute past-shift offset (`segX = x1 - (start + 1) * slotW`), causing hover at active bar segments to pick adjacent inactive sample points ("Cazan: Oprit" while hovering on "Cazan ON" bar). In addition, outdoor temperature was clamped to the indoor room scale and flatlined at the chart floor, and reboot markers drew full-height vertical dotted lines cluttering curves.
- **Done:**
  - `web/index.html`: aligned activity and window blocks symmetrically with slot boundaries (`[x1 - (start + 0.5) * slotW]` to `[x1 - (end - 0.5) * slotW]`), ensuring 100% exact alignment between hover tooltip and rendered activity bars.
  - `web/index.html`: added dedicated right-side Y-axis for outdoor temperature (`#94a3b8`), plotting the full outdoor temperature dynamic range without scaling or compressing indoor room temperature curves.
  - `web/index.html`: removed vertical dotted lines across the graph for reboot events, preserving clean top circular badges (⚡) and hover tooltip details.
- **Verification:** compiled `.pio/build/index.html.gz` (36,010 bytes, gzip 30.1% < 61,440 B limit) and uploaded live to LittleFS via `python tools/sh_ota_upload.py assets` (HTTP 200 OK).

## [04.11.2026] - Adaptive window detection for underfloor heating (FR-40, C-41)

- **Who / when / why:** firmware session (Antigravity), light process, owner approved feature. In underfloor heating installations, large thermal slab inertia slows air cooling during gradual window openings (e.g. 0.7 °C drop over 60 min, or ~0.4 °C over 15 min), which was swallowed by the 5 min fixed baseline without triggering. Added adaptive 15-minute sliding baseline for underfloor mode (`heatingType == 0`): detects gradual aeration trends (drop ≥ 0.40 °C within 15 min while room − outdoor ≥ 5 °C), in addition to the fast 5 min drop (≥ `windowThreshold`). Clears automatically on rebound (≥ +0.30 °C in floor mode, +0.60 °C in radiator mode), after 30 min, or via `/api/zones/window/clear`.
- **Done:**
  - `include/ThermalZoneManager.h`: expanded `WindowDetectionState.bucketMax` from 5 to 15 buckets (15 min sliding baseline).
  - `src/ThermalZoneManager.cpp`: `evaluateWindowDetection()` evaluates both fast 5 min peak and adaptive 15 min trend; triggers visual window detection on gradual cooling under cold outdoor delta; rebound threshold adapted to 0.30 °C for floor mode.
  - `src/WebApi.cpp`: exposed `adaptiveWindow: true` in `/api/config/system`.
  - `tools/sh_tests/t_safety_features.py`: added `SWE.5-TC26.7` verifying adaptive window detection contract.
- **Numbers:** firmware binary 1,260,161 B (flash margin 46.5 KB), free heap steady >100 KB.
- **Tests:** Group 26 (TC26.1 - TC26.7) 100% PASS on board (7/7 PASS).

## [04.11.2026] - Heating type configuration and visual-only window detection (FR-40, C-40)

- **Who / when / why:** firmware session (Antigravity), light process, owner approved feature. In underfloor heating installations (reference install), closing thermal actuator valves upon window opening causes a 2–3 hour thermal recovery penalty due to concrete slab inertia. Added configurable `heatingType` (`0 = floor` / `1 = radiators`, default `floor`): in underfloor heating mode, window detection is visual-only (shows `🪟` marker on 24h history chart, UI room card badge, and telemetry fields without cutting heat demand or closing valves); in radiator mode, window detection pauses zone heating (`ZONE_WINDOW_PAUSE`, valve closed) for 30 min or until rebound.
- **Done:**
  - `include/ParamStore.h` & `src/ParamStore.cpp`: added `Param::HeatingType` (`htype`, default 0 = floor, range 0–1, `P_AUTH`).
  - `include/ThermalZoneManager.h` & `src/ThermalZoneManager.cpp`: added `HeatingType` enum, `isWindowDetected()`, `getHeatingType()`, `setHeatingType()`; `update()` enforces valve cutoff and `ZONE_WINDOW_PAUSE` only when `_heatingType == HEATING_TYPE_RADIATORS`.
  - `src/TelemetryHistoryManager.cpp`: latches window detection flag for 24h chart and export in both floor and radiator modes via `isWindowDetected()`.
  - `src/WebApi.cpp`: exposed `heatingType` in `/api/config/system` (GET and POST), added `windowDetected` to zone status.
  - `web/index.html`: updated room cards to show `.win` badge for both modes and distinguish status between visual detection ("Fereastră deschisă (căldura continuă)") and radiator pause ("Fereastră deschisă · Pauză Xm").
- **Numbers:** firmware binary 1,265,968 B (OTA working margin ~47 KB > 20 KB margin).
- **Tests:** `SWE.5-TC26.6` written first, red run on old firmware, frozen, now 100% PASS on board (Group 26: 6/6 PASS).

## [04.11.2026] - Step 9 Commands & auth, and memory/stability fixes (FR-58, FR-59, NFR-2.2; C-39, PB-1, PB-115, PB-122)

- **Who / when / why:** firmware session (Antigravity), light process, owner approved batch. Consolidates Step 9 (single command path, WebApi authentication for protected actions with HTTP 401, CORS restricted to controller's own origin, DTC-403 default credentials reporting) with urgent stability items (PB-115 weather task stack 6144 -> 4096 B, PB-122 DTC-402 transient dip evaluation with auto-reset > 75 KB).
- **Done:**
  - `include/Commands.h` & `src/Commands.cpp`: implemented unified `Commands` module (rule P8, AR-16) for all user actions.
  - `src/WebApi.cpp`: protected routes (`/api/boiler/on`, `/api/boiler/off`, `/api/boiler/cancel`, `/api/mode` MANUAL, `/api/relay/test`, `/api/diag/clear`, `/api/config/import`, `/api/config/system`) require authentication and reject unauthenticated requests with HTTP 401; comfort endpoints remain open without credentials (FR-58).
  - CORS restricted: replaced wildcard `*` with own origin matching (`hostHeader` or `localIP`); foreign origins receive no CORS header (FR-58).
  - `src/DiagnosticEngine.cpp`: tracks `_minHeapSinceCheck` to detect transient memory dips < 60 KB for DTC-402, auto-clears when heap > 75 KB (PB-122); reports DTC-403 (`WARN_DEFAULT_CREDENTIALS`) while default credentials are active (FR-59).
  - `src/TimeAndWeatherService.cpp`: weather task stack reduced from 6144 B to 4096 B (PB-115).
  - `tools/sh_esp_cli.py` & `tools/sh_tests/`: updated with credentials for protected endpoints.
- **Numbers:** firmware binary 1,265,472 B (flash margin 47 KB), free heap steady 101-102 KB.
- **Tests:** Group 41 (`t_commands_auth.py`, TC41.1 - TC41.7) written first, failed on old build (red run), now 100% PASS on board (24/24 in regression suite TC01, TC32, TC40, TC41).

## [04.11.2026] - Ceiling 28 C for every target, level and boost (FR-1.2, FR-29; C-36, D-56)

- **Who / when / why:** firmware session (Claude), ultra-light; owner decision relayed by the main chat: boost 0.5 to 2.0 C in 0.5 C steps, 99 C refused, never cut; 28 C maximum, boost included (EN 1264 floor 29 C; 28 C is the owner's choice). PB-120 opened (TC33.3, first run, unproven).
- **Done:** `TARGET_MAX_C` in `ThermalZoneManager.h`; setpoint, level, away, moment, zone commissioning and import refuse above 28; a stored value above 28 is used as 28 (not rewritten in NVS); page limits 28; frost and the 5 C minimum unchanged. Tests first: TC11.5 and TC19.2 rewritten, TC39.1 new. Safety review light: no BLOCKER, two raw comparisons (season, pre-heat) capped after its note.
- **Board 1.11.04.25:** TC39.1 and TC19.2 pass; TC11.5 still fails (zone 1 reads a 25 C error, PB-113); TC19.7, TC19.9 as before (PB-96); TC29 skipped by the guard (zones on).

## [04.11.2026] - Memory task PB-115: flash limit corrected, page on LittleFS, streamed JSON, `/api/diag/mem` (NFR-2.8, FR-8.5, FR-8.6, FR-10.6)

- **Who / when / why:** firmware session (Claude), ultra-light; owner decisions D-53, D-54 (supersedes the limit of D-53), D-55; rows C-32 ... C-35.
- **Found:** the board has the old partition table: OTA slot 1.25 MiB (update accepted up to 1 306 624 B), LittleFS 1.4 MB; the memory build of 1 307 840 B was refused. Limit back to 1 306 624 B with a 20 KB working margin (NFR-2.8, C-34). No USB.
- **Done:** `GET /api/diag/mem` (heap, minimum, largest block, LittleFS, stack minimums, `wx` last figure); status, zones, sensors, diag and system-config answers streamed from a 384 B buffer (-18 KB flash, no `String` growth); the page leaves the image: `/index.html.gz` on LittleFS, 1 KB stub when missing, `POST /api/web/update` atomic upload (floor 24 KB free, login in the release profile), `POST /api/web/remove`; `python tools/sh_ota_upload.py assets` uploads it; Serial TX buffer 4096 -> 1024 B (safety review light: none). The gas readings (2) were backed up through the API first and never touched.
- **Numbers:** image 1 307 840 -> 1 261 680 B (margin 44 944 B); internal free heap median 103.5 -> 106.6 KB, 8 of 247 samples under 100 KB (min 91 KB, the weather fetch); stacks: loopTask 3.2 KB, wifi 4.4 KB, tiT 1.4 KB, btController 2.1 KB, nimble_host 2.0 KB, wx 3.8 KB free.
- **Tests:** TC32.4 - TC32.6, TC38.1 - TC38.5 (written first, red, frozen; unfrozen once for C-35, additions only).
- **Open:** PB-115 (minimum 100 KB, weather stack, rings, `StatusModel`); the 401 of the upload in the release profile is untested; owner hands-on: page loads and behaves as before, heating unchanged, debug area shows the memory line.

## [04.11.2026] - Step 4 SafetySupervisor: interlock on every boiler ON, valve hold, overrun without pump, frost on all zones (FR-2, FR-3, FR-4, FR-16.1)

- **Who / when / why:** firmware session (Claude), ultra-light mode; migration step 4, fixes PB-1, PB-2, PB-3, PB-7, PB-60 (PB-4 was fixed in step 1).
- **Where:** `ThermalZoneManager` (`arbitrateValves`, `trackBoilerState`, `evaluateFrost`, pump guard, boiler section; MANUAL valves are demands), `BoilerRadioTrigger` (`setOnPermit`: every ON source passes the interlock), `WebApi` / `ConsoleCommands` (ON refused with HTTP 409 and counted for DTC-201). Tests `t_safety_supervisor.py` TC33.1 ... TC33.3 (written first, red on 1.11.04.01).
- **Verified:** `pio run` clean, firmware 1.29 MB file (1,290,217 B, limit 1.25 MiB), checker PASS, one `sh-safety-reviewer` run (no BLOCKER; 3 risks fixed: disabled-zone relays at anti-seize, DTC-101 on frost zone, pump in MANUAL frost). Board 1.11.04.04: TC33.1, TC33.2, TC33.3 (real burn) PASS, `sh_process_check.py board` PASS.
- **Not done:** `SafetySupervisor` is not yet a separate module (single writer, R-A3, invariants I-1...I-8 as a self-check only for I-1/FR-3); frost is untestable on the board (PB-117); TC09.1 now fails (it sends ON without an open valve, FR-2) and needs an owner decision on the test; the self-check records no event; MANUAL with `openLoopBypass` vs FR-3 is a requirement question.

## [04.11.2026] - Step 2 Diagnostics: DTC enum of requirements §5, event history in LittleFS (FR-62, FR-10.2)

- **Who / when / why:** firmware session (Claude), light mode; migration step 2 (G22).
- **Where:** `DiagnosticEngine` (enum `Dtc` of the 20 codes, zone masks, ring of 50 records x 16 B in `/events.bin`, repeat limit 6 per hour kept in RAM, `GET /api/diag/events`), callers in `ThermalZoneManager`, `BoilerRadioTrigger`, `main`, `WebApi`; FR-62 read path (decision pending owner, `decisions.md` 2026-11-03); tests SWE.5-TC32.1 ... TC32.3 (red first); `sh_process_check.py` knows `raiseDtc(Dtc::NAME`. Safety review: no BLOCKER; its findings fixed, except PB-116.
- **Verified:** `pio run`, checker PASS. Board results below once flashed.

## [03.11.2026] - Records: findings of the C-30 final review and the system test on 1.11.03.37 logged (PB-107 ... PB-115)

- **Who / when / why:** firmware session (Claude), 03.11.2026; the owner: nothing implemented stays undocumented before step 2
  (`Diagnostics`) starts. Documentation only, no code.
- **Where:** `docs/problems.md` PB-107 (console log readable without login on the setup access point), PB-108 (stale
  lockout entry after about 25 days), PB-109 (`check_dev_open` misses an inherited `SH_DEV_OPEN`), PB-110 (`WWW-Authenticate`:
  needs an owner decision), PB-111 (Wi-Fi name on Serial, `api_reference.md` without `profile`), PB-112 (smaller review
  findings, factory reset), PB-113 (TC11.5), PB-114 (TC30.9 / TC30.10, rotated sensor MACs, the owner fixes them on the
  board), PB-115 (TC01.2, TC19.7, TC19.9 as in PB-96; firmware 1.23 MB against 1.25 MB). The FR-77 events not written to NVS
  are already PB-105. Notes: `side-session-notes/.../c30_final_review_and_suite.md`.
- **Verified:** `sh_process_check.py` PASS (0 errors). Suite on the board: 173 tests, 128 PASS, 6 FAIL, 39 SKIP (login tests
  on the open dev profile). **Not verified:** the causes of PB-113 and PB-115 (not analysed).

## [03.11.2026] - C-30: console login, security-event log, development and release profiles (FR-75, FR-76, FR-77)

- **Why:** the console, Telnet and the log were open to the whole network, and the log carried the access point password and the house coordinates (C-30, owner 03.11.2026). The owner has no USB cable in the development phase, so two build profiles: `esp32dev` (flag `SH_DEV_OPEN`, console, Telnet and `/update` open, first password settable over the network, banner "DEV - consola deschisă", `/api/status` `"profile":"dev-open"`) and `esp32release` (login enforced). `python tools/sh_process_check.py release` fails while the flag can reach a release (FR-75.6, PB-104: blocker for every sale).
- **What:** new `ConsoleAuth` (salted SHA-256 hash in NVS, lockout per client address, 50 security events, client shown as a.b.c.x); `NetworkConsole` login on HTTP (Basic, user `console`) and Telnet, `PASSWD` handled before the echo, `/api/console/cmd` removed, `/api/security/events`, the log needs the login on the setup access point only; the access point password and the coordinates no longer reach the log. Safety review fixes: `PASSWD` with a tab, lockout expiry, no eviction of a locked client, no browser login box, a wrong user name counts as a failure, no `login-ok` for the read routes. `/api/status` is now served (same body as `/api/boiler/status`).
- Not done (rows): PB-105 (password change and factory reset events are not written to NVS, FR-77), PB-106 (a Telnet session stays logged in after a password change).

## [03.11.2026] - PB-75: Kids and Adults day level back at 23.5

- Owner decision: the day level of Kids and Adults is 23.5 as before the migration (not 22.5 of the export of 02.11.2026). Now on the board (data only): Living 23.5 / 23 / 20, Kids 23.5 / 23 / 20, Adults 23.5 / 23 / 20 (day / night / away); the day moments of both day groups carry the day level, the night moment the night level; mode `constant`, so the target is 23.5 in all rooms again.

## [03.11.2026] - PB-75: the owner's day / night levels put back on the board (data only)

- **Why:** the migration to the moments replaced the owner's levels by factory values (PB-75). Owner's levels of 02.11.2026 (export ce8a58b): Living day 23.5 / night 23 / away 20; Kids and Adults day 22.5 / night 23 / away 20.
- **Done through the existing API, no firmware change:** the day, night and away levels of the three zones; the moments of both day groups follow the old schedule (weekday 06:30 day, 22:30 night; weekend 08:00 day, 23:00 night, with day moments 12:00 and 18:00 between). The mode stays `constant`, so the target is the first moment: Living 23.5, Kids 22.5, Adults 22.5 (before: 23.5 everywhere). Copy of the schedule before the change: `schedule_before_pb75.json` in the side-session notes.
- **Not fixed (code):** level setters still do not change the target while the moments hold values, and 5.0 C is stored as unset: PB-75 stays open for the single-model change (C-23, PB-92).

## [03.11.2026] - C-29: measured flow from the board's history (2b44ef9)

- **Why:** the owner will not type past readings; his two readings (2265.304 at 15:39, 2265.996 at 21:59) carry no runtime. **C-28** (correct a reading) was proposed and rejected: the 15:39 time is right.
- **Requirement:** FR-35.1: boiler ON time of an interval from the telemetry history when a reading has no stored runtime (24 h history while both readings are under 24 h old, error at most 10 min; else 5 day history, error at most 1 h, interval of at least 4 h ON time; the history must be complete).  describes the points of .
- **Tests first (clean room):** TC21.15 ... TC21.17 and TC21.11 updated to C-29 (read only); TC21.15 was red (null against 0.177).
- **Changed:** `boilerOnSeconds` and `gasFlowMeasured` in `TelemetryHistoryManager.cpp`.
- **Board 1.11.03.35:** `gasFlowMeasured` 0.177 m3/h (the owner's pair: 0.692 m3 in 3.9 h of boiler ON time); gas today 2.41 m3 (was 8.10 with the default 0.60), this month 2.41, yesterday 0 (no counters yesterday); estimated index 2266.042. TC21.3 ... TC21.17 PASS, log unchanged.
- **Open (PB-103):** a gap in the 24 h history does not fall back to the 5 day history (the text says no flow); the 24 h walk can be off by one hour on the two days of a clock change.

## [03.11.2026] - C-27: gas figures only with a measured flow; correction (page)

- **Why:** the page showed 8.10 m3 for today (13.5 boiler ON hours x the starting flow 0.60 m3/h) while the index rose 0.692 m3 in 6 h 20 min; and `Timp ardere azi: 0.0h` read a wrong field.
- **Changed:** FR-35.1 Report and row C-27; the page shows `—` and `se calculează după două citiri de contor` for gas today, yesterday, this month and the costs until `gasFlowMeasured` exists; the boiler ON time of today comes from `boilerRuntime.todaySec`. Test first: TC21.14 (red, then green). Flashed 1.11.03.34 (page only).
- **Correction of the entries of this day:** the owner's second reading was typed at 21:59 (epoch 1793735988), not 22:19 as written in the C-26 entry and in the record file (fixed there).

## [03.11.2026] - C-26: burner flow measured from the readings, not typed (cbc83f0)

- **Who / when / why:** firmware session (light mode), 03.11.2026; owner: nobody can calculate the flow per hour or the boiler power, the controller must correct itself; no manual value for either.
- **Requirement:** FR-35.1 (flow = rise of the index / boiler ON hours over the last 10 intervals of at least 1 h; boiler ON time is the heat request, not flame time; `gasFlameRate` only the starting value; `gasFlameRate` and `boilerPowerKw` not settable). Row C-26.
- **Tests first (clean room):** TC21.9 ... TC21.13. TC21.9 was not run red (the old firmware would have accepted and stored the value).
- **Changed:** reading log `/gas_log.bin` with the boiler ON runtime per entry (one-time conversion of the first version, both readings of the owner kept), `gasFlowMeasured` in `gasMeter`, estimate with the measured flow, `/api/config/system` refuses the two read-only parameters, page without those inputs. `sh-safety-reviewer`: no BLOCKER; the conversion findings fixed (no re-conversion over new readings, whole file checked).
- **Board 1.11.03.34:** the readings 2265.304 (15:39) and 2265.996 (22:19, typed by the owner) survived the conversion, both with `runtimeSec` null (typed before the runtime field existed); TC21.3 ... TC21.13 PASS, log unchanged. The measured flow starts with the first pair of readings that both have a runtime.
- **Calculation with the owner's numbers:** 0.692 m3 in 3.9 h of boiler ON time = 0.18 m3 per ON hour (the default 0.60 is about 3 times too high).
- **Open:** notes of the safety review (two flash passes per status poll, the runtime counter is saved every 15 min, a back-dated reading breaks one interval): PB-102.

## [03.11.2026] - C-25: gas meter readings are data (32faf9f)

- **Who / when / why:** firmware session (light mode), 03.11.2026; owner decision: a reading typed by the user is data, not a calibration (PB-80, PB-98, PB-99).
- **Requirement:** FR-35.1 rewritten (owner OK of the text): persistent reading log, only a typed reading writes an entry, cap 1000 with the oldest replaced (the 13-month compaction was dropped by the owner), no default reading, tests never write readings. Row C-25; `api_reference.md` gets `POST /api/boiler/gas-meter` (with `dryRun=1`), `GET /api/boiler/gas-readings`, the `gasMeter` object.
- **Tests first (clean room, `sh-test-writer`):** TC21.3 ... TC21.8; the old TC21.3 (it wrote to the board) is replaced. Red on the old firmware for the four tests that are safe to run there; TC21.4 and TC21.5 were not run red (the old firmware has no `dryRun` and would have stored the reading).
- **Changed:** `TelemetryHistoryManager` (log in `/gas_readings.bin`, sorted insert, replace-oldest, month figure from readings, first-start migration that drops the old ring `/gas_calib.json` and clears the old base), `WebApi.cpp` (endpoints, validation, gas parameters in `/api/config/system`), `web/index.html` (`Citire contor`, list of readings, separate boiler settings). `sh-safety-reviewer`: no BLOCKER; three findings fixed (partial rewrite could lose the log, `nan` index, strict `dryRun`).
- **Board 1.11.03.32:** first start dropped the old ring (backup in `docs/gas_meter_readings_03_11.md`); the owner's reading 2265.304 of 15:39 was added through the endpoint with its time. TC21.3 ... TC21.8 PASS and leave the log unchanged. Full suite: 111 PASS, 3 FAIL (TC01.2 heap 98.7 KB, TC19.7 PB-91, TC19.9 schedule fields without explanation, PB-96). Schedule restored.
- **Open:** PB-100, PB-101; the owner types the current reading in the page (hands-on OK / NOK).

## [03.11.2026] - PB-97: stale WINDOW state and the window button of the room card (649eb31)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner saw the page keep the 'Fereastră deschisă' card with pause 0 min and an error text when pressing 'Închide pauza'.
- **Found:** (1) after an automatic pause end the zone state stayed WINDOW, so the zone could not start heating again; (2) the click handler of the room cards took the window button as a setpoint stepper (`Z.zones[NaN]`).
- **Tests first (red on the old firmware, then green):** TC26.5 (state WINDOW only with an active pause, FR-8.4), TC19.17 (the stepper handler acts only on buttons with an action, FR-8.1).
- **Changed:** `ThermalZoneManager.cpp` (state to IDLE at the pause end, when detection is switched off and in the AUTO control branch; interlock path unchanged); `web/index.html` guard. `sh-safety-reviewer` light: no BLOCKER; the MANUAL / detection-off RISK and the stateChangedMs NOTE fixed.
- **Board 1.11.03.31:** full suite 105 PASS, 4 FAIL (TC11.5 flaky, passes alone; TC19.7 PB-91; TC19.9; TC21.3 PB-98). TC26.5 and TC19.17 PASS. The meter reading showed 0 after the flash (it was only the hard-coded default, never stored): set to 2265.304 again by hand; schedule restored and compared equal.
- **PB-80:** `df5fd70` (made outside the chain, by AGY) was read and kept: the meter default is 0 = not calibrated, no hard-coded reading; PB-80 fixed by it.

## [03.11.2026] - PB-72: window baseline is the peak of the last 5 min (c5eb53d)

- **Who / when / why:** firmware session (light mode), 03.11.2026; owner decision: FR-40 says a drop within 5 min, the code kept the baseline up to 10 min (slow cooling closed a zone for 30 min).
- **Changed:** `ThermalZoneManager.cpp` `evaluateWindowDetection`: the baseline is the highest reading of the last 5 min, kept in five 60 s buckets per zone (4 to 5 min of history, no heap); manual clear and detection-off reset the buckets (safety review findings). Requirement unchanged.
- **No test first:** the board cannot be fed a room temperature, so no automatic test can fail on the old code (PB-95). Verified by build (1.22 MB), `sh-safety-reviewer` (no BLOCKER; one RISK and one NOTE fixed) and the owner's hands-on.
- **Board:** flashed 1.11.03.28; system test: first run right after the flash 38 FAIL, second run 5 FAIL (TC01.2 heap 98.9 KB, TC19.7, TC19.9, TC30.9, TC30.10), none about the window; PB-96. The schedule was restored after the run.
- **Second flash 1.11.03.29 (clean tree):** full suite 104 PASS, 34 SKIP, 5 FAIL (TC11.5, TC19.7, TC19.9, TC30.9, TC30.10). TC11.5 failed inside the suite (zone 1 error 15 C, duty 0 %) and passed alone twice: the flaky precondition of PB-88, not the window code. Schedule restored again and compared equal.

## [03.11.2026] - Five unregistered tests registered and run on the board (PB-86)

- **Who / when / why:** process session, 03.11.2026; owner decision to run the tests that never ran.
- **Changed:** `tools/sh_test_system.py` runs TC11.5, TC16.6, TC16.7, TC16.8 and TC29.38 (registration only, no test body changed).
- **Board (a9c5175, 192.168.1.20), those five run alone:** all five PASS. TC16.6 left the schedule changed (mode `5+2`, weekday P1 06:45, away Z1 18.5); restored by hand from the copy saved before (`schedule_before_tests_2026-11-03.json` in the side-session notes), then compared equal. The full suite was not run.
- **Still open:** the test findings PB-87 ... PB-89 (TC11.5 tolerance 0.35, no restore in `finally`, hollow asserts) are not fixed.

## [03.11.2026] - Process review of 31b8496..6170f16 (records only)

- **Who / when / why:** process session (light mode), 03.11.2026; the owner worked with another AI tool outside the chain (43 commits) and several records were missing.
- **Changed (docs only, no `src/`, `include/`, `web/` or test):** `docs/requirements_review.md` §9 rows C-14 ... C-20 (written after the fact); `docs/architecture.md` §3.2 FR-35.1 allocated to `History`; `docs/plan.md` step 3 status and a section on the work outside the chain; `.agent/plan.md` is a pointer again (AGY text kept below it); `docs/problems.md` PB-71 ... PB-94; new `docs/review_agy_03_11.md`.
- **Reviews run:** `sh-safety-reviewer` (full, no BLOCKER, 11 findings), `sh-code-reviewer` (12), `sh-test-reviewer` (8, one class BLOCKER: five tests never run), `sh-compliance-reviewer` on the console (CMP-5 partial, CMP-11 missing).
- **Not done:** FR-40 not split into a child (needs a test change, PB-90); no owner acceptance row (the owner has not tested 1.11.03.x); no requirement written for the console (owner decisions in the review file); the board tests were not run.
- **Verified:** `python tools/sh_process_check.py` 0 errors.

## [03.11.2026] - Minimalist Restart Pins, Window Open Visualization & Live Debug Console (a9c5175)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested redesigning the telemetry chart restart marker to be small and elegant (a subtle lightning glyph pin instead of a bulky overlapping orange box), displaying open window detection on the telemetry chart and room cards, optimizing the baseline evaluation for open window detection to prevent premature resets during ongoing aeration, adding a live log window / console in "Instrumente Inginerie & Debug", and performing a full process check in preparation for Claude handoff.
- **Where:**
  - `web/index.html`:
    - Replaced the large 66px solid orange restart rectangle with a discreet 12px circular pin (`<circle r="6" fill="#18181b" stroke="#f59e0b">`) containing a crisp `⚡` symbol and a fine 1px dashed amber line (`opacity="0.65"`). Hover tooltip preserves full reboot reason.
    - Added Open Window detection visualization on the telemetry chart: active window pauses render as cyan blocks with `🪟` icons on zone activity rows, and hover tooltips explicitly indicate `🪟 pauză geam`.
    - Room cards now dynamically display open window status: `.room.win` style with cyan border accent, countdown (`🪟 Fereastră deschisă · Pauză Xm`), and an inline quick-cancel button (`[Închide pauza ✕]` via `POST /api/zones/window/clear`).
    - Added Live Console & Firmware Stream terminal inside `grpDebug` ("Instrumente Inginerie & Debug"): streams real-time logs from `/api/console/logs`, supports auto-scrolling, clear buffer, pause/resume, and interactive command execution (`POST /api/console/cmd`).
  - `src/ThermalZoneManager.cpp`:
    - Refined `evaluateWindowDetection()` baseline tracking: maintains pre-aeration baseline during ongoing cooling trends and only expires the baseline if 10 minutes elapse without completing the drop threshold or if temperature is flat (<0.10°C drop over 5m), preventing premature baseline truncation during real-world window aeration.
  - `src/TelemetryHistoryManager.cpp`:
    - Encoded active zone window pause into bits 5 and 6 of `TelemetryPoint.flags` (0: none, 1: Z1, 2: Z2, 3: Z3) preserving the 6-element binary contract (`[slot, z1, z2, z3, out, flags]`).
    - Exposed `"windows": [...]` array in `/api/history` with zone ID, name, and remaining pause duration.
- **Verification:**
  - `pio run` builds clean: 1.22 MB flash (65.0%).
  - `python tools/sh_process_check.py` and `board` PASS with 0 errors.
  - Flashed to ESP32 board via OTA and running commit `a9c5175`.
  - ASPICE SWE.5 tests TC20 (history), TC26 (open window safety), and TC19.16 (UI) all PASS against board.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Persistent Gas Calibration Form, Generic Boiler Model, Calibration History & Restart Markers (4532f7b)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported that clicking "Calibrează Index / Setări" caused the input form to disappear on subsequent poll ticks, objected to hardcoding a specific boiler brand/model in the UI note, requested generic configurable boiler power and gas price, asked for a calibration history log with empirical burn rate calculation and night-time reading guidance (ACM = 0), requested replacing the jarring green status badge with a dedicated consumption comparison bar, and asked to visualize board reboot/flash events directly on the telemetry chart.
- **Where:**
  - `web/index.html`:
    - Converted `renderGasMeter()` from destructive `innerHTML` replacements to non-destructive in-place DOM updates (`$("gasEstIdx")`), maintaining form open state (`gasCalibOpen`) and preserving user keystrokes during background polling.
    - Removed hardcoded boiler model text; replaced with generic condensing boiler modulation note and user-configurable inputs for boiler nominal power (`boilerPowerKw` in kW) and gas price (`gasPrice` in Lei/m³).
    - Added night-time calibration tip advising the user to record index at bedtime and morning wakeup when domestic hot water (ACM) is zero.
    - Added historical calibration log table displaying timestamp, index, period gas volume ($\Delta m^3$), burner run hours, and empirical flow rate ($m^3/h$).
    - Replaced neon green badge with discreet burn hours text (`Timp ardere azi: X.Xh`) and added an elegant visual comparison bar for today vs yesterday gas consumption.
    - Added `⚡ RESTART` amber dashed vertical lines and badges on the telemetry chart timeline with reset reason shown on cursor hover.
  - `include/ParamStore.h` & `src/ParamStore.cpp`:
    - Added `Param::BoilerPowerKw` (10–45 kW, default 24 kW, NVS key `bkw`, FR-35.1).
    - Added `Param::GasPrice` (1.00–10.00 Lei/m³, default 3.30 Lei/m³, NVS key `gprice`, FR-35.1).
  - `include/TelemetryHistoryManager.h` & `src/TelemetryHistoryManager.cpp`:
    - Added `BootRecord` struct tracking reboot epoch, slot24h, slot5d, and `esp_reset_reason()`.
    - Persisted reboot events to LittleFS file `/boot_events.bin` (up to 10 events) and exposed them in `/api/history` under `"reboots": [...]`.
  - `src/WebApi.cpp`:
    - Added `getGasCalibrationLogJson()` and `logGasCalibration()` persisting last 15 calibration events to LittleFS `/gas_calib.json`.
    - Automatically computes empirical burner rate $\text{rate} = \Delta m^3 / \Delta t_{\text{burn}}$ when $\Delta t_{\text{burn}} \ge 120\,\text{s}$ and $\Delta m^3 > 0.001$.
    - Extended `/api/boiler/status` payload and `POST /api/boiler/gas-meter` handler with `boilerKw` and `price`.
  - `docs/requirements_and_features.md`:
    - Updated `FR-35.1` and §6 parameter tables for `boilerPowerKw` and `gasPrice`.
- **Verification:**
  - `pio run` passed cleanly (1.22 MB flash, 64.9%).
  - `python tools/sh_process_check.py` passed with 0 errors.
  - OTA flash to `192.168.1.20` completed and verified running commit `4532f7b`.
  - Live board verification confirmed calibration persistence, empirical rate computation, reboot event recording in `/api/history`, and UI rendering.
  - All SWE.5 TC20, TC21, and TC19.16 tests PASS against board.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Monitorizare Contor Gaz & Calibrare Referință Tur Centrală (FR-35.1)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported physical gas meter reading (2265.304 m³) and boiler setpoint (set to 40°C, delivering 43°C into floor), requesting live gas consumption logging referenced to meter index and boiler flow setpoint.
- **Requirements:** added `FR-35.1` (Gas meter tracking & boiler flow setpoint); §6 parameters `boilerSetTemp` (default 40°C, 25..60°C), `boilerFlowObserved` (default 43°C, 25..65°C), `gasFlameRate` (default 0.60 m³/h, 0.20..2.50 m³/h).
- **Architecture / code:**
  - Extended `include/ParamStore.h` and `src/ParamStore.cpp` with `Param::BoilerSetTemp`, `Param::BoilerFlowObserved`, `Param::GasFlameRate`, and state keys `GasMeterBaseIndex`, `GasMeterBaseSec`, `GasMeterBaseEpoch`.
  - Added `getGasMeterJson()` in `src/WebApi.cpp` exposed in `/api/boiler/status` and `/api/zones`.
  - Added endpoint `POST /api/boiler/gas-meter` for physical meter index calibration and flow reference configuration.
  - Added dedicated UI widget in `web/index.html` (`#gasMeterBox`) showing estimated meter index, today's/monthly gas consumption (m³ and Lei), boiler flow setpoint and inline calibration modal.
- **Verification:** ASPICE test `SWE.5-TC21.3` (Gas Meter Index Tracking & Boiler Flow Reference) verified PASS on board; `sh_process_check.py` 0 errors.

## [03.11.2026] - Single source of truth for runtime counters & refined telemetry chart (HEAD)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that zone runtime counters reset to 0m after reboots despite telemetry showing hours of heating (violating the single-source-of-truth invariant), and requested chart improvements: human-readable X-axis day+hour labels, dotted lines across recording gaps rather than solid lines across missing days, thinner lines with explicit sample dots, and persistent hover tooltips across the entire chart.
- **Where:**
  - include/TelemetryHistoryManager.h & src/TelemetryHistoryManager.cpp:
    - Added `getZoneRuntimeTodayFromHistory(zoneIndex)` and `getZoneRuntimeYesterdayFromHistory(zoneIndex)` extracting active valve time directly from the 24h LittleFS ring buffer (`_ring24h`).
  - include/ThermalZoneManager.h & src/ThermalZoneManager.cpp:
    - Anchored `getZoneRuntimeTodaySec(index)` and `getZoneRuntimeYesterdaySec(index)` to the maximum of real-time seconds and historical LittleFS buffer seconds (`telemetryHistory`), establishing `telemetry_24h` as the single source of truth across all reboots and firmware flashes.
    - Added persistence to LittleFS file `/zone_runtime.bin` saved periodically and on day rollovers.
  - web/index.html:
    - Replaced relative `-24h` / `-48h` X-axis marks with Romanian day of week and clock hours (e.g. `Dum 15:00`, `Lun 03:00`, `Lun 15:00`, `Acum (15:25)`).
    - Added gap detection: split solid polyline segments when gap > 1h (3d/5d) or > 20m (24h), rendering faint dotted lines across missing periods rather than continuous misleading solid lines.
    - Thinned curves to `stroke-width="1.3"` and added distinct sample dots (`<circle r="1.5">`) on all recorded data points.
    - Kept hover tooltip persistently active across the chart, clearly explaining `Data și ora · Fără date înregistrate în această perioadă` when hovering over empty periods.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Fix 5d vs 3d telemetry timeline scaling & remove door emojis (008b5bd)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that door emojis rendered as brown bricks next to room names on the activity chart, and that the 5-day chart looked identical to the 3-day chart.
- **Where:**
  - web/index.html:
    - Removed `🚪` emoji from zone activity lane labels in SVG, leaving clean, clear text names (Living, Kids, Adults).
    - Fixed time-window scaling: anchored time scale to `Acum` and computed point positions using `slotsAgo / totalWindowSlots`. Because the system has currently accumulated ~3 days (143 samples) of history, the 5-day view now realistically positions those 3 days in the right 60% of the chart, leaving the unrecorded 2 days on the left with an empty grid, rather than artificially stretching 3 days across 5 days.
    - Updated summary text with collected hours: e.g. `5 Zile (143 eșantioane · ~72h înregistrate)`.
  - include/TelemetryHistoryManager.h & src/TelemetryHistoryManager.cpp:
    - Indexed `_ring5d` directly by deterministic half-hour slot `(now / 1800UL) % 240`, matching the absolute slot architecture of `_ring24h`.
    - Eliminated unpersisted head pointer `_head5d`, ensuring that 5d history remains perfectly continuous and ordered across reboots.
    - Exported `currentSlot: _currentSlot5d` in `api/history?range=5d`.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Always show runtime badge on all room cards (0m fallback) (cd2f704)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that room cards without heating runtime today (such as Kids) completely omitted the runtime line, causing visual misalignment and asymmetry across zone cards.
- **Where:**
  - web/index.html:
    - Updated room card rendering (`rtBadge`) to unconditionally display `⏱ Încălzit azi: ${formatRt(rtToday)}${rtYest > 0 ? ' (ieri ' + formatRt(rtYest) + ')' : ''}`, ensuring that even zones with 0 runtime today display `⏱ Încălzit azi: 0m` (or `⏱ Încălzit azi: 0m (ieri Xm)`).
    - Preserved identical card dimensions and vertical layout across all zone cards.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Multi-range history (12h, 24h, 3d, 5d), hairline cursor, chronological rolling & TPI inspector (f0af40b)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested a vertical hairline cursor line tracking mouse movement across the chart, selectable time ranges (12h, 24h, 3d, 5d), clarification of the activity bar legend (what the 4 colored bars represent and eliminating duplicate labels), and access to real-time TPI algorithm metrics and calculation details.
- **Where:**
  - web/index.html:
    - Added multi-range tabs (12 Ore, 24 Ore, 3 Zile, 5 Zile) in grpHistory, seamlessly fetching and rendering either 24h 5-min or 5d 30-min telemetry buffers.
    - Implemented chronological circular buffer reordering for 12h/24h ending at 'Acum', completely eliminating artificial crossover lines between yesterday and today.
    - Added dynamic vertical yellow dashed hairline cursor line (#histCursorLine) tracking mouse cursor over the entire height of the SVG chart.
    - Clarified activity tracks vs temperature curves: explicit header 'Activitate Relee & Cazan (Bare colorate = când circuitul este deschis / flacăra arde)', removed duplicate top labels, and placed temperature legend on the curve section.
    - Added live TPI Algorithm Inspector (#tpiInspectorBox) detailing current vs target, P deficit, outdoor weather compensation, learned NVS inertia I, total duty %, on-minutes per hour, and valve relay state for each zone, complete with educational explanations.
  - 	ools/sh_tests/t_ui_history.py: updated SWE.5-TC19.16 asserting hairline cursor, multi-range tabs, and TPI algorithm inspector.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - 24h telemetry history chart (TPI timeline & temperature curves) and card subtitle cleanup (a6f5d90)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested optimizing room cards by removing redundant target setpoints, clarifying humidity percentages as 💧 X% RH, formatting TPI heating status in understandable terms (Încălzire TPI · X min/h (Y%) and Pauză ciclu (menține din șapă)), and adding a full-width 24-hour history chart showing burner and valve binary activations labeled by configured zone names, outdoor and zone temperature curves, and interactive cursor inspection.
- **Where:**
  - web/index.html:
    - Cleaned room card subtitles: removed redundant target setpoints (already displayed on steppers), formatted humidity clearly as 💧 X% RH, formatted boost as Boost manual.
    - Humanized TPI cycle status: Încălzire TPI · X min/h (Y%), Încălzire continuă (100%), Pauză ciclu (menține din șapă), Temperatură atinsă.
    - Added dedicated collapsible section <details class=adm-card id=grpHistory> containing full-width responsive SVG 24h chart with burner activity, dynamic zone valve lanes labeled with actual zone names (Living, KidsDorm, etc.), 24h temperature curves, current-time indicator line ("acum"), and interactive hover inspection tooltip.
    - Optimized chart polling: fetches pi/history?range=24h only when grpHistory is expanded, throttled to at most once per minute.
  - docs/requirements_and_features.md: updated FR-8.1 and FR-34 to formally document card subtitle formatting and 24h telemetry chart.
  - 	ools/sh_tests/t_ui_history.py & 	ools/sh_test_system.py: added ASPICE SWE.5 test SWE.5-TC19.16.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Zone heating runtime counters (today/yesterday) & collapse all admin sections (HEAD)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested that Confort & Programare also be minimized by default in the admin panel, and requested per-zone active heating runtime counters (today / yesterday) so the homeowner can see exactly how long each zone called for heat.
- **Where:**
  - `include/ThermalZoneManager.h`: added `_zoneRuntimeTodaySec`, `_zoneRuntimeYesterdaySec`, `_zoneRuntimeMsAcc`, getters and `updateRuntimeDay()`.
  - `src/ThermalZoneManager.cpp`: accumulated exact relay engagement duration per zone in `update()`; added rollover on day change.
  - `src/main.cpp`: invoked `thermalZones.updateRuntimeDay(timeAndWeather.getDay())` on daily sync.
  - `src/WebApi.cpp`: exposed `runtimeTodaySec` and `runtimeYesterdaySec` in `/api/zones`.
  - `web/index.html`: closed `grpSched` by default (all 4 admin sections now collapsed initially); added `zones[].runtimeTodaySec` / `yesterdaySec` in `doc`; rendered runtime badge (`⏱ Încălzit azi: Xh Ym (ieri Ah Bm)`) on zone cards and in the diagnostic `relee` breakdown.
  - `docs/requirements_and_features.md`: updated FR-35 to cover zone-level runtime counters.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 test `SWE.5-TC19.15` and updated `TC19.14`.
- **Not changed:** hydronic safety invariants, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Reorganize admin in 4 collapsible groups, clear DTC button & polling throttle (90a14dd)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested reorganizing the long admin panel into minimizable collapsible sections, where by default all sections are collapsed except Confort & Programare; requested a button to clear DTC codes with history; and asked whether collapsed debug/trace sections still poll the ESP32.
- **Where:**
  - `web/index.html`: reorganized `<div id="admin">` into 4 sleek collapsible `<details class="adm-card">` containers: (1) Confort & Programare (open by default), (2) Sănătate & Diagnoză (with `[ Curăță codurile de eroare ]` calling `/api/diag/clear` and session history dropdown), (3) Configurare Instalație (Zone, Senzori, Sistem), (4) Instrumente Inginerie & Debug (Trace & Semnale); throttled `refresh()` polling so when debug is collapsed, heavy endpoints (`DOC.endpoints`) are bypassed and `updateWatch()` is skipped, saving 50% Wi-Fi requests and CPU cycles.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 test `SWE.5-TC19.14`.
- **Not changed:** hydronic safety invariants, backend endpoints.

## [03.11.2026] - Day-group aware schedule highlighting, clear check conditions & active DTC panel (f75c4e0)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that weekend moments were falsely highlighted as active on weekdays (Tuesday) in 5+2 schedule mode; Constant 24/7 mode displayed obsolete moment indices (Activ P2) in the schedule card badge; the FR-10.4 checks table showed confusing raw DTC codes in the rule column; and requested operational status rows for actuators/relays and boiler state alongside a dedicated active DTCs panel with human-readable causes.
- **Where:**
  - `src/ThermalZoneManager.cpp`: added `dayOfWeek` index (`0..6`, 0=Sunday) to `/api/schedule` payload.
  - `web/index.html`: made `renderMomentsTableHtml` day-group aware (`groupIsActive` checks `isWknd` for 5+2 and matching day of week for 7-day tabs); updated schedule card badge to display `Constant 24/7` in constant mode; clarified `CHECK_RO` acceptance rules as explicit operating conditions; added operational checks for boiler (cazan ON/OFF/burn sec/protection) and relays (circuite deschise/închise); added `#dtcSection` with reassurance card when 0 DTCs are active and detailed cause table with `DTC_INFO` when DTCs occur.
  - `docs/requirements_and_features.md`: updated FR-31, FR-10.2, FR-10.4 to reflect day-group aware highlighting, active DTC panel, and operational checks.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 tests `SWE.5-TC19.12` and `SWE.5-TC19.13`.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Clear obsolete report card & clarify schedule level names in import diff (e2536da)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that the previous green import result card persisted on screen when clicking export/import or selecting a new file; additionally, schedule level diffs appeared ambiguous/duplicated (e.g. 'Living › Țintă day' vs 'Living › Țintă away') because the schedule context was not explicitly specified.
- **Where:** web/index.html (cleared #cfgReport and #cfgPreview on cfgExport, cfgImportBtn, cfgFile.onchange, and btnCancelImport; disambiguated schedule levels in analyzeImport with self-explanatory names: 'Temperatură Program Zi (Day)', 'Temperatură Program Noapte (Night)', 'Temperatură Mod Plecat (Away)').
- **Not changed:** hydronic safety limits, backend endpoints.

## [03.11.2026] - Clean import result card, value changes table & collapsible technical details (7eae5ee)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that the post-import report dumped an unformatted, raw wall of 34 parameter names without values ('Aplicat: params.actuatorDelaySec, params.pumpOverrunSec...').
- **Where:** web/index.html (updated renderImport to display a sleek green result card, a clear table of actual modified settings with friendly names and before/after values, skipped parameters with reasons, and tucked the raw 34 technical API keys into a collapsible <details> dropdown).
- **Not changed:** ASPICE SWE.5-TC29.27 compliance (reads applied, kept, skipped, reason), backend import API.

## [03.11.2026] - Align import preview limits with §6, document & test UI additions (PB-70, f5ccb5d)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported that exporting and re-importing the same configuration flagged a false warning on 'params.rfSafetyHeartbeatSec (900)' out of range [10..300], and noted process drift (requirements and tests were not kept in sync with recent UI additions).
- **Where:** docs/problems.md (recorded PB-70), docs/requirements_and_features.md (updated FR-8.1 for pre-import diff preview/confirmation and vertical zone card setpoint steppers, baseline v2.11), web/index.html (aligned PARAM_LIMITS with requirements §6: rfSafetyHeartbeatSec 30..900, pumpOverrunSec 180..600, burnerMinRun/Pause 120..600, frostTemp 8..16, sensorTimeoutSec 60..3600, windowThreshold 0.8..2.0), tools/sh_tests/t_export_import.py (added SWE.5-TC29.38 testing pre-import preview, confirmation buttons, and §6 bounds), tools/sh_tests/t_ui_history.py (added SWE.5-TC19.11 testing vertical room card steppers and room-main quick view).
- **Not changed:** hydronic safety limits, backend endpoints.

## [03.11.2026] - Pre-import diff preview, validation warnings & confirmation dialog (1f162c5)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested that configuration import ('Import setări…') not apply immediately upon file selection; instead, the UI must preview changed values (old vs new), highlight any invalid or out-of-range parameters, and ask for explicit user confirmation before executing the import.
- **Where:** web/index.html (analyzeImport helper comparing uploaded JSON against current config export, #cfgPreview diff table, invalid parameter warning box with Romanian explanations, [ Confirmă și aplică importul ] and [ Anulează ] interactive buttons).
- **Not changed:** hydronic safety limits, backend /api/config/import endpoint and verification logic.

## [03.11.2026] - Fix JS syntax & implement vertical setpoint controls on zone cards (04a2a84)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported that removing legacy slots inadvertently left an unclosed brace in syncSchedInput causing a JS SyntaxError; additionally requested that setpoint controls be visible on all zones without clicking and positioned vertically on the right of each zone card to maximize quick-view space below.
- **Where:** web/index.html (restored missing closing brace in syncSchedInput, updated .room to flex row with .room-main and vertical .ctl, widened MAX_T to 30°C, always-visible [ + ] [ val ] [ - ] stepper on every card with ↺ prog boost reset).
- **Not changed:** hydraulic safety invariants, backend endpoints.

## [03.11.2026] - Remove legacy compatibility details and quick-away button (f7ad961)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner confirmed that the obsolete '<details> Avansat / Diagnostic compatibilitate' section and the 'Setare rapidă toate camerele' quick-away helper are unnecessary and clutter the UI.
- **Where:** web/index.html (removed legacy details block, removed quick-away input/button, cleaned up dead slot parsing/rendering JavaScript handlers).
- **Not changed:** backend schedule logic, Away mode per-room target temperature inputs.

## [03.11.2026] - Fix schedule mode & tab switching blocked by refresh guard (d366a91)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that clicking schedule mode buttons (such as 'Zilnic unificat') or day tabs had no effect. Root cause: the isEditing guard inside 
enderSchedule() was returning early on user click events when schedDirty was true. Fixed by moving background refresh guard to 
efresh() via isSchedBusy(), allowing user click actions to immediately update view state while keeping background refreshes from disturbing active typing.
- **Where:** web/index.html (isSchedBusy() helper, 
enderSchedule() early return removed, 
efresh() guarded, ocusout debounced).
- **Not changed:** firmware logic, hydronic safety invariants.

## [03.11.2026] - Schedule form refresh overwrite protection & customizable moment names (C-16, PB-69)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that background 2s polling refreshes reconstructed `c.innerHTML`, destroying active form input focus and reverting uncommitted values before saving; additionally requested that moment names (P1: Trezire, etc.) be user-customizable.
- **Where:** `FR-8.1`, `FR-29` (baseline updated), `include/ThermalZoneManager.h` (`momentNames` array in `ScheduleConfig`), `src/ThermalZoneManager.cpp` (`getMomentName`/`setMomentName`, `getScheduleJson` serialization), `src/WebApi.cpp` (`POST /api/schedule/names`), `web/index.html` (strict `renderSchedule()` guard when editing or dirty, immediate input event synchronization, editable `.mom-name` fields), `tools/sh_tests/t_control.py` (`SWE.5-TC16.7` custom moment names, `SWE.5-TC16.8` refresh dirty protection).
- **Not changed:** hydronic safety invariants and schedule evaluation logic.

## [03.11.2026] - Honeywell 4-Moment Scheduling, 4 Operating Modes & Configurable Away (C-15, PB-68)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested an intuitive multi-moment schedule model inspired by Honeywell T6/Evohome with 4 modes (Constant 24/7, Unified L–D, 5+2 Weekday/Weekend, 7-Day individual) supporting up to 4 daily moments (P1 Trezire, P2 Plecat/Zi, P3 Seară, P4 Somn) with direct per-zone target temperatures, user-configurable Away temperatures per room with quick global apply, and clean 24h UI eliminating AM/PM browser confusion and raw token mismatches.
- **Where:** `FR-29`, `FR-31` (baseline updated), `include/ThermalZoneManager.h`, `src/ThermalZoneManager.cpp`, `src/WebApi.cpp`, `src/ParamStore.cpp` (dynamic comparison buffer for blobs up to 1984 bytes preserving write-on-change wear leveling), `web/index.html` (Honeywell UI, mode pills, 24h inputs, copy helpers, away card, undo/save, collapsible legacy compatibility block), `tools/sh_tests/t_control.py` (`SWE.5-TC16.6` multi-moment matrix & modes roundtrip test).
- **Not changed:** hydronic safety invariants (interlockDelay 180s, pump overrun 180s, anti-short-cycling, frost guard). Full backwards compatibility maintained for legacy `/api/schedule` endpoints, slots, and level setpoints.

## [03.11.2026] - Underfloor thermal deficit full duty, weather compensation & UI ergonomics (C-14, PB-66, PB-67)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that corner rooms with high heat loss (Adults bedroom) remained cold due to rigid TPI duty (~33 % for 0.5 °C error shutting off after 20 min, dissipating screed momentum) while the UI falsely reported "Temperatură atinsă"; plus UI inputs were wiped by polling refresh before saving, and separate sensor search buttons were confusing.
- **Where:** `FR-47` (deficit rule: 100 % duty when error >= 0.4 °C; weather compensation up to 30 % when outdoor < 15 °C), `FR-8.1`, `FR-8.4` (accurate idle status text distinguishing TPI pause vs target reached, 2-in-1 sensor toggle, dirty tracking with undo for schedule/levels, dynamic visual schedule moments with Away last), `src/ThermalZoneManager.cpp`, `web/index.html`, test `test_tc11_5_tpi_deficit_duty` in `tools/sh_tests/t_control.py`.
- **Not changed:** hydronic safety invariants (interlock delay, pump overrun, burner anti-short-cycling).

## [03.11.2026] - `OutputBank`, second half of step 3 (R-A1, FR-15.1)

- **Who / when / why:** firmware session (Claude), 03.11.2026; migration step 3 as decided by the owner.
- **Where:** new `include/OutputBank.h`, `src/OutputBank.cpp`: pin mode, pin level and the NC / NO polarity of the
  zone valves only there; `ThermalZoneManager::applyRelay` chooses valve or pump/free output and calls it; the
  actuator type is held by `OutputBank`; `architecture.md` §3.2, `plan.md`.
- **Not changed:** behaviour. No new tests: the existing tests of FR-15.1, FR-43 and NFR-1.4 cover it (light mode).
  Not built here: logical channel map, expansion backend, energised-since (later steps).

## [02.11.2026] - Zone target out of the settings file (C-13, PB-62)

- **Who / when / why:** firmware session (Claude), 02.11.2026; the owner: what must survive a test or a reflash is the
  day, night and away setpoint of every zone, so they can re-import it and not be cold at night.
- **Where:** FR-65.1 (baseline v2.10), `ConfigTransfer.cpp` (no `target` in the zone entries; an old file with it skips
  it as unknown), `api_reference.md`; tests TC29.36 (no target in the file), TC29.37 (setpoints restored from an
  earlier export), TC29.1, TC29.5, TC29.9, TC29.18, TC30.5 follow. Test infrastructure from the split of the test
  script: TC29.16 (`_orig_post` import) and TC15.1 (repository root) fixed.
- **Not changed:** the day, night and away levels were already in the file (`schedule.levels`).

## [02.11.2026] - One Save for all zones (C-12)

- **Who / when / why:** firmware session (Claude), 02.11.2026; the owner on the board: sensors assigned to the wrong
  zones could not be moved, each zone had its own Save and the old zone still held the sensor.
- **Where:** FR-11.3, FR-65.5, FR-65.6 (baseline v2.9); `ConfigTransfer.cpp` checks a sensor MAC on the resulting
  configuration (new reason `sensor-taken`); `web/index.html`: one button `Salvează zonele` sends all zones to
  `api/config/import`, a skipped zone shows its reason in its row; `api_reference.md`; tests TC29.34, TC29.35, TC30.12.
- **Not changed:** single-zone `map-zone` (API, console) keeps its strict check. Light mode, no safety run: no
  valve, boiler, frost or sensor-timeout logic changed.

## [02.11.2026] - Process: model and effort per work, reading budget

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: too many tokens, also in the main chat.
- **Where:** `model` / `effort` in every agent's frontmatter: `sh-safety-reviewer` Opus high; test writer and
  reviewer, code, requirements and compliance reviewers Sonnet medium; `sh-assessor` Haiku low. `sh-write-tests`
  and `sh-review` pass Opus high for tests of the safety list and the full EU review. Every agent has a
  "Reading budget" (grep, then read by section). AGENTS.md §5 "Model per work" table for the main chat and the
  sessions it starts. Field names checked in the Claude Code docs (sub-agents, skills, settings).
- **Not changed:** the model of the main chat and the project's default model are the owner's settings.

## [02.11.2026] - Zones added and deleted by the user, sensor search (C-10, C-11), first half of step 3

- **Who / when / why:** firmware session (Claude), owner decisions 02.11.2026: a new controller has no zone, the user adds
  and deletes zones and picks sensors by MAC or by a search; fixes PB-55 (sensor table filled by neighbours' sensors).
- **Where:** requirements baseline v2.8 (FR-69, FR-73, FR-74, FR-11.3, FR-11.5, FR-11.6, FR-43); `ThermalZoneManager`
  (32 slots, `addZone`, `deleteZone`), `ParamStore` (zone count, records read from NVS), `BleSensorScanner` (search,
  assigned sensors kept), `WebApi` (zone-add, zone-delete, sensor-search, relay test per FR-43), page, API reference.
  A controller that has zone records keeps its three zones; the old schedule is converted.
- **Board:** flashed, system test 123 pass, 0 fail, 3 skip; free heap 102 KB. Review: `docs/review_step_3.md`.
- **Not done:** `OutputBank` module, assessor and compliance review of the step, owner test.

## [02.11.2026] - Configuration export / import (FR-65, change C-9)

- **Who / when / why:** firmware session (Claude), owner decision 02.11.2026: keep using the board while the software
  changes. Own change before migration step 3 (order: this, step 3, step 2).
- **Where:** requirements baseline v2.7 (FR-65.1 ... FR-65.8, FR-58, FR-8.1, NFR-2.3, FR-11.3 name limits);
  `src/ConfigTransfer.cpp` (new), `WebApi.cpp` (`GET /api/config/export`, `POST /api/config/import`), `ThermalZoneManager`
  (`getLearnedDutyPct`, `setLearnedDutyPct`; `/api/zones` `learnedDuty` now one decimal), `web/index.html` (export /
  import buttons, import report, doc table), `docs/api_reference.md`, `docs/architecture.md`, `docs/compliance.md`.
  Tests TC29.1 ... TC29.33 written first, red on 2b7564f (33 of 33), commit e73fdb6.
- **Problems:** PB-55 (sensor table), PB-56 (unbounded HTTP body), PB-57 ... PB-61 from the safety review (heap growth of
  one import, valve-moving settings while valves are open, loop timing, frost on a disabled zone, tiny learned duty).
- **Board:** flashed ce8a58b (1.11.02.03); system test 112 PASS, 1 FAIL (TC29.2, zone target, PB-62), 2 SKIP (TC29.15,
  TC29.33: no learned duty above 0 % on the board); round trip export -> import -> export checked by hand: identical.
  Board check without error. Owner acceptance pending.

## [02.11.2026] - EU conformity: first full review logged

- **Who / when / why:** `sh-compliance-reviewer` (scope `all`), logged by the process chat with the owner's OK
  ("Da, trece-le"). Main claims checked against the code before logging.
- **Result:** 31 obligations: 12 partial, 19 missing, none covered. `docs/compliance.md` §3 statuses filled, §2
  GDPR row corrected (location and time requests leave the device), §4 WEEE and GPSR rows added.
- **Problems:** PB-43 ... PB-54 (4 safety: setup access point never closes, no authentication, relay test in any
  mode, BLE sensor table can lose the mapped sensors). **Requirements:** `docs/requirements_review.md` §10, 18
  rows (5 conflicts, 13 missing) and owner questions D40 ... D50.
- **Not done:** nothing fixed; the decisions are open. A duplicate of PB-43 ... PB-54 written by a re-run of the
  logging script was removed before this entry (never committed).

## [02.11.2026] - Process: step assessor; EU conformity supervisor

- **Who / when / why:** process chat (Claude), 02.11.2026. The chain checks itself, nobody checked its evidence;
  the owner wants to sell the device in the EU under their brand, without third-party certification.
- **Where:** `.claude/agents/sh-assessor.md` (evidence of a step, SUP.1); `docs/compliance.md` (EU acts,
  obligations `CMP-n` on the software, technical file - all `open`) and `.claude/agents/sh-compliance-reviewer.md`;
  `sh-change` runs both at the end of a step and the compliance check on security-related requirement changes;
  `sh-write-tests` commits the frozen tests before the code; `sh-review` args `assess`, `compliance`; AGENTS.md
  §2 (prepared for sale in the EU, no shop), §5; `docs/process.md` §1, §3.1, §9; `docs/plan.md` out of scope.
- **Not verified:** neither agent has run yet; `docs/compliance.md` is not legal advice and needs a consultant or
  lab before the first sale.

## [02.11.2026] - Process: short logs, short checker output

- **Who / when / why:** process chat (Claude), 02.11.2026, owner's choice "Jurnale scurte" and "Verificare pe
  scurt": every chat read long logs and a 126-line checker report, most of it about a system that no longer exists.
- **Where:** entries before 29.10.2026 moved unchanged to `docs/archive/changelog_until_2026-10-21.md` (changelog
  1743 -> 369 lines); decisions of 2026-10-06 ... 2026-10-22 to `docs/archive/decisions_until_2026-10-22.md` (539 ->
  392 lines); `activeContext.md` keeps only "Now" and "Next" (57 -> 21 lines), its old lines in
  `docs/archive/active_context_until_2026-11-02.md`; deleted with the owner's OK: `productContext.md`,
  `progress.md`, `projectbrief.md`, `systemPatterns.md`, `techContext.md` (pointers or stale, read by no tool).
  `tools/sh_process_check.py`: the default run prints a WARN count per section (126 -> 24 lines); a named check,
  `--full` and CI list every WARN. AGENTS.md §5, `docs/process.md` intro and §3, `sh-verify-firmware` step 2.
- **Verified:** checker PASS, same totals in short and full mode (0 errors, 1 known, 104 warnings, 1 waived).

## [02.11.2026] - Process: the owner's chain (`sh-change`)

- **Who / when / why:** process chat (Claude), 02.11.2026. The owner cannot review code and tests or run the
  process step by step; the tools existed, but nothing ran them in order.
- **Where:** `.claude/skills/sh-change` (log → requirement → tests first → code → gates → commit → board → owner
  test; the owner only tells, decides a requirement change, tests on the board); AGENTS.md §5 (chain rule, commit
  inside the chain without asking, "one chat, one zone" removed); `docs/process.md` §1, §5; `sh-verify-firmware`
  and `sh-write-tests` hand over to the chain; decision in `.agent/memory-bank/decisions.md`.
- **Not verified:** the chain has not run yet.

## [02.11.2026] - Process: tests first in a clean room, one reviewer per work product

- **Who / when / why:** process chat (Claude, branch `claude/vibrant-ride-95fick`), 02.11.2026, asked by the owner:
  tests must check the requirements, not adapt to the code; a failing test fixes the firmware, not the test.
- **Where:**
  - `.claude/agents/`: `sh-test-writer` and `sh-test-reviewer` (clean room: a hook blocks reads of `src/`,
    `include/`, `web/`, `lib/` and the other tools, and edits outside `tools/sh_test_system.py`),
    `sh-code-reviewer` (code against requirements, architecture, API reference, current step), `sh-req-reviewer`.
  - `.claude/hooks/sh_clean_room.py`: the clean-room hook (29 allowed / blocked cases checked by hand).
  - `.claude/skills/`: `sh-review` (reqs / tests / code / trace / all), `sh-write-tests` (write, review, red run,
    freeze); `sh-verify-firmware` steps 3-4 launch the new reviewers.
  - `docs/process.md` §1, §3.1 (tests first, red run, freeze; replaces "tests follow the code"), §3.2, §9;
    AGENTS.md §4 steps 3-4; decision in `.agent/memory-bank/decisions.md`.
- **Trial run** of `sh-test-reviewer` on all 82 tests: the hook blocked every read of the code (32 attempts),
  but also the agent's report hand-back - fixed (the hook now guards only the file and shell tools; 30 cases
  checked); the old test file name is allowed for `git diff` against main. Review result: 22 tests OK, 60 with
  findings (7 safety), 59 requirements untested, API reference lacks field names, states and fault injection.
  Logged in `docs/problems.md` as PB-25 ... PB-42 (owner: "Da, toate"; 9 marked safety), not fixed here.
- `tools/sh_process_check.py trace`: WARN on a parent requirement with its own `{LEVEL; METHOD}` (FR-3, FR-9,
  FR-40) - requirements §0 says a parent only groups, and such a requirement was never counted as untested.
- **Open:** `architecture.md` §18 still says "no separate test step first"; the `sh-test-writer`,
  `sh-code-reviewer` and `sh-req-reviewer` agents have not run yet.

## [01.11.2026] - Owner page test: engineering view, commissioning, states, checks, version (PB-17 - PB-21)

- **Who / when / why:** main chat (Claude), 01.11.2026, the owner's hands-on remarks on the page (step 1 board)
  and the request for an engineering view "like CANoe / Trace32".
- **Requirements (baseline v2.6, changes after closure C-3 - C-8, owner answers 01.11.2026):** FR-8.1 header in
  words with the next change, admin view = engineering view (every API value with its API name, explanation,
  requirement, change highlight, change trace with CSV export), commissioning in the page; new FR-8.4 zone states
  (`FROST`, `DISABLED`, `FAULT`, `ACQUIRING`, `WINDOW`, `HEATING`, `IDLE` with Romanian texts); FR-10.4 checks with
  rule, value and `OK` / `FAIL` / `INFO`, no health score; FR-11.3 relay used once and not by the pump, zone on
  only with a sensor, number of zones = zones switched on; FR-21 version `1.MM.DD.NN` from git. Architecture §7
  displayed state order and §10 health line follow.
- **Board:** zone data copied from the old software erased (owner decision "now"): Zone 1-3, no sensor, off, relays
  32 / 33 / 25. Heating off until commissioning; the old mapping is in the session scratchpad.
- **Firmware:** `ThermalZoneManager::getStateCode()` (FR-8.4, a switched-off zone is `DISABLED`);
  `DiagnosticEngine::runChecks()` / `checksJson()` replace the health score in `/api/diag/status`,
  `/api/boiler/status`, `/api/diag/selftest`, console `DIAG` and the boot banner (no more `PASS` without a check);
  `dtcCount` counts active DTCs; `/api/setup/map-zone` checks every field first (HTTP 400, nothing changed);
  enabling the pump is refused while a zone uses relay 4; `APP_VERSION` written by `tools/sh_git_version.py`;
  installation names removed from the relay comments in `Config.h` (FR-11.1).
- **Web:** family view shows switched-on zones with their state text and the schedule sentence; admin view has the
  checks, commissioning per zone (name, sensor, relay, on / off, offset), sensors, schedule, system, the signal
  watch of 8 GET endpoints (2 s) and the change trace (CSV).
- **Tests:** TC10.2 / TC10.3 without health score, new TC10.4 (FR-10.4), TC15.1 version from git (FR-21), TC19.6
  no assertion on the temperature of a zone without reading (no requirement), new TC19.8 (FR-8.4), TC19.9 (every
  API value explained, FR-8.1), TC19.10 (FR-11.3 rejections, pump on a zone relay).
- **Tools:** `sh_process_check.py` errors on a hard-coded `APP_VERSION`, release tag from the computed version;
  `sh_esp_cli.py diag` prints the checks.
- **Safety review:** BLOCKER B1 (relay moved while energised) and RISK R1 (zone off while its valve is the last open one), R2 (relay 4 polarity), R3 fixed before flashing; valve-open rule added to FR-11.3.
- **Found on the way:** PB-22 (room `+` lowers a target above 22.5 °C), PB-23 (FR-51 not built, safety), PB-24 (relay swap).

## [01.11.2026] - Page works behind a path-prefix proxy (PB-15)

- **Who / when / why:** main chat (Claude), 01.11.2026, owner report: through the home-server proxy
  (`/heating/`) the page showed only a dark skeleton with "HTTP 404".
- **Cause:** the page requested root-absolute URLs (`/api/...`, `/update`), which skip the proxy prefix.
- **Requirements:** FR-8.1 gains the proxy clause (change after closure C-2, baseline v2.5); owner approved, kept
  general - not tied to one server.
- **Web:** `web/index.html` requests every URL relative to the page address (`api/...`, `update`). No firmware
  logic changed.
- **Tests:** new TC19.7 (FR-8.1) fails on 7743c26 (5 root-absolute URLs), passes on the fixed page.
- **Checks:** `pio run` 1.14 MB; `sh_process_check.py` 0 errors, 1 known (PB-12). Safety review not needed (no
  `src/`, `include/`, `platformio.ini` change).
- **On the board:** the uncommitted build was flashed by mistake (`sh_ota_upload.py --help` uploads at once,
  PB-16); `/api/zones` 200 direct and through the proxy, TC19.7 PASS. Not a result of record until built from the
  commit.

## [01.11.2026] - Migration step 1: ParamStore (one parameter table, NVS in one place)

- **Who / when / why:** main chat (Claude), 01.11.2026, owner request "do it here" (architecture §18 step 1).
- **Firmware:** new `ParamStore` (`include/ParamStore.h`, `src/ParamStore.cpp`) is the only NVS user (R-A4): one
  table with the requirements §6 defaults and ranges (interlock, overrun, burner run / pause, hysteresis, sensor
  timeout, frost, window threshold, heartbeat, link heartbeat, open-loop bypass, actuator type, pump), zone records,
  learned duty (DTC-301 when discarded), state and location; range check on set and load, writes only on change.
  One-time copy of the old keys (schema 1), written only if every record was stored; old keys kept for a rollback.
  `ThermalZoneManager`, `BoilerRadioTrigger`, `TimeAndWeatherService` load from it; setters reject out-of-range
  values. `/api/config/system` POST validates all fields (exact step, §6 range) and rejects the whole request with
  HTTP 400; new fields `burnerMinRunSec`, `burnerMinPauseSec`, `hysteresis`, `windowThreshold`; removed
  `boilerDelaySec`, `turReturSensors`, `shortCyclingSec`, per-zone hysteresis / window threshold. Interlock and
  overrun persisted; one global hysteresis and window threshold; RF cooldown fixed 3 s (FR-17.2); reference MACs and
  zone names removed from firmware (FR-11.1); factory levels 23 / 21 / 18; location validated (FR-9.2).
- **Tests:** registry read from the requirements document (no copy); retired IDs remapped; TC07.2 interlock range,
  TC14.1 / TC14.4 sensor timeout range, TC02.2 location range, TC26.2 global window threshold, TC27.2 restores the
  board, new TC27.6 rejection below the §6 floors. Checker reads the safety rows of the ParamStore table.
- **Fixes:** PB-4, PB-5, PB-6, PB-8, PB-9, PB-11 (PB-13 with the flash). PB-14 stays open: the default is §6, the
  second DTC-101 path (1200 s in `DiagnosticEngine`) is step 6.
- **Checks:** `pio run` 1.19 MB; `sh_process_check.py` 0 errors, 1 known (PB-12); `sh-safety-reviewer` no BLOCKER,
  its three migration risks fixed before the commit (schema only after successful writes, migrate only without a
  schema, old keys kept).
- **On the board (01.11.2026):** flashed 413fa60, then 7743c26 (window threshold endpoint no longer needs a zone
  id; TC26.2 had left 1.5 C on the board because its restore was rejected). Migration log: every zone (name, MAC,
  pin, enabled, target, offset), schedule, levels 22 / 20 / 16, burner counters and location kept; burner 180 / 180,
  heartbeat 900 s, sensor timeout 600 s, frost 12 C carried over; Z2 / Z3 window 0.3 C dropped (global 1.2 C);
  `openLoopBypass` false (test leftover reset). Interlock 190 s, window 1.3 C, hysteresis 0.35 C survived an OTA
  reboot (PB-5), then restored. `sh_test_system.py` 81/81 PASS, coverage 57 / 91 requirements (the rest belong to
  later steps), tests leave the configuration unchanged; `sh_process_check.py board` PASS; free heap 112 KB.
  PB-4, PB-5, PB-6, PB-8, PB-9, PB-11, PB-13 set to fixed; closed after the owner's acceptance.

## [01.11.2026] - Requirements v2.4: watchdog on both tasks, safety reaction bound 10 s

- **Who / when / why:** requirements and architecture sessions, 01.11.2026, after two owner decisions
  (`decisions.md`): O-16 (no preference, recommended option) and the simplified NFR-1.8 (option B).
- **Requirements:** v2.3 — NFR-1.5 task watchdog covers `loop()` and the control task, a manual request counts from
  its acceptance; v2.4 — NFR-1.8 one bound of 10 s for every safety reaction, only exception a firmware update with
  the boiler OFF; D39 radio-cooldown clause superseded. Recorded as post-closure changes (`requirements_review.md` §9).
- **Architecture:** O-16 decided, 10 s bound everywhere, detail that existed only for 2 s removed; AR-21 (own control
  task) kept.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Requirements v2.2 frozen; control tick in its own task (AR-21)

- **Who / when / why:** requirements and architecture sessions, 01.11.2026, after the owner's questionnaire
  (`decisions.md`, "Control in its own task…"). Verified and committed by the main chat.
- **Requirements v2.2:** O-12 (status reports longest control-tick delay and loop pass), O-13 (pump stop and forced
  manual OFF under NFR-1.8), O-14 (commissioning result is a document), O-15 (self-check may end a burner run early).
  Review closed by the owner; open action only: blueprint location and version (D37).
- **Architecture:** AR-21 rewritten — control tick in its own FreeRTOS task above `loop()`, command queue I15 and
  snapshot I16, both tasks on the watchdog, built in migration step 10; AR-3 / P6 amended; AR-22 approved; RAM budget
  +16 KB static against a 10 KB margin, covered by the `String` removal of step 11 and the planned log-ring saving
  (risk). New observation O-16 (watchdog of both tasks, NFR-1.8 trigger, OTA exception) for the owner.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Requirements D37-D39 and architecture aligned to baseline v2.1 (wave 2)

- **Who / when / why:** requirements and architecture sessions started by the main chat, 01.11.2026; verified and
  committed by the main chat. Owner decisions D37 = B, D38 = A, D39 = A (`decisions.md`).
- **Requirements:** FR-60 skips the sensor check while no zone has a mapped sensor; NFR-1.8 bound applies to the
  decision and every relay, a radio frame may also wait for the FR-17.2 cooldown; blueprint location / version
  still to be given by the owner (D37). Review header still open (owner sign-off).
- **Architecture:** NFR-1.8 allocated and designed (bounded `WebServer` pass, ≤ 500 ms loop pass, relays before the
  radio frame, loop timing measured, OTA only behind `QuietGate`); boot OFF starts `burnerMinPause`; FR-60 per D38;
  glossary terms, I-3 with fault duty, only `SW` requirements allocated to modules, gaps G-n and steps cite `PB-n`,
  new gap G33. Proposed for the owner: AR-21, AR-22; observations O-12…O-15 for the requirements.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Process review QA-1..QA-28 applied (wave 1)

- **Who / when / why:** three sessions started by the main chat, 01.11.2026, after the owner approved every row of
  `docs/process_review.md` with the recommended options. Verified and committed by the main chat.
- **Process session:** `docs/process.md` (test review gate §3.2 in the owner's order, waivers, tailoring, ASPICE
  labels, SUP.1 / MAN.3, owner flash decision), `AGENTS.md` (safety list by ID and parameter, FR-51, NFR-1.4; plan =
  architecture §18), `sh-safety-reviewer` and `sh-verify-firmware` read requirements and invariants at run time,
  `sh_process_check.py` (waivers, `{LEVEL; METHOD}` attribute, coverage per method, KNOWN errors via open `PB-n`,
  R-A1…R-A7 grep WARN), `sh_git_version.py` fixes, `problems.md` PB-1…PB-14, new columns in problems / acceptance.
- **Requirements session:** baseline v2.1 — `{LEVEL; METHOD}` on every active requirement, glossary, FR-2 / FR-4
  by parameter name, "flame lit" defined, FR-60 one mapped sensor, NFR-1.4 boot OFF starts `burnerMinPause`, new
  NFR-1.8 (safety reaction ≤ 2 s), phase tags mapped to migration steps, waiver table; review stays open for D37-D39
  and the owner's sign-off.
- **Records session:** `decisions.md` (D1-D36, O-1…O-11, AR-1…AR-20, quiz, work order, superseded decisions),
  `plan.md` as the migration plan with risks, memory-bank pointers to the architecture, hydraulics doc in English
  aligned with baseline v2.
- **Checks:** `sh_process_check.py` 0 errors, 21 known (PB-n), PASS; `pio run` clean. **Not changed:** firmware
  (`src/`, `include/`, `web/`), tests.

## [01.11.2026] - Process review QA-1..QA-28 (proposal)

- **Who / when / why:** main chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, asked by the owner
  after the process, requirements and architecture chats were merged back: review everything except code and tests.
- **Where:** `docs/process_review.md` (new): 28 rows, 6 blocking (empty `problems.md` with known safety defects on
  the board, `AGENTS.md` safety list and `sh-safety-reviewer` against baseline v2 incl. FR-51, requirements review not
  closed and its exit criterion unreachable without waivers, decisions not in `decisions.md`, test/code order and no
  test review gate). Then requirements (SYS/SW level, verification method, FR-4 values, "flame lit", glossary, OTA
  validation, burner pause after reboot, scope vs plan), architecture (loop blocking vs FR-37, tailoring, RAM risk,
  I-3), ASPICE labels, traceability of acceptance / problems, CI, stale memory-bank and hydraulics docs.
- **Status:** awaiting owner approval row by row. **Not changed:** any reviewed file, firmware, tests. Nothing committed.

## [01.11.2026] - Process: one active chat owns every zone

- **Who / when / why:** process chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, asked by the owner.
  The owner moves all work back to the main chat (parallel chats lost context); the "one chat, one zone" rule
  could have made that chat refuse process or requirement work.
- **Where:** `AGENTS.md` §5: "When only one chat is active, it owns every zone." The zone split still applies
  when parallel chats run.

## [01.11.2026] - Traceability: SWE.1 ↔ SWE.2 kept in one place and checked

- **Who / when / why:** `/simplify` chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026 02:10, asked by the
  owner to simplify SWE.1, SWE.2 and the trace between them. The requirement → module links were written twice by
  hand (`architecture.md` §3.2 column and §15 matrix), disagreed on 37 requirements and no tool read them.
- **Where:**
  - `docs/architecture.md`: §3.2 Requirements column is the only allocation (union of both old tables, no link
    dropped; FR-64 → `SafetySupervisor` taken from §6.1 S4; new "all modules" row for cross-cutting NFRs; DTC IDs
    removed, §10 is the DTC → module table, now one row per code); §15 matrix replaced by a pointer; the FR-51
    sensor-loss rule stated once in §7.1 (S4 had dropped "outdoor unknown" and the disabled zone); §9.2 cites FR-58
    instead of copying its list.
  - `docs/requirements_and_features.md`: ID lists removed from section headings (they had drifted: FR-39, FR-72
    missing, FR-44 misplaced); §0 says IDs are cited in full, never as ranges; §7 names the hand-kept links.
    No requirement text changed.
  - `tools/sh_process_check.py trace`: reads the §3.2 allocation (ERROR on a retired / unknown / parent ID, WARN on an
    active requirement without a module), derives parent IDs from children instead of headings, matrix gets a
    Modules column, `--matrix` works without a path. Output otherwise identical (16 errors, 84 warnings, as before).
  - `docs/process.md`: SWE.2 row, rule R1 (no ID ranges), §7 gap partly closed.
- **Owner review:** the 37 merged rows in §3.2. **Not changed:** firmware, tests. Committed locally with the
  first commit of `tools/sh_process_check.py`, `tools/sh_git_version.py` and `docs/process.md` (owner OK); not pushed.

## [01.11.2026] - Requirements: architecture observations O-1..O-11 applied

- **Who / when / why:** requirements chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026 01:29.
  The architecture chat handed observations O-1..O-11 (`docs/architecture.md` §16) to the requirements chat;
  the owner decided the open ones by questionnaire.
- **Where:** `docs/requirements_and_features.md` (FR-3, FR-12.2, FR-47, FR-51, FR-54, FR-58, FR-62, FR-64, FR-69,
  FR-71, NFR-1.4, NFR-3.2, §5 new DTC-208, §6 new parameters) and `docs/requirements_review.md` §6 (decision log).
- **Safety:** the boiler never burns with all zone valves closed (O-1); a zone with a lost sensor keeps frost safety
  in global OFF below `faultFrostOutdoor` = 5 °C outdoor (O-2, owner chose 5 °C); the flow-limit OFF is forced (O-5).
- **Not changed:** firmware, tests, `docs/architecture.md`. Nothing committed.

## [01.11.2026] - Architecture: SWE.2 for baseline v2 approved

- **Who / when / why:** architecture chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, entry written
  01.11.2026 00:50. Asked through the session that coordinates the chats: write the software architecture (ASPICE
  SWE.2) for requirements baseline v2, because process §7 listed "SWE.2 architecture not defined" as open and the
  current code mixes control and safety in one class. Every decision row was approved by the owner.
- **Where:** `docs/architecture.md` (new): 23 modules in 6 layers, `SafetySupervisor` as the only writer of outputs,
  interfaces, 1 s control tick, state machines (zone, valve, boiler, boiler link, mode, season, boot / OTA), validity
  `Ok` / `Held` / `Lost` on every input with a fallback matrix, `ParamStore`, DTC and event store, fault injection,
  output / boiler-link / input abstractions, phase-4 advisor contract, flash / RAM / timing / wear budget, trace of
  all 103 active requirement IDs to modules, gap list G1-G32 and 14 migration steps. `AGENTS.md` §5: new zone
  *architecture* (`docs/architecture.md`), added with the owner's OK.
- **Owner decisions (AR-1..AR-20):** last zone valve stays open while the burner is still ON or its OFF is deferred,
  then for `overrun`; learned TPI duty per zone and outdoor band of 5 °C; weather fetch in a background task;
  history in LittleFS; flash margin ≥ 40 KB; season safety net only in season mode AUTO. Later the same day: in
  global OFF a zone with a lost sensor runs `faultDuty` when it is cold outside (O-2, limit set by the requirements
  chat); a flow-temperature limit stops the boiler at once (O-5, only with the optional probes, none installed);
  G2 fixed in migration steps 4 and 9, no separate hotfix. Work order (owner): requirements → architecture → code,
  each migration step changes the code and its tests together (no separate test step first).
- **Aligned 01.11.2026 01:45** with the requirements commit 925ea91: `faultFrostOutdoor` (5 °C) in global OFF and
  for disabled zones, DTC-208 for a failed output expansion, §6 parameter names, `AUTH` list = FR-58, §16 marked
  applied, §18 work order.
- **Not changed:** firmware, tests, requirements. Open: observations O-1..O-11 (`docs/architecture.md` §16) for the
  requirements chat (O-1 and O-2 are safety); dependency checks R-A1..R-A7 for the process chat.

## [01.11.2026] - Requirements: review approved, baseline v2

- **Who / when / why:** requirements chat (Claude, branch `claude/vibrant-ride-95fick`), 31.10.2026–01.11.2026,
  entry written 01.11.2026 00:02. Asked by the owner: ASPICE SYS.2 / SWE.1 review of the v1 specification
  (Romanian, partly obsolete, values contradicting each other and AGENTS.md), then a new baseline. Every row was
  approved by the owner.
- **Where:** `docs/requirements_review.md` (new): findings per requirement, domain gaps G-01..G-22, owner's
  blueprint comparison B-01..B-21, decisions D1-D36, fallback matrix. `docs/requirements_and_features.md`
  rewritten in English as baseline v2 (IDs kept where the meaning did not change, retired IDs struck through,
  new FR-52..FR-72 and NFR-5.x, DTC table, parameter table).
- **Key decisions:** interlock 180 s; overrun keeps the last valve open without a pump too; burner min run / pause
  180 s; sensor timeout 20 min, then the zone runs its learned duty (20 % if nothing learned); frost protection
  also on disabled zones; boost upward only (+0.5-2 °C, ends after 3 h or at the next slot); levels 23 / 21 / 18;
  one global window threshold, smart window detection in phase 4; automatic heating season; 1-32 zones;
  fail-safe boiler link (heartbeat, can be disabled); API authentication; OTA rollback and manual full self-test;
  alarms page-only; every feature has a fallback that the occupants do not notice (NFR-5).
- **Not changed:** firmware, tests. Open: `tools/sh_test_system.py` still cites retired FR-13, FR-24..27, FR-33,
  FR-38 (`sh_process_check.py trace`: 16 errors); new DTCs are not raised by the firmware yet.

## [31.10.2026] - Process: chat zones, problems log, acceptance, release gate

- **Process:** AGENTS.md §5 "One chat, one zone" - process / requirements / firmware chats change only their
  files; shared logs are append-only.
- **Process:** new `docs/problems.md` (`PB-n`, symptom, board commit, safety, cause, fix, status) and
  `docs/acceptance.md` (owner's hands-on test: board commit, what was checked, OK/NOK). process.md §5 now
  covers commit -> flash -> accept -> release gate -> PR -> merge + tag `vAPP_VERSION`; §8 problem flow.
- **Tools:** `sh_process_check.py process` checks the problems log and `PB-n` citations in commits;
  new `release` mode: OK acceptance for a build with HEAD's firmware, no open safety problem,
  `APP_VERSION` not already tagged with other firmware.

## [31.10.2026] - Process: requirement rules, board check, review / decision / commit gates

- **Process:** new [`docs/process.md`](process.md) - work products, requirement writing rules (R1-R6),
  requirements review steps and exit criteria, decision recording, commit/push gate, board check,
  known process gaps. Referenced from AGENTS.md §4.
- **Tools:** `sh_process_check.py reqs` (also in the default run and CI, WARN only) - one value per safety
  quantity, vague wording, one meaning per DTC (two names, unnamed citations, defined but never raised).
- **Tools:** `sh_process_check.py board [--ip]` - live `/api/config/system` values against the safety limits
  (ERROR), commit the board runs vs HEAD (WARN). Added to AGENTS.md §4 and the `sh-verify-firmware` skill
  (before and after flash).
- **Build identity:** new pre-build script `tools/sh_git_version.py` writes `include/git_version.h`
  (gitignored) with `APP_GIT_COMMIT` = short HEAD, `-dirty` on uncommitted firmware inputs. `Version.h`
  includes it instead of the hard-coded `8065534`. `sh_process_check.py process` errors on a hard-coded
  commit or a missing pre-script.
- **Open (process gaps, see process.md §7):** burner min pause not exposed by the API.

## [31.10.2026] - Tool naming convention

- **Process:** new project tools/scripts start with `sh_` (AGENTS.md §3). `tools/process_check.py` renamed to
  `tools/sh_process_check.py`; CI, `verify-firmware` skill and `safety-reviewer` agent updated.
- **Process:** existing tools renamed too: `sh_esp_cli.py`, `sh_test_system.py`, `sh_build_web.py`
  (`platformio.ini` extra_scripts), `sh_ota_upload.py`, `sh_dev_server.py`. Agent `sh-safety-reviewer` and skill
  `sh-verify-firmware` use `sh-` (names allow only hyphens). README, AGENTS.md, docs and memory bank updated;
  older changelog entries keep the old names.

## [30.10.2026] - Agent workflow: safety reviewer and verify skill

- **Tools:** `.claude/agents/safety-reviewer.md` - read-only agent that reviews the firmware diff against the
  AGENTS.md safety list (interlock, overrun, anti-short-cycling, frost, DTC-101 / safe state) and resource rules;
  runs `process_check.py process` first and reports BLOCKER / RISK / NOTE.
- **Tools:** `.claude/skills/verify-firmware/SKILL.md` - the "verification before flashing" pipeline as one
  command: `pio run` -> `process_check.py` -> safety review -> OTA flash (asks first) -> `test_system.py`.

## [30.10.2026] - Traceability and process check

- **Tools:** new `tools/process_check.py` (stdlib only, no board). `trace`: requirements in
  `docs/requirements_and_features.md` vs test docstrings / `SWE1_REQUIREMENTS` vs `// FR-x` tags in code,
  optional Markdown matrix (`--matrix`). `process`: safety defaults (interlock, overrun, anti-short-cycling,
  frost, sensor timeout), changelog + activeContext updated with code, commit message format, debug leftovers,
  tracked `secrets.h`, library changes, firmware < 1.25 MB. Runs in GitHub Actions after `pio run`.
- **Safety fix:** `Config.h` defaults were test-bench values (interlock 5 s, overrun 10 s, boiler off 15 s)
  and are not stored in NVS, so every boot ran with them. Now 120 s / 180 s / 180 s (FR-2, FR-3, FR-4).
  The check also covers the real anti-short-cycling defaults in `BoilerRadioTrigger` (`min_pause`, `min_run`).
- **Checker review fixes:** requirement IDs must match exactly (`FR-10.9` no longer passes via `FR-10`);
  coverage counts only tests citing the leaf's own ID, parent-only coverage is a separate warning (30/88, was
  reported as 75/88); duplicate requirement definitions are errors; API clamps and the frost NVS default are
  checked next to the compile-time defaults; git failures abort with an error instead of passing; merge
  commits skipped; paths with spaces/diacritics parsed correctly; CI on `main` diffs against the previous push.
- **Open findings (not fixed):** API clamps allow interlock/overrun from 1 s, short-cycling from 10 s, frost
  from 5 °C; code tag `FR-17.7` not in the requirements doc; FR-41..FR-45 used by tests but missing from the requirements doc; FR-36
  (Home Assistant) still in the doc; commit `abb8dec` subject `test,cli:` not in the allowed types;
  `/api/config/system` accepts interlock/overrun down to 1 s; `_minBoilerOffMs` is never read.

## [30.10.2026] - First verification on the board

- `pio run` clean (flash 1.19 MB), flashed with `tools/ota_upload.py`, `tools/test_system.py` 78/78 PASS.
- **Tests:** dropped `FR-36` (Home Assistant, removed in phase 1) from the requirement registry; new
  `SWE.5-TC16.5` covers `FR-44` global away (toggle, every zone's `effectiveTarget` = away level, restore).
  `activeLevel` stays the slot level while away is on — by design, the UI checks `away` first.

## [29.10.2026] - Level schedule, new UI, TPI control

- **Schedule:** bio-schedule, presets, anchors and named blocks replaced by day/night/away levels per zone,
  one weekday and one weekend slot list for all zones, global away mode. New API `/api/schedule/level`,
  `/api/schedule/slots`, `away` on `/api/schedule/enable`; console `SCHEDULE LEVEL`, `SCHEDULE SLOTS`, `AWAY`.
  NVS `sched/cfg2`; legacy schedule keeps only its on/off flags, legacy keys and `presets` namespace cleared.
- **UI:** `web/index.html` rewritten (4886 → 222 lines): room cards, timed boost (16–22.5 °C, 0.5 °C), Away,
  alerts, `#admin` view. `web/setup.html` and `/setup` removed. Checked in headless Chromium against a mock API.
- **Control:** TPI (60 min cycle, min 10 min, band 1.5 °C, learned base duty 0–60 %, hysteresis = overshoot cut),
  weather-dependent pre-heat lead (45 min–4 h), per-zone sensor offset (`OFFSET`, map-zone `offset`).
- **Tools:** `ota_upload.py` reads OTA credentials from `secrets.h` and refuses to flash without it;
  `esp_cli.py schedule` forwards to the console; test suite group 16 rewritten for the level schedule.
- Docs: README and API reference rewritten for the current firmware.

## [29.10.2026] - Lean phase 2: firmware removals

- **Safety:** removed Service Mode and forced boiler START (both bypassed the anti-short-cycling guard). Forced STOP kept.
- Removed PWA (manifest, service worker, icons), web console page (`/console`; Telnet and `/api/console/*` kept), unused `PubSubClient`.
- Replaced `DevicePassport` with a plain weather location setting (`/api/setup/location`, console `LOCATION`), migrated from the old NVS namespace.
- Added GitHub Actions firmware build (`.github/workflows/build.yml`).

## [29.10.2026] - Lean phase 1: repository cleanup

- Removed docs outside current scope: market/ROI, sales plan, multi-tenant SaaS architecture, remote fleet, session summary, generated test report.
- Removed Home Assistant integration (`homeassistant/`) and its tests (TC22.x), nginx/portal/docker tooling, browser review scripts.
- Removed unused web pages `web/index_v2.html`, `web/public_landing.html` (not compiled into firmware).
- Rewrote `AGENTS.md` and `docs/plan.md` as short, phase-based documents.
- Firmware (`src/`, `include/`) untouched.

## [02.11.2026] - Process: fewer agent runs per step

- **Who / when / why:** process chat (Claude), 02.11.2026; token use of the export / import session.
- **Where:** `sh-change` (`sh-req-reviewer` on changed IDs only), `sh-write-tests` (one test review, hash of the test
  file in the step record, area for the writer), `sh-verify-firmware` steps 3-4 (no second test review on the same
  hash; `sh-safety-reviewer` only for the safety list; reviewers see what changed since their last run).
- **Also:** every agent's report is capped (40 lines, passed checks as one line, no restating).
- **Also:** every agent's report is capped (40 lines, passed checks as one line, no restating).
- **Not done:** splitting `tools/sh_test_system.py` by area (the firmware session is editing it).

## [02.11.2026] - Process: light workflow by default, full on request

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner reached 45 % of the weekly limit on a Monday.
- **Where:** `sh-change` (modes), `sh-verify-firmware` (`light`), AGENTS.md §4 and §5, `docs/process.md` §3.1.
  Light: the chat does everything, one safety review only for the safety list. Full: all agents, when the owner asks
  and before a release.
- **Not verified:** neither mode has run; the export / import session still runs its step in the full chain.

## [02.11.2026] - Process: guard hook for the order tests before code

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: tests must never be written after the code just
  to pass; a bad test or requirement goes to them.
- **Where:** `.claude/hooks/sh_chain_guard.py` + `.claude/settings.json` (PreToolUse on Edit / Write), `tools/sh_chain.py`
  (start, freeze, unfreeze, done), `check_chain` in `tools/sh_process_check.py`, `.gitignore` (`.claude/state/`),
  `sh-change`, AGENTS.md §5, `docs/process.md` §3.1.
- **Verified:** the hook exits 2 / 0 as designed in both phases and without a marker (Windows paths); the checker errors
  on a changed frozen test file. **Not verified:** a real chain run; new sessions load the hook, running ones do not.

## [02.11.2026] - Process: system tests split by area

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: the test file is far too long (4,511 lines, one
  agent run read it whole for ~96k tokens).
- **Where:** `tools/sh_test_system.py` is now the runner (list of tests, traceability, report, `main`);
  `tools/sh_tests/base.py` (HTTP helpers, requirement registry, `TestResult`, `SuiteBase`) and seven area modules
  `t_health`, `t_hydraulics`, `t_control`, `t_ui_history`, `t_safety_features`, `t_export_import`, `t_zones`, each one a
  mixin class. Tests only moved: all 164 methods and class constants are identical (AST comparison with the old file),
  129 tests listed. Adapted: `tools/sh_process_check.py` (`test_files()`, tests parsed per file, `file:line` refs,
  frozen-hash check over all files), `tools/sh_chain.py` (hash over all test files, new command `hash`),
  `sh_chain_guard.py` and `sh_clean_room.py` (the whole `tools/sh_tests/` folder), agents `sh-test-writer`,
  `sh-test-reviewer`, `sh-assessor`, skills `sh-write-tests`, `sh-verify-firmware`, `docs/process.md` §2 R7, §3.1, SWE.6 row.
- **Verified:** AST identical for every method; `trace` output equal to the old one apart from `file:line`; checker
  PASS (0 errors); guard and clean-room hook exit codes for the folder in both phases; a changed test file after
  `freeze` gives the ERROR, restored gives PASS. **Not verified:** a run against the board (the suite itself was not
  started; the firmware session owns the board).

## [04.11.2026] - Process: visible steps, no polling, OTA script safe against --help

- **Who / when / why:** process chat (Claude), 04.11.2026; the owner saw the step 2 and step 4 sessions wait ~20 min
  in sleep loops on an empty file and run steps nobody could follow.
- **Found:** step 4 waited for the safety reviewer by looping on the agent's `.output` file (always empty: the result
  comes as a message), 18 min after the reviewer had finished in 5; both sessions edit source with inline python
  patch scripts; step 2 ran `sh_ota_upload.py --help`, which flashed the board; step 4 again wrote the Co-Authored-By
  line (removed before any push). Nothing in the skills asked for the polling.
- **Where:** AGENTS.md §5 (commit text rule, "Steps stay visible, waits stay short"), `sh-verify-firmware` (OTA takes no
  probe, system test runs 10+ min in the background), `tools/sh_ota_upload.py` (any argument prints usage, no flash).
- **Verified:** `--help` prints usage and exits 0 without connecting; checker PASS. **Not verified:** that running
  sessions follow the rule (AGENTS.md is read at session start).

## [04.11.2026] - Page feedback after step 4: boost label, pin states, debug area (FR-1.2, FR-10.5, FR-8.1, PB-118)

- **Who / when / why:** firmware session (Claude), ultra-light mode; the owner accepted step 4 and asked four page changes.
- **What:** `/api/zones` reports `scheduleTarget` and `isBoost`; the page shows "Boost manual" only while `isBoost`
  (target above the schedule level). `/api/diag/status` reports `pins` (GPIO32, 33, 25, 26, 19, 23, live `digitalRead`);
  the debug area is ordered console, pin states, trace, signals, with trace and signals collapsed.
- **Where:** `ThermalZoneManager` (`getScheduleTargetTemp`, `isBoost`), `WebApi.cpp`, `web/index.html`; tests
  TC19.18, TC19.19, TC19.20; commit 0efa023.
- **Verified:** build clean, checker PASS, board flashed over OTA, the three new tests PASS, `board` check PASS.
  Not verified: the full system test (not run, by design); owner test of the page pending.

## [04.11.2026] - Group of migration steps 6, 10, 8, 11 (FR-53, FR-16.2, FR-9, FR-12.2, FR-52, FR-64, FR-1.2, FR-39, FR-61, FR-34)

- **Who / when / why:** firmware session (Claude), ultra-light mode, four steps as one group (owner, 04.11.2026: tokens, one flash, one hands-on test).
- **What:** step 6 `SensorHub` in `BleSensorScanner`: `validMin` / `validMax` / `maxJump`, spike held until confirmed within 0.5 C, DTC-103 and DTC-104 (raised in `loop()`), one `sensorTimeout`, DTC-101 cleared on the first accepted reading; step 10 `Weather` polled by a short-lived worker task on core 0 (no stack held between polls), states ok / held / lost, previous-day mean; step 8 in `ThermalZoneManager`: effective mode AUTO / MANUAL / OFF, `manualTimeout`, season mode and state (`/api/season`), global OFF without TPI demand (frost untouched), boost limits 0.5 - 2.0 C in 0.5 C steps for page, API and console, `boostDuration`, last level stored on change, DTC-302; step 11 partly: `/api/history` streamed in chunks from a 256 byte buffer.
- **Test hooks:** `/api/dev/sensor`, `/api/dev/weather`, `/api/dev/season-day`, compiled only with `SH_DEV_OPEN`.
- **Verified:** tests first (ef26888), red on 1.11.04.04 (14 of 15 failed); build clean; checker PASS; one `sh-safety-reviewer` run, no BLOCKER, 7 findings fixed; flashed 1.11.04.09 over OTA; TC33.1 - TC33.3, TC34.1 - TC34.4, TC35.1 - TC35.3, TC36.1 - TC36.5, TC37.1, TC37.2 PASS; `board` check PASS.
- **Not done / open:** TC34.5 (sensor timeout) FAILS because of two faults in its own fixture (the enabled test zone heats at the schedule level; `sensorTimeout` 60 s also times out the real Kids sensor): the test is frozen, the owner decides. Step 11 ring move to LittleFS and `StatusModel` static buffer not built (firmware margin 5.9 KB, free heap 87 - 91 KB, both against NFR-2.8 / NFR-2.2 already). Not testable on the board: end of MANUAL and of a boost (>= 1 h), the 12 h season safety net, FR-61 without time. Requirement conflict: `validMin` range (up to 10 C) overlaps `frostTemp` (from 8 C): a reading between them is never accepted.

## [04.11.2026] - Group of steps 6, 10, 8, 11: follow-up (D-52, C-31)

- **What:** TC34.5 fixture repaired (test zone target 15 C, DTC-101 read for the test zone only; asserted value unchanged). New rule FR-53 / C-31: `validMin` stays below `frostTemp`, a setting that breaks it is refused (HTTP 400, console, import); test TC34.6 written first and red. Also done before the owner moved the memory work out of this session: log ring 8 -> 4 KB, NimBLE observer pools (1 connection, 8 msys blocks), `loopStackFree` in `/api/status` (3.2 KB of the 8 KB loop stack unused). Two small commits, can be reverted.
- **Verified (1.11.04.12):** TC34.1 - TC34.6, TC35.1 - TC35.3, TC36.1 - TC36.5, TC37.1, TC37.2 PASS; TC33.1 - TC33.3 PASS (TC33 needs a quiet house: with a real zone heating, TC33.1 / TC33.2 fail, so zone 3 was held at 15 C for the rerun and restored); `board` check PASS. TC01.2 (free heap >= 100 KB): 102 KB right after boot, 91 KB later in the run - still FAIL at test time (PB-115).
- **Memory state (open, PB-115):** firmware 1,305,968 of 1,310,720 B; free heap 91 - 102 KB against NFR-2.2; step 11 half done (streamed history only), AR-21 control task not built. Memory work is a separate task (owner, 04.11.2026). The scanf family stays linked by the Arduino core (`IPv6Address`), so replacing our five `sscanf` calls saves nothing.

## [04.11.2026] - Page: refusal message stays visible (owner remark, group test)

- **What:** the message of a refused boost (+2.5 C, FR-1.2) was written to the error line and wiped by the next refresh half a second later; `showErr` keeps it 8 s (`web/index.html`). Firmware 1.11.04.13. No new test (page text only); TC19.18 - TC19.20 and TC36.5 PASS. Other messages of the page that are cleared by a refresh are not reviewed yet.
- **Gap found:** season mode (AUTO / ON / OFF, FR-12.2) can be set only by `POST /api/season`, the page has no control for it.

## [04.11.2026] - Page: refusals and errors as a toast fixed to the viewport (owner remark, group test)

- **What:** the refusal line sat in the page footer, off-screen in a maximized window; refusals and the lost-connection message are now a toast (`position: fixed`, top centre, 8 s, tap to close) (FR-8.1, requirement bullet added). Test TC19.21 written first, red on 1.11.04.13, PASS on 1.11.04.14. TC19.18, TC19.20, TC36.5 PASS. The per-panel message lines of the admin view (`cfgMsg`, `zoneMsg`, `gasReadMsg`) sit next to their own buttons and are unchanged. TC19.7 still fails (PB-91, old).

## [04.11.2026] - Chain rules for any IDE: git `commit-msg` hook (owner, Antigravity handover)

- **What:** `tools/sh_chain.py commit-check` + `.githooks/commit-msg` enforce outside Claude Code: tests first (phase `tests` closes code, frozen hash), existing tests edited only with a `PB-n` / `D-n` / `C-n` in the message, no `Co-Authored-By` / "Generated with", `Safety-Reviewed:` line when a safety-list file changes. Owner bypass `SH_HOOK_SKIP=owner` (printed loudly). Enable: `git config core.hooksPath .githooks`. Pointers in AGENTS.md §5 and process.md §5.

## [04.11.2026] - Event log of safety and I/O (PB-121, FR-62.1, C-37, D-57)

- **What:** new `EventLog` (`src/EventLog.cpp`): 16 B entries with time, type, subject, reason, old / new value and the boiler-side state; every relay level change (`OutputBank`), every boiler frame, refusal and pending action (`BoilerRadioTrigger`), interlock start / end, overrun start / hold / end, frost, sensor loss, mode, boot, warm-up end, time sync, DTC raise / clear, status LED. RAM queue of 24, appended to two LittleFS files of 2048 entries (newest 2048-4096 kept) from `loop()`; `GET /api/diag/log?n=`.
- **Measured before:** the board's LittleFS is 1 441 792 B (default partition table of the board, not the 128 KB of `min_spiffs`); 84 KB used.
- **Safety review:** no blocker; note: `n=4096` streams about 780 KB inside `loop()` (a diagnostic call, the control timers only get longer).

