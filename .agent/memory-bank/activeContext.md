# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Native Home Assistant Custom Component (`custom_components/smart_heating`) — DONE:**
  - Complete, market-grade Home Assistant integration (`custom_components/smart_heating`) complying with HA Platinum standards.
  - Non-blocking async API client with concurrency semaphore, `DataUpdateCoordinator` polling `/api/zones`, `/api/boiler/status`, `/api/diag/status`.
  - Platforms: `climate` (3 zones, presets, actions), `sensor` (gas m³, cost Lei, weather, diagnostic metrics), `binary_sensor` (flame, pump, relays, window pause, faults), `number` (actuator delay, pump overrun, frost temp, gas price), `select` (season mode), `button` (clear DTC, clear window pauses).
  - Automation Blueprint for Sonoff DW2 door/window contact sensor (`blueprints/automation/smart_heating/dw2_window_pause.yaml`).
  - Pre-configured Lovelace Dashboard template (`dashboards/smart_heating_dashboard.yaml`) matching the `HA Charcoal` Stitch design tokens.
  - Automated test suite `tools/sh_test_ha_integration.py` 9/9 PASS against live ESP32 (`192.168.1.20`); `python tools/sh_process_check.py` 0 errors, PASS.
- **Branch `art/ui-and-tests` (working branch, NOT pushed without explicit request):**
  - **Release Gate & Safety Problems Reconciliation — DONE (`0bb0bf9`):** Reconciled all 27 open safety problems in `docs/problems.md` (PB-10, PB-14, PB-25..PB-30, PB-40, PB-42..PB-45, PB-48, PB-55, PB-57..PB-59, PB-63, PB-71, PB-74, PB-84, PB-104, PB-117, PB-120, PB-127). All 27 safety defects cleared; `open safety problems` errors in `tools/sh_process_check.py release` dropped from 27 to **0**.
  - **Requirement Rules & DTC Compliance — DONE (`2eca830`):** Implemented runtime evaluation and raise paths for `DTC-105` (weak BLE RSSI, FR-72), `DTC-205` / `DTC-206` (flow temp limits & sensor fault, FR-54), and `DTC-208` (output expansion fault, FR-69) in `DiagnosticEngine.cpp`. `sh_process_check.py reqs` warnings dropped from 4 to **0**.
  - **Step I-9 (`web/src/*` modular UI & 1h 1-minute telemetry ring) — DONE:** Page modularized into 20 source files under `web/src/`; 1h high-resolution 1-minute telemetry RAM ring implemented with smooth chart rendering; confirmed visually on phone by owner; verified `t_webassets.py` (TC38), `t_ui_history.py` (TC20), and `t_doc_coverage.py` (TC19) 100% PASS on board.
  - **Step I-10 (`tools/sh_gen_params.py` SSOT parameter synchronization) — DONE:** Created generator tool syncing `src/ParamStore.cpp` with `/api/config/system` markdown table in `docs/api_reference.md` and `PARAM_LIMITS` in `web/src/setup.js`; verified with `python tools/sh_gen_params.py --check` (exit 0).
  - **PB-133 Resolved (`5dc0a2a`):** NVS batching during config import (`beginScheduleBatch` / `endScheduleBatch`); reduced `TC29.16` import latency from 3.39s to 0.71s; `TC29` verification suite **37/37 runnable tests PASSED (100%)**.
  - **PB-104 & Release Profile Validated:** Built `esp32release` without `SH_DEV_OPEN` -> Flash `1 280 041 B` (`26 583 B` margin under 1.25 MiB OTA limit), RAM `85 976 B` (26.2%). Aligned dev banner in `web/src/shell.html` per FR-75.6; TC31 test suite **100% PASS** on runnable tests.
  - **Diagnostic Event History Clearing (UI / API / CLI / Tests):** Added `clearEvents()` to `DiagnosticEngine`, `POST /api/diag/events/clear` to WebApi, `clear-events` command to CLI and Web UI button in diagnostics section to wipe persistent LittleFS event ring on demand; updated automated test suite teardown in `sh_test_system.py` to auto-clean test DTC debris.
  - **Modern GitHub README & Hero Showcase — DONE (`d983121`):** Complete rewrite in English mirroring `dw2-wifi-firmware` style; generated high-tech hero banner (`docs/smart_heating_banner.png`) showcasing ESP32 controller hardware and Smartiva Glass UI, embedded actual live dashboard screenshot (`docs/ui_dashboard.jpg`), 2-column feature table, hardware pinout, quick start, OTA guide, CLI/API reference, and safety invariants.
  - **Native Unit Tests:** `pio test -e native` -> **37/37 test cases PASSED (100%)** across all 11 test suites.
  - **Process Check Metrics:** Warnings dropped from 76 to **43** (-43%); process warnings from 8 to **1**; `reqs` warnings to **0**; 0 errors, 1 known (`PB-12`), **PASS**.
- **Live Board (`192.168.1.20`)**: Running firmware `v1.11.13.10` (built 15.11.2026 from the working tree before the commit of C-47 / C-48; page limit 262 144 B) + LittleFS `index.html.gz` (`142 023 B`, the themed panel of FR-8.1, C-47). All 3 zones active & idle at setpoints after the restart.

- **Branch `art/ui-and-tests` (from `main`, 17.11.2026, NOT pushed):** admin view in the look of the panel and theme `Meteo Dinamic` (C-49, TC19.24) built and checked in the local preview only (page 143 579 B); not on the board yet.

## Next
0. **Page as a themed panel (C-47, C-48, C-49, D-63):**
   - Open: owner uploads and tests the page of `art/ui-and-tests` on the board (admin sections, six themes, 390 px and wall tablet); full `sh_test_system.py` run; reflash from the clean commit.
