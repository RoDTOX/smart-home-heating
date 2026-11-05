# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (ASPICE MAN.3). Status is tracked there.

## Current Phase: Transition from Step 5 to Step 7

- **Active State**: Step 5 (`BoilerLink`) code & SWE.5-TC42 suite complete and verified 100% PASS on board. Owner hands-on test pending.
- **Immediate Next Step**: Step 7 (`ZoneController` - TPI, outdoor compensation, fallback duty FR-51).

## Stage Tracker (Execution via `/sh-change`)
- [x] Step 5 Implementation & SWE.5-TC42 tests (100% PASS on board)
- [ ] Owner hands-on test of Step 5 on physical plant (`docs/acceptance.md`)
- [ ] Step 7 Requirements & Architecture alignment (`docs/requirements_and_features.md`, `docs/architecture.md` §7.1, §18)
- [ ] Step 7 Tests First (TDD suite in `tools/sh_tests/`)
- [ ] Step 7 Implementation (`ZoneController`, learned duty, fault duty)
- [ ] Step 7 Local gates (`pio run`, `python tools/sh_process_check.py`)
- [ ] Step 7 Flash OTA & Board tests (`tools/sh_test_system.py`)
