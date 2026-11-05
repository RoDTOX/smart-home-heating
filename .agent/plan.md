# Step 12 — Maintenance: OTA Quiet Gate, Rollback, Valve Exercise, Full Self-Test

## Original request (verbatim)
Continuam cu pasul 12.

## Goal (architecture §18, step 12)
Close G24 (anti-seize 20s weekly), G25 (OTA quiet gate & rollback), G26 (full self-test).
Requirements: FR-19, FR-60, NFR-1.8.

## Design
1. `include/Maintenance.h` & `src/Maintenance.cpp`:
   - `QuietGate`: checks if boiler flame is ON; rejects or defers OTA upload until boiler is confirmed OFF and overrun ended.
   - `Exercise`: weekly 20s exercise (FR-19) on unexercised zone valves, records epoch in NVS.
   - `SelfTest`: validates NVS integrity, LittleFS filesystem, radio driver, sensors and task health.
2. WebApi integration:
   - Protect `/update` upload handler with `QuietGate::canStartOta()`.
   - Add `/api/maint/selftest` endpoint.
   - Add `/api/maint/exercise` manual trigger.

## Stage tracker
- [x] 1. Write tests-first in tools/sh_tests/t_maintenance.py
- [x] 2. Implement include/Maintenance.h & src/Maintenance.cpp
- [x] 3. Integrate QuietGate into OTA upload in main.cpp / WebApi.cpp
- [x] 4. Integrate weekly valve exercise into ControlTask/ThermalZoneManager
- [x] 5. Implement full self-test contract
- [x] 6. Build (pio run), process check, and flash to board
- [x] 7. Verify SWE.5-TC maintenance test suite 100% PASS
