# Step 14 — Phase 4: PlantMonitor (FR-56, FR-57) & Advisors

## Original request (verbatim)
pas 13 amanat, nu am sonde de temperatura momentan, dar documenteaza ce trebuie sa cumpar, sa fie potrivit si ca pret si ca utilitate.
deschidem pasul 14.

## Goal (architecture §18, step 14; §13 PlantMonitor)
1. Document flow probe hardware recommendations (DS18B20 vs NTC, clamp-on pipe mounting, pricing/utility).
2. Implement Step 14 Part A: `PlantMonitor` (L4 observer, FR-56, FR-57):
   - FR-56: Zone overheat detection (`overheatMargin` 2°C, `overheatTime` 60 min) with commanded-closed valve -> DTC-203 `ERR_ZONE_OVERHEAT` (valve stuck open or polarity error).
   - FR-57: Heating without effect (`noEffectDuty` 50%, `noEffectTime` 3h, `noEffectRise` 0.2°C) -> DTC-202 `ERR_ZONE_NO_EFFECT`.
   - FR-57: Boiler not heating (all demanding zones stagnant for 15 min -> resend ON once -> 15 min still stagnant) -> DTC-204 `ERR_BOILER_NOT_HEATING`.
   - Pure observer in L4, execution time <= 2 ms per tick, memory <= 1 KB RAM.

## Stage tracker
- [x] 1. Document flow probes recommendation for owner
- [x] 2. Write tests-first in tools/sh_tests/t_plant_monitor.py (SWE.5-TC46)
- [x] 3. Implement include/PlantMonitor.h and src/PlantMonitor.cpp
- [x] 4. Register PlantMonitor in ControlTask / main.cpp loop & WebApi status
- [x] 5. Run process check, build clean (pio run), flash OTA, and verify 100% PASS
