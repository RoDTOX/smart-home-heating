# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **NVS Schema Migration & Heap Hysteresis Fix (C-40)**: Complete, flashed and verified on board. Added `ZoneRecordV1` backward-compatible migration in `ParamStore` (prevents loss of zone commissioning across OTA schema expansions) and fixed heap alert clearing logic in `DiagnosticEngine` (`dtcCount: 0`).
- **Step 14 Part C (`SolarThrottle` FR-70)**: Complete, flashed and verified on board with SWE.5-TC48.1..TC48.4 100% PASS. Implemented `SolarThrottle` in Layer 4 (FR-70):
  - Monitors forecast solar irradiance (`solarLimit` [200..800 W/m², default 400]) and applies `solarReduction` [10..60%, default 30%] to TPI duty multiplier for zones with `solarGain: true`.
  - Added "Însorire (Sud/Vest)" checkbox column in Section 4.1 (`#setup` table in `web/index.html`) and mapped to `solarGain` boolean in `/api/config/import` and `/api/setup/map-zone`.
  - Fail-safe fallback: weather loss or missing forecast immediately bypasses reduction (0% reduction).
  - Internal heap healthy (~93.6 KB free, OTA slot margin ~22 KB).
- **Step 14 Part B (`HeatUpModel` FR-48, FR-67)**: Complete, flashed and verified on board with SWE.5-TC47.1..TC47.4 100% PASS.
- **Step 14 Part A (`PlantMonitor` FR-56, FR-57)**: Complete, flashed and verified on board with SWE.5-TC46.1..TC46.4 100% PASS.
- **Accepted Steps (1–12)**: All validated on board.

## Next
1. **Step 14 Part D (`CycleAligner` FR-63 & `SmartWindow` FR-40.2)**: Synchronize zone PWM cycles to minimize boiler starts; dynamic window opening detection.
2. **Step 14 Part E (`HumidityComp` FR-71)**: Perceived comfort compensation via relative humidity.
