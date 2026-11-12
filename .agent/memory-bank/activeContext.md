# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Branch `claude/vibrant-ride-95fick` (overnight work, NOT pushed, NOT flashed to board):**
  - Disabled `sh-cost-guard` microplan hook (`fd639ca`) per owner.
  - Flash deduplication (`37ee751`): `sendScheduleJson()` in `WebApi.cpp` + `appendDayMomentsJson`/`appendBoolField` in `ThermalZoneManager.cpp` brought `esp32dev` `.bin` down to `1 302 048 B` (`-3 920 B` vs `ba61543`). Clarified `docs/plan.md` `R-2` (`.bin` vs ELF Flash).
  - **Step I-4 (`SafetySupervisor`) — Code + Host Tests Done, Pending Board Acceptance:** Extracted pure `SafetySupervisor` (`include/SafetySupervisor.h`, `src/SafetySupervisor.cpp`), wired `ThermalZoneManager` to delegate `arbitrateValves`, `trackBoilerState`, `evaluateFrost`, `boilerOnPermitted`, added `test/test_safety/test_safety.cpp` (5/5 PASS; total 28/28 native tests PASS). Logged `PB-127` (`FR-4` burner guard host test deferred in `BoilerRadioTrigger`). Built `esp32dev` (`1 302 832 B`) and `esp32release` (`1 302 848 B`), both > 3.7 KB under `1 306 624 B`.
  - **Step I-9 (`web/src/*`) — Code + Build Done, Pending Board Assets Upload & Visual Check:** Split `web/index.html` into 20 files under `web/src/` + refactored `renderHistoryChart` (`prepareHistoryChartData` / `drawHistoryChart`); updated `tools/sh_build_web.py` (`44 101 B` gzip vs `44 512 B` baseline) & `tools/sh_dev_server.py`. Dropped legacy monolithic `web/index.html` after owner confirmation.
  - **Step I-8 (part 1, `R-A9`) — Done:** Enforced pure-module isolation (`PURE_MODULES` single map in `tools/sh_process_check.py`: `0` `millis()`, `0` `Arduino.h` in headers / unguarded `.cpp`, host test required under `test/`) as ERROR (`R-A9` in `docs/architecture.md` §3.3 & `docs/process.md`); `R-A7` stays WARN until `I-7`.
  - **Step I-5 (`Schedule` `d29e1f3` + `ModeManager` `9484aaf`) — Code + Host + Board Verified:** Extracted pure `Schedule` (`test_schedule` 5/5) and `ModeManager` (`test_mode_manager` 4/4; `37/37` native total across 11 suites), `esp32dev` `1 303 744 B` / `esp32release` `1 303 760 B`, Opus safety reviews PASS, `TC12`/`TC14`/`TC16` 15/15 PASS on `9484aaf`. `ThermalZoneManager.cpp` is `1 770` lines (target `< 700` re-evaluated after `I-7`).
  - **Step I-8 (`R-A9` `2289008` + `R-A7` ERROR) — Done:** Enforced `R-A9` (`PURE_MODULES`, 10 pure modules) and `R-A7` (ERROR across L2–L4 with `R_A7_EXCEPTIONS` for `PB-131` external library boundaries); verified negative tests fail with exit 1 and clean tree passes (`0` errors).
- **Live Board (`192.168.1.20`)**: Running `v1.11.12.12` (`esp32dev` `1 286 208 B`, `20 416 B` OTA margin; 1h 1-min RAM ring + clean chart rendering; `TC20.1`/`20.2`/`20.3`/`37.1` `4/4 PASS`) + LittleFS `index.html.gz` (`44 432 B`).

## Next
0. **Owner morning review:** Decide on remaining `ThermalZoneManager.cpp` `< 700 lines` target (`1 644` lines, `3 408 B` release flash margin), check pre-existing board failures (`PB-115`, `TC09.1`, `TC35.2`, etc.), and confirm `git push` when ready.
1. **Owner morning review:** Check remaining pre-existing board failures (`PB-115`, `TC19.13`, `TC31.7`, `TC31.13`, `TC20.4`, etc.) and explicit `git push` confirmation when ready.
