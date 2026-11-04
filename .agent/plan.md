# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.

## Original request (verbatim)
"Pt raspunsul anterior: ALL OK
Pt ultimul tau raspuns: da, implementam ajustarea cu geamul deschis vizual, dar trebuie sa fie o optiune. adica e pentru incalzire in pardoseala. pentru calorifere, trebuie oprita si caldura pe acea zona."

## Stage tracker
- [ ] Stage 1: Requirements update (`docs/requirements_and_features.md` FR-40, §6 table for `heatingType`)
- [ ] Stage 2: Chain start & Tests First (`tools/sh_chain.py start`, write tests for `heatingType` in `tools/sh_tests/`, verify red run on board, `sh_chain.py freeze`)
- [ ] Stage 3: Implementation (`ParamStore`, `ThermalZoneManager`, `WebApi`, `web/index.html`)
- [ ] Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py`)
- [ ] Stage 5: Flash OTA & Board tests (`tools/sh_ota_upload.py`, test affected against `192.168.1.20`)
- [ ] Stage 6: Documentation, commit and chain completion (`docs/changelog.md`, activeContext, `sh_chain.py done`)

## Verification steps
1. Requirements §6 and FR-40 updated.
2. `tools/sh_chain.py` starts and freezes tests.
3. Tests fail against current board (red run) and pass after flash.
4. Firmware builds clean with `pio run` and respects OTA flash margin (< 1 306 624 B).
5. `tools/sh_process_check.py` reports 0 errors.
6. Board system tests pass against `192.168.1.20`.
