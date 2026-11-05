# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 5 (BoilerLink)**: L2 I/O transport abstraction built with `Rf433Link` and `ContactLink` (FR-5.1, FR-17.1..7, FR-55, FR-57, FR-69; C-44, G16, G17). Configurable `boilerLinkType`, `hbInterval` (30–900 s), and fail-safe `DTC-207` raised when heartbeat is disabled and cleared when enabled. Group 42 tests (SWE.5-TC42.1..TC42.4) 100% PASS on board. Owner hands-on test pending.
- **Step 9 (Commands + auth)**: Accepted by owner ("ALL OK", `docs/acceptance.md` citing `ee7c762`). Custom credentials via `/api/config/credentials`, DTC-403 cleared, SWE.5-TC41.8 passes.
- **Accepted Steps (1–4, 6, 8, 9, partial 10 & 11)**: All validated on board. LittleFS assets, adaptive window detection (TC26 7/7 PASS), 24h telemetry chart with dual Y-axis, Honeywell multi-moment schedule, gas meter calibration (`2265.304 m³`).
- **Health & Resources**: Free heap steady >100 KB (101–102 KB), flash margin ~47 KB (firmware ≤ 1 306 624 B limit, NFR-2.8).
- **Tooling & Skills Consolidation**:
  - Unified 6 `/sh-*` skills (`sh-change`, `sh-grill`, `sh-debug`, `sh-review`, `sh-verify`, `sh-coach`).
  - Zero orphan files invariant enforced in `AGENTS.md` §3 (no `.scratch/`, all work products strictly in `docs/`).
  - Single Source of Truth bridged to Claude Code via `CLAUDE.md`.
  - Git hook `.githooks/commit-msg` active.

## Next
1. **Owner hands-on test of Step 5 (`BoilerLink`)**: Verify receiver behavior and DTC-207 on the live board (`docs/acceptance.md`).
2. **Step 7 (`ZoneController`)**: TPI algorithm, learned duty per outdoor band, FR-51 fallback duty upon sensor loss, window state integration, and `FaultInjector`.
3. **Open problems to resume at Step 7**: PB-120, PB-123.
4. **Step 10 / 11 memory completion**: AR-21 control task separation and full LittleFS ring buffer.
