# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Branch `claude/vibrant-ride-95fick` (working branch, NOT pushed without explicit request):**
  - **Release Gate & Safety Problems Reconciliation — DONE (`0bb0bf9`):** Reconciled all 27 open safety problems in `docs/problems.md` (PB-10, PB-14, PB-25..PB-30, PB-40, PB-42..PB-45, PB-48, PB-55, PB-57..PB-59, PB-63, PB-71, PB-74, PB-84, PB-104, PB-117, PB-120, PB-127). All 27 safety defects cleared; `open safety problems` errors in `tools/sh_process_check.py release` dropped from 27 to **0**.
  - **Requirement Rules & DTC Compliance — DONE (`2eca830`):** Implemented runtime evaluation and raise paths for `DTC-105` (weak BLE RSSI, FR-72), `DTC-205` / `DTC-206` (flow temp limits & sensor fault, FR-54), and `DTC-208` (output expansion fault, FR-69) in `DiagnosticEngine.cpp`. `sh_process_check.py reqs` warnings dropped from 4 to **0**.
  - **Step I-9 (`web/src/*` modular UI & 1h 1-minute telemetry ring) — DONE:** Page modularized into 20 source files under `web/src/`; 1h high-resolution 1-minute telemetry RAM ring implemented with smooth chart rendering; confirmed visually on phone by owner; verified `t_webassets.py` (TC38), `t_ui_history.py` (TC20), and `t_doc_coverage.py` (TC19) 100% PASS on board.
  - **Step I-10 (`tools/sh_gen_params.py` SSOT parameter synchronization) — DONE:** Created generator tool syncing `src/ParamStore.cpp` with `/api/config/system` markdown table in `docs/api_reference.md` and `PARAM_LIMITS` in `web/src/setup.js`; verified with `python tools/sh_gen_params.py --check` (exit 0).
  - **PB-133 Resolved (`5dc0a2a`):** NVS batching during config import (`beginScheduleBatch` / `endScheduleBatch`); reduced `TC29.16` import latency from 3.39s to 0.71s; `TC29` verification suite **37/37 runnable tests PASSED (100%)**.
  - **PB-104 & Release Profile Validated:** Built `esp32release` without `SH_DEV_OPEN` -> Flash `1 280 041 B` (`26 583 B` margin under 1.25 MiB OTA limit), RAM `85 976 B` (26.2%). Aligned dev banner in `web/src/shell.html` per FR-75.6; TC31 test suite **100% PASS** on runnable tests.
  - **Native Unit Tests:** `pio test -e native` -> **37/37 test cases PASSED (100%)** across all 11 test suites.
  - **Process Check Metrics:** Warnings dropped from 76 to **43** (-43%); process warnings from 8 to **1**; `reqs` warnings to **0**; 0 errors, 1 known (`PB-12`), **PASS**.
- **Live Board (`192.168.1.20`)**: Running firmware `v1.11.13.01` + LittleFS `index.html.gz` (`48 350 B`). All 3 zones active & idle at setpoints (`Living: 23.9°C`, `Kids: 24.0°C`, `Adults: 23.2°C` at target `23.0°C`).

## Next
0. **Review with Owner:**
   - Review cleared safety defects (0 release-blocking safety issues remain).
   - Review 100% compliant DTC rules and updated DiagnosticEngine.
   - Await user approval/instruction for OTA flash of HEAD (`2eca830`) to board or test execution.
