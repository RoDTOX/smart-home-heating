# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 14 Part A (`PlantMonitor` FR-56, FR-57)**: Complete, flashed and verified on board with SWE.5-TC46.1..TC46.4 100% PASS. Implemented `include/PlantMonitor.h` & `src/PlantMonitor.cpp` as Layer 4 lightweight observer (execution <= 2 ms per tick, memory ~500 B):
  - FR-56: Zone overheat check (`overheatMargin` [1.0..5.0 °C, def 2.0], `overheatTime` [30..180 min, def 60]) with commanded closed valve raising DTC-203 `ERR_ZONE_OVERHEAT`.
  - FR-57: Zone heating without effect (`noEffectDuty` [30..100%, def 50], `noEffectTime` [1..6 h, def 3], `noEffectRise` [0.1..1.0 °C, def 0.2]) raising DTC-202 `ERR_ZONE_NO_EFFECT`.
  - FR-57: Boiler not heating with underfloor-adjusted feedback wait (30 min stagnation before single ON re-send, 60 min cumulative before raising DTC-204 `ERR_BOILER_NOT_HEATING`).
  - Added `/api/plant/status` JSON report and exposed parameters in `/api/config/system`. Live board health: internal heap ~95.5 KB, contiguous largest ~73.7 KB, OTA margin ~26 KB.
- **Step 13 (`FlowInput` + `FlowLimit`)**: Deferred per owner decision (05.11.2026); boiler mechanical setpoint <= 45 °C protects floor; hardware guidance (DS18B20 waterproof stainless probes) documented.
- **Step 12 (`Maintenance` FR-19, FR-60, NFR-1.8)**: Complete, flashed and verified on board with SWE.5-TC45.1..TC45.4 100% PASS. QuietGate, rollback, weekly valve exercise, self-test.
- **Accepted Steps (1–12)**: All validated on board.

## Next
1. **Step 14 Part B (`HeatUpModel` FR-67)**: Adaptive pre-heat lead per zone (°C/h vs outdoor temperature, learned).
2. **Step 14 Part C (`SolarThrottle` FR-70)**: Solar irradiance compensation with zone orientation/exposure configuration.
3. **Step 14 Part D (`CycleAligner` FR-63 & `SmartWindow` FR-40.2)**.
