# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (ASPICE MAN.3). Status is tracked there.

## Current Phase: Step 10 Part 2 (`ControlTask` AR-21) & Step 7 Review

- **Active State**: Step 7 and Step 10 Part 2 built, flashed, and verified on board with 100% test pass rate. Owner hands-on test pending.
- **Goal of Step 10 Part 2**: FreeRTOS `ControlTask` on Core 1 (priority 3, period 1000 ms, 4 KB stack) decoupled from `loop()` (priority 1), 90s TWDT registration (NFR-1.5), timing metrics measurement (`longestTickDelayMs`, `longestLoopPassMs`, FR-23), stack headroom monitoring (NFR-1.5, NFR-2.2), and preemption under HTTP load (NFR-1.8).

## Stage Tracker (Execution via `/sh-change`)
- [x] Step 7 Implementation & SWE.5-TC43 tests (100% PASS on board, commit `211a7ee`)
- [x] Step 10 Stage 1: Requirements & Architecture alignment (AR-21, FR-23, NFR-1.5, NFR-1.8)
- [x] Step 10 Stage 2: Tests First (`tools/sh_tests/t_control_task.py`, SWE.5-TC44 suite, Red Run on board, `tools/sh_chain.py freeze`)
- [x] Step 10 Stage 3: Implementation (`include/App.h`, `src/App.cpp`, `ThermalZoneManager`, `WebApi`, `ConsoleCommands`, `main.cpp`)
- [x] Step 10 Stage 4: Local verification (`pio run`, `python tools/sh_process_check.py` 0 errors)
- [x] Step 10 Stage 5: Flash OTA & Board tests (`SWE.5-TC44.1..4` 100% PASS, `sh_process_check.py board` PASS)
- [x] Step 10 Stage 6: Documentation, plan status, changelog, and commit (commit `431a64f`)
