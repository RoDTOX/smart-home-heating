# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Branch `claude/vibrant-ride-95fick` (working branch, NOT pushed without explicit request):**
  - **Step I-9 (`web/src/*` modular UI & 1h 1-minute telemetry ring) — DONE:** Page modularized into 20 source files under `web/src/`; 1h high-resolution 1-minute telemetry RAM ring implemented with smooth chart rendering; confirmed visually on phone by owner; verified `t_webassets.py` (TC38), `t_ui_history.py` (TC20), and `t_doc_coverage.py` (TC19) 100% PASS on board.
  - **Step I-10 (`tools/sh_gen_params.py` SSOT parameter synchronization) — DONE:** Created generator tool syncing `src/ParamStore.cpp` with `/api/config/system` markdown table in `docs/api_reference.md` and `PARAM_LIMITS` in `web/src/setup.js`; verified with `python tools/sh_gen_params.py --check` (exit 0).
  - **PB-133 Resolved (`5dc0a2a`):** NVS batching during config import (`beginScheduleBatch` / `endScheduleBatch`); reduced `TC29.16` import latency from 3.39s to 0.71s; `TC29` verification suite **37/37 runnable tests PASSED (100%)**.
  - **PB-104 & Release Profile Validated:** Built `esp32release` without `SH_DEV_OPEN` -> Flash `1 280 041 B` (`26 583 B` margin under 1.25 MiB OTA limit), RAM `85 976 B` (26.2%). Aligned dev banner in `web/src/shell.html` per FR-75.6; TC31 test suite **100% PASS** on runnable tests.
- **Live Board (`192.168.1.20`)**: Running firmware `v1.11.13.01` + LittleFS `index.html.gz` (`48 350 B`). All 3 zones active & idle at setpoints (`Living: 23.9°C`, `Kids: 24.0°C`, `Adults: 23.2°C` at target `23.0°C`).

## Next
0. **Review with Owner:**
   - Display and demo verified steps I-9 and I-10.
   - Status of `PB-104` (release build validated, dev build stays open during active iteration).
   - Await explicit user instruction for `git push origin claude/vibrant-ride-95fick` when desired.
