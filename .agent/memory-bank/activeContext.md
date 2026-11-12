# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Branch `claude/vibrant-ride-95fick` (overnight work, NOT pushed, NOT flashed to board):**
  - Disabled `sh-cost-guard` microplan hook (`fd639ca`) per owner.
  - Flash deduplication (`37ee751`): `sendScheduleJson()` in `WebApi.cpp` + `appendDayMomentsJson`/`appendBoolField` in `ThermalZoneManager.cpp` brought `esp32dev` `.bin` down to `1 302 048 B` (`-3 920 B` vs `ba61543`). Clarified `docs/plan.md` `R-2` (`.bin` vs ELF Flash).
  - **Step I-4 (`SafetySupervisor`) — Code + Host Tests Done, Pending Board Acceptance:** Extracted pure `SafetySupervisor` (`include/SafetySupervisor.h`, `src/SafetySupervisor.cpp`), wired `ThermalZoneManager` to delegate `arbitrateValves`, `trackBoilerState`, `evaluateFrost`, `boilerOnPermitted`, added `test/test_safety/test_safety.cpp` (5/5 PASS; total 28/28 native tests PASS). Logged `PB-127` (`FR-4` burner guard host test deferred in `BoilerRadioTrigger`). Built `esp32dev` (`1 302 832 B`) and `esp32release` (`1 302 848 B`), both > 3.7 KB under `1 306 624 B`.
  - **Step I-9 (`web/src/*`) — Code + Build Done, Pending Board Assets Upload & Visual Check:** Split `web/index.html` into 20 files under `web/src/` + refactored `renderHistoryChart` (`prepareHistoryChartData` / `drawHistoryChart`); updated `tools/sh_build_web.py` (`44 101 B` gzip vs `44 512 B` baseline) & `tools/sh_dev_server.py`. Kept `web/index.html` synced awaiting owner confirmation before removal.
- **Live Board (`192.168.1.20`)**: Untouched overnight; still runs previous firmware until owner reviews, pushes, and flashes.

## Next
0. **Overnight sequence (`I-8` part 1 -> `I-5` `Schedule` only, if Flash margin >= 512 B and safety reviewer quota allows)** on `claude/vibrant-ride-95fick`.
1. **Owner morning review:** Decide on dropping synced `web/index.html`, upload assets + OTA flash + `python tools/sh_test_system.py` + `python tools/sh_process_check.py board` + explicit `git push` confirmation when ready.
