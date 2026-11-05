# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 14 Part E (`HumidityComp` FR-71)**: Complete, flashed and verified on board with SWE.5-TC50.1..TC50.4 100% PASS (commit pending). Implemented `HumidityComp` in Layer 4 (FR-71):
  - At room relative humidity $\ge 60\%$, automatically lowers control target by up to 0.5 °C for zones with `humidityComp: true`.
  - Fallback: missing or invalid sensor reading results in 0.0 °C reduction.
  - Added `ZoneRecordV3` (91 B) legacy reader in `ParamStore`, automatically migrating to 92 B on read without configuration loss.
  - Exposed status report via `GET /api/humidity/status`.
- **Step 14 Part D (`CycleAligner` FR-63)**: Complete, flashed and verified on board with SWE.5-TC49.1..TC49.4 100% PASS (commit `6097c99`).
- **NVS Schema Migration & Heap Hysteresis Fix (C-40)**: Complete, flashed and verified on board.
- **Step 14 Part C (`SolarThrottle` FR-70)**: Complete, flashed and verified on board with SWE.5-TC48.1..TC48.4 100% PASS.
- **Step 14 Part B (`HeatUpModel` FR-48, FR-67)**: Complete, flashed and verified on board with SWE.5-TC47.1..TC47.4 100% PASS.
- **Step 14 Part A (`PlantMonitor` FR-56, FR-57)**: Complete, flashed and verified on board with SWE.5-TC46.1..TC46.4 100% PASS.
- **Accepted Steps (1–12)**: All validated on board.

## Next
1. **Step 14 Part F (`SmartWindow` FR-40.2)**: Dynamic window opening detection via slope / absolute humidity (Risk R-5: calibration traces).
