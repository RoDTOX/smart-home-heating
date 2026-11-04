# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.

## Original request (verbatim)
"Da-i drumul"
Context: Pachet Extins: Stabilitate (PB-115 + PB-122) + Pasul 9 complet (Commands + autentificare WebApi FR-58/59, CORS, DTC-403) - totul într-un singur flash OTA mare.

## Stage tracker
- [ ] Stage 1: Chain start & Tests First (`tools/sh_chain.py start`, write tests for PB-115, PB-122, Step 9 auth/CORS/DTC-403, run red tests on current board, freeze)
- [ ] Stage 2: Memory & Stability fixes (PB-115 weather stack 6144 -> 4096 B, PB-122 DTC-402 dip tracking with auto-reset > 75 KB)
- [ ] Stage 3: Step 9 Implementation (`include/Commands.h`, `src/Commands.cpp`, WebApi route protection with HTTP 401, CORS restriction to own origin, DTC-403 for default credentials)
- [ ] Stage 4: Tooling & Test Suite updates (`tools/sh_esp_cli.py`, `tools/sh_tests/` updated with auth credentials)
- [ ] Stage 5: Verification & Safety review (`pio run`, `tools/sh_process_check.py`, `sh-safety-reviewer` check for safety impacts)
- [ ] Stage 6: OTA Flash & Live board testing (`tools/sh_ota_upload.py`, run system tests against board 192.168.1.20)
- [ ] Stage 7: Documentation, commit and handoff (`docs/changelog.md`, `docs/problems.md`, `docs/plan.md`, activeContext, `git commit`, `sh_chain.py done`)

## Verification steps
1. `tools/sh_chain.py` starts and freezes cleanly.
2. New tests fail against current board (red run) and pass after implementation.
3. Firmware builds clean with `pio run` and respects OTA flash margin (< 1 306 624 B).
4. `tools/sh_process_check.py` reports 0 errors.
5. Flash via OTA (`tools/sh_ota_upload.py`) succeeds without touching LittleFS or gas data.
6. Board system tests pass against `192.168.1.20`.
