# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.

## Original request (verbatim)
"da, adaptive."

## Stage tracker
- [x] Stage 1: Requirements update (`docs/requirements_and_features.md` FR-40 adaptive window detection)
- [x] Stage 2: Chain start & Tests First (`tools/sh_chain.py start`, write `SWE.5-TC26.7` test in `tools/sh_tests/t_safety_features.py`, verify red run on board, `sh_chain.py freeze`)
- [x] Stage 3: Implementation (`WindowDetectionState` with 15 buckets, fast 5m + adaptive 15m evaluation in `ThermalZoneManager.cpp`, adaptive rebound threshold)
- [x] Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py`)
- [x] Stage 5: Flash OTA & Board tests (`tools/sh_ota_upload.py`, test affected TC26 against `192.168.1.20`)
- [x] Stage 6: Documentation, commit and chain completion (`docs/changelog.md`, activeContext, `sh_chain.py done`)

## Verification steps
1. FR-40 updated with adaptive window detection rule.
2. `tools/sh_chain.py` starts and freezes tests.
3. Test fails against current board (red run) and passes after flash.
4. Firmware builds clean with `pio run` and respects OTA flash margin (< 1 306 624 B).
5. `tools/sh_process_check.py` reports 0 errors.
6. Board system tests pass against `192.168.1.20`.
