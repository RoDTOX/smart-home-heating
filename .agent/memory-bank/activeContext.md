# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Step 14 Completed (Phase 4 Advisors: FR-56, FR-57, FR-67, FR-70, FR-63, FR-71, FR-40.2)**:
  - Part A (`PlantMonitor` FR-56, FR-57): TC46 100% PASS on board.
  - Part B (`HeatUpModel` FR-48, FR-67): TC47 100% PASS on board.
  - Part C (`SolarThrottle` FR-70): TC48 100% PASS on board.
  - Part D (`CycleAligner` FR-63): TC49 100% PASS on board (commit `6097c99`).
  - Part E (`HumidityComp` FR-71): TC50 100% PASS on board (commit `e932f6a`).
  - Part F (`SmartWindow` FR-40.2): TC51 100% PASS on board. Multivariable window detection advisor skeleton with physical absolute humidity ($AH$) via Magnus formula, $\Delta T < 5.0\text{ }^\circ\text{C}$ suspension rule, baseline FR-40 fallback contract (Risk R-5), and status endpoint `/api/window/status`.
- **Accepted Steps (1–12)**: All validated on board. Step 13 (probes) deferred with owner confirmation.

## Next
1. **Migration Review & Final Release Gate**: Assess whole system status, verify process checks, and complete handoff documentation for Step 14.
