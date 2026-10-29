# Active Context

## Current focus
Lean refactor on branch `claude/vibrant-ride-95fick`. See [`docs/plan.md`](../../docs/plan.md) for phases.

## Recent changes
- Phase 1 done: removed sales/SaaS/fleet docs, Home Assistant integration, nginx/portal tooling, unused web pages.
  `AGENTS.md` and `docs/plan.md` rewritten short. Firmware untouched.

## Next steps
- Phase 2: inventory firmware features/endpoints (`src/main.cpp`, `ThermalZoneManager`), drop dead ones,
  rename "bio-schedule/anchors" to plain "schedule", split `main.cpp`. Verify with `pio run` + `tools/test_system.py`.
