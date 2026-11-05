# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 11 Part A (G28 Telemetry Rings to LittleFS)**: Complete, flashed and verified on board. `_ring24h` (2.88 KB) and `_ring5d` (2.4 KB) removed from RAM; history is streamed and persisted directly via LittleFS random-access (`TELEMETRY_24H_POINTS` fixed layout). Live RAM footprint reduced to a single 20 B cache point. Internal free heap increased to >101 KB (NFR-2.2 satisfied). SWE.5-TC37.1..TC37.2 100% PASS.
- **Web UI & Telemetry Polishing**: Built, tested, and flashed to board. Added Admin password verification modal (`POST /api/auth/verify`) requiring credentials to open Admin; moved `⚙️ Admin` / `✕ Ieși Admin` button up to the header; removed the top orange `#devBanner` (now a subtle footer tag); implemented offline interval detection ($T_{\text{OFF}} \rightarrow T_{\text{ON}}$) via RTC Slow Memory and LittleFS `/last_alive.bin` to display exact downtime on timeline; replaced solid orange event boxes with continuous semi-transparent amber boxes; condensed hover tooltip to single-line format; fixed TPI duty formatting (21% / 13 min/h); added Basic Auth candidates and console fallback to credentials update; guarded all `.toFixed` calls with `typeof === "number"`.
- **Step 10 Part 2 (`ControlTask` AR-21)**: Built, flashed, and verified on board with SWE.5-TC44.1..TC44.4 100% PASS. Dedicated FreeRTOS `ControlTask` on Core 1 (priority 3, period 1000 ms, 4 KB stack) registered with 90s TWDT (NFR-1.5). Decoupled `flushPendingNvs()` in `loop()` (R-A8). Preemption verified under HTTP load (tick delay <= 90 ms vs 500 ms bound). Owner accepted 05.11.2026.
- **Step 7 (ZoneController & FaultInjector)**: Built, flashed, and verified on board with SWE.5-TC43.1..TC43.4 100% PASS. Decoupled L4 `ZoneController`, outdoor bands (AR-12), FR-51 fallback duty upon sensor timeout (fixes PB-23, PB-61), and RAM-based `FaultInjector` (NFR-5.5, fixes PB-95). Owner accepted 05.11.2026.
- **Step 5 (BoilerLink)**: Accepted by owner on board (05.11.2026, commit `f28b8fc`, row added to `docs/acceptance.md`). SWE.5-TC42.1..TC42.4 100% PASS, DTC-207 verified.
- **Step 9 (Commands + auth)**: Accepted by owner ("ALL OK", `docs/acceptance.md` citing `ee7c762`).
- **Accepted Steps (1–10)**: All validated on board. LittleFS assets, adaptive window detection (TC26 7/7 PASS), 24h telemetry chart with dual Y-axis, Honeywell multi-moment schedule, gas meter calibration (`2265.304 m³`).
- **Health & Resources**: Free heap ~99 KB, internal free heap ~101.7 KB (≥100 KB target NFR-2.2 PASS), flash margin ~31 KB, control stack free >1.3 KB (2.1 KB typical).
- **Tooling & Skills Consolidation**:
  - Unified 6 `/sh-*` skills (`sh-change`, `sh-grill`, `sh-debug`, `sh-review`, `sh-verify`, `sh-coach`).
  - Zero orphan files invariant enforced in `AGENTS.md` §3 (no `.scratch/`, all work products strictly in `docs/`).
  - Single Source of Truth bridged to Claude Code via `CLAUDE.md`.
  - Git hook `.githooks/commit-msg` active.

## Next
1. **Owner hands-on test of Step 10 & Step 7**: verify `ControlTask` timing metrics and `/api/diag/inject` on live board.
2. **Step 11 completion**: ring move to LittleFS and `StatusModel` static buffer (PB-115).
3. **Step 12 (`Maintenance`)**: OTA quiet gate, rollback, full self-test, valve exercise.
