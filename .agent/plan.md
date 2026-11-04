# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.

## Original request (verbatim)
"Go" (Context: Pasul 5 - BoilerLink interface, Rf433Link, ContactLink, hbInterval, DTC-207, FR-5.1, FR-17.1..7, FR-55, FR-57)

## Stage tracker
- [x] Stage 1: Requirements & Architecture alignment (`docs/requirements_and_features.md`, `docs/architecture.md` §7.4, §12.2, §18)
- [x] Stage 2: Tests First (`tools/sh_tests/t_boiler_link.py` Group 42 `SWE.5-TC42.1`..`TC42.4`, verify red run on board, `tools/sh_chain.py freeze`)
- [x] Stage 3: Implementation (`include/BoilerLink.h`, `src/BoilerLink.cpp`, `include/BoilerRadioTrigger.h`, `src/BoilerRadioTrigger.cpp`, `include/ParamStore.h`, `src/ParamStore.cpp`, `src/DiagnosticEngine.cpp`)
- [x] Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py`)
- [x] Stage 5: Flash OTA & Board tests (`tools/sh_ota_upload.py`, `python tools/sh_test_system.py --only tc42`, `python tools/sh_process_check.py board`)
- [x] Stage 6: Documentation, plan status, changelog, and commit (`docs/plan.md`, `docs/changelog.md`, activeContext, `sh_chain.py done`)

## Verification steps
1. `BoilerLink` abstraction defined with `Rf433Link` and `ContactLink`.
2. `SWE.5-TC42` suite added and verified RED RUN on current firmware.
3. Tests frozen with `tools/sh_chain.py freeze`.
4. Firmware compiles cleanly within OTA flash limit (working margin >= 40 KB).
5. Board tests pass 100% on live hardware (`192.168.1.20`).
6. DTC-207 raised when heartbeat disabled, cleared when enabled.
7. `tools/sh_process_check.py` reports 0 errors.
