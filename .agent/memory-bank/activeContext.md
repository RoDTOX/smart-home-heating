# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 14 Part B (`HeatUpModel` FR-48, FR-67)**: Complete, flashed and verified on board with SWE.5-TC47.1..TC47.4 100% PASS. Implemented `include/HeatUpModel.h` & `src/HeatUpModel.cpp` as Layer 4 advisor:
  - Learns per-zone underfloor heat-up speed (°C/h) with bounded EMA filter [0.1..2.0 °C/h].
  - Evaluates pre-heat lead time using FR-48 formula $[120 + 9 \times (5 - T_{\text{out}})]$ clamped $[45..240\text{ min}]$, with exact 90 min fallback when weather data is lost or invalid.
  - Exposed `/api/heatup/status` and connected to `ThermalZoneManager::getPreHeatLeadMinutes()`.
  - Internal heap healthy (~93.8 KB free, largest contiguous block ~77.8 KB, OTA slot margin ~25 KB).
- **Step 14 Part A (`PlantMonitor` FR-56, FR-57)**: Complete, flashed and verified on board with SWE.5-TC46.1..TC46.4 100% PASS. Overheat, no-effect, and boiler stagnant detection.
- **Accepted Steps (1–12)**: All validated on board.

## Next
1. **Step 14 Part C (`SolarThrottle` FR-70)**: Solar irradiance compensation with zone orientation/exposure configuration.
2. **Step 14 Part D (`CycleAligner` FR-63 & `SmartWindow` FR-40.2)**.
