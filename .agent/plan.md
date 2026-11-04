# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.

## Original request (verbatim)
"Comenzi și/sau elemente grafice le interfața web. 

Apoi pasul 5."

## Stage tracker
- [x] Stage 1: Requirements & API docs update (`docs/requirements_and_features.md` FR-59, `docs/api_reference.md` for `POST /api/config/credentials` and `ADMINPASS`)
- [x] Stage 2: Tests First (`tools/sh_tests/t_commands_auth.py` add `SWE.5-TC41.8`, verify red run on board, `tools/sh_chain.py freeze`)
- [x] Stage 3: Implementation (`src/ConsoleCommands.cpp`, `src/WebApi.cpp`, `web/index.html`)
- [x] Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py`)
- [x] Stage 5: Flash OTA & Board tests (`tools/sh_ota_upload.py`, `python tools/sh_test_system.py --only tc41`)
- [x] Stage 6: Documentation, commit and chain completion (`docs/changelog.md`, activeContext, `sh_chain.py done`)

## Verification steps
1. Requirements FR-59 and API reference updated.
2. `SWE.5-TC41.7` test written and verified RED RUN on current board.
3. Tests frozen with `tools/sh_chain.py freeze`.
4. Firmware builds cleanly with `pio run` and respects OTA flash margin.
5. Board tests pass 100% on live hardware (`192.168.1.20`).
