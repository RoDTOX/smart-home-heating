# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 14 Part D (`CycleAligner` FR-63)**: Complete, flashed and verified on board with SWE.5-TC49.1..TC49.4 100% PASS (commit pending). Implemented `CycleAligner` in Layer 4 (FR-63):
  - Synchronized top-of-hour TPI cycle alignment across zones to maximize valve overlap and prevent burner short-cycling.
  - `minLoops` parameter bounds [1..7 circuits, default 3] and per-zone `circuits` [1..16, default 1, reference 3/2/2].
  - Grouping and extension advisor logic exposed via `GET /api/cycles/status`.
  - Added `ZoneRecordV2` (90 B) legacy reader in `ParamStore`, automatically migrating to `ZoneRecord` (91 B) on read without losing configuration.
  - Board operational with 3 zones, 3 BLE sensors active, 0 DTCs.
- **NVS Schema Migration & Heap Hysteresis Fix (C-40)**: Complete, flashed and verified on board. Added `ZoneRecordV1` backward-compatible migration in `ParamStore` and fixed heap alert clearing logic in `DiagnosticEngine` (`dtcCount: 0`).
- **Step 14 Part C (`SolarThrottle` FR-70)**: Complete, flashed and verified on board with SWE.5-TC48.1..TC48.4 100% PASS.
- **Step 14 Part B (`HeatUpModel` FR-48, FR-67)**: Complete, flashed and verified on board with SWE.5-TC47.1..TC47.4 100% PASS.
- **Step 14 Part A (`PlantMonitor` FR-56, FR-57)**: Complete, flashed and verified on board with SWE.5-TC46.1..TC46.4 100% PASS.
- **Accepted Steps (1–12)**: All validated on board.

## Next
1. **Step 14 Part E (`SmartWindow` FR-40.2 & `HumidityComp` FR-71)**: Dynamic window opening detection via slope / absolute humidity and perceived comfort compensation via relative humidity.
