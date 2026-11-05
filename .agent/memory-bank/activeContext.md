# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 7 (ZoneController & FaultInjector)**: Built, flashed, and verified on board with SWE.5-TC43.1..TC43.4 100% PASS (commit `211a7ee`). Decoupled L4 `ZoneController`, outdoor bands (AR-12), FR-51 fallback duty upon sensor timeout (fixes PB-23, PB-61), and RAM-based `FaultInjector` (NFR-5.5, fixes PB-95). Owner hands-on test pending.
- **Step 5 (BoilerLink)**: Accepted by owner on board (05.11.2026, commit `f28b8fc`, row added to `docs/acceptance.md`). SWE.5-TC42.1..TC42.4 100% PASS, DTC-207 verified.
- **Step 9 (Commands + auth)**: Accepted by owner ("ALL OK", `docs/acceptance.md` citing `ee7c762`).
- **Accepted Steps (1–6, 8, 9, partial 10 & 11)**: All validated on board. LittleFS assets, adaptive window detection (TC26 7/7 PASS), 24h telemetry chart with dual Y-axis, Honeywell multi-moment schedule, gas meter calibration (`2265.304 m³`).
- **Health & Resources**: Free heap steady >101 KB, flash margin ~37.2 KB (firmware binary 1,269,377 B ≤ 1 306 624 B limit, NFR-2.8).
- **Tooling & Skills Consolidation**:
  - Unified 6 `/sh-*` skills (`sh-change`, `sh-grill`, `sh-debug`, `sh-review`, `sh-verify`, `sh-coach`).
  - Zero orphan files invariant enforced in `AGENTS.md` §3 (no `.scratch/`, all work products strictly in `docs/`).
  - Single Source of Truth bridged to Claude Code via `CLAUDE.md`.
  - Git hook `.githooks/commit-msg` active.

## Next
1. **Owner hands-on test of Step 7**: verify `/api/diag/inject` and fallback duty on live board.
2. **Step 10 / 11 memory completion**: AR-21 control task separation and full LittleFS ring buffer.
3. **Step 12 (`Maintenance`)**: OTA quiet gate, rollback, full self-test, valve exercise.
