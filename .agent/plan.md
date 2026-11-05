# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (ASPICE MAN.3). Status is tracked there.

## Current Phase: Step 7 (ZoneController & FaultInjector)

- **Active State**: Step 5 (`BoilerLink`) accepted on board (`docs/acceptance.md`). Transitioned to Step 7.
- **Goal of Step 7**: Decouple `ZoneController` (L4 control layer) from `ThermalZoneManager`, implement learned duty per outdoor temperature band (7 bands, AR-12), implement FR-51 sensor-loss fallback duty (fixes PB-23, PB-61), and build `FaultInjector` (NFR-5.5, fixes PB-95).

## Stage Tracker (Execution via `/sh-change`)
- [x] Step 5 Implementation & SWE.5-TC42 tests (100% PASS on board)
- [x] Step 5 Owner hands-on test & acceptance recorded (`docs/acceptance.md`)
- [x] Step 7 Stage 1: Requirements & Architecture alignment (`docs/requirements_and_features.md`, `docs/architecture.md` §7.1, §11, §18)
- [x] Step 7 Stage 2: Tests First (`tools/sh_tests/t_zone_controller.py`, SWE.5-TC43 suite, Red Run on board, `tools/sh_chain.py freeze`)
- [x] Step 7 Stage 3: Implementation (`include/ZoneController.h`, `src/ZoneController.cpp`, `include/FaultInjector.h`, `src/FaultInjector.cpp`, integration in `ThermalZoneManager`)
- [x] Step 7 Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py`)
- [x] Step 7 Stage 5: Flash OTA & Board tests (`tools/sh_ota_upload.py`, `python tools/sh_test_system.py --only tc43`, `python tools/sh_process_check.py board`)
- [x] Step 7 Stage 6: Documentation, plan status, changelog, and commit (`docs/plan.md`, `docs/changelog.md`, activeContext, `sh_chain.py done`)
