# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 12 (`Maintenance` FR-19, FR-60, NFR-1.8)**: Complete, flashed and verified on board with SWE.5-TC45.1..TC45.4 100% PASS. Implemented `include/Maintenance.h` & `src/Maintenance.cpp`: `canStartOta()` (QuietGate blocks OTA while boiler flame is ON or pending overrun), `checkAutomaticRollback()` (app validity and rollback verification), `updateValveExercise()` (FR-19 weekly 5 min exercise on idle valves), and `runSelfTest()` (FR-60 diagnostics across 6 subsystems). Exposed `/api/maint/status`, `/api/maint/selftest`, `/api/maint/exercise`. Added `exerciseIdleDays` [7..14] and `selfTestBurn` [5..20] to ParamStore and `/api/config/system`. Free heap ~93 KB, internal ~93 KB, flash margin ~28 KB.
- **Step 11 (Telemetry Rings to LittleFS & StatusModel)**: Complete, flashed and verified on board. `_ring24h` and `_ring5d` removed from RAM; history streamed from LittleFS; StatusModel static buffer implemented. SWE.5-TC37.1..TC37.2 100% PASS.
- **Web UI & Telemetry Polishing**: Built, tested, and flashed to board. Admin password verification modal (`POST /api/auth/verify`) requiring credentials to open Admin; offline interval detection; continuous amber event boxes; TPI duty formatting.
- **Accepted Steps (1–11)**: All validated on board.
- **Tooling & Skills Consolidation**:
  - Unified `/sh-*` skills, zero orphan files invariant enforced.
  - Git hook `.githooks/commit-msg` active.

## Next
1. **Step 13 (`FlowInput` + `FlowLimit`)**: waits for owner flow-probe decision (risk R-4 / DS18B20 library approval).
2. **Phase 4 (Step 14)**: Advisors (`PlantMonitor`, `HeatUpModel`, `SmartWindow`).
