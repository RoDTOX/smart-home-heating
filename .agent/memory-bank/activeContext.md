# Active Context

## Current focus
Lean refactor on branch `claude/vibrant-ride-95fick`. See [`docs/plan.md`](../../docs/plan.md) for phases.

## Recent changes
- Phase 1 done: removed sales/SaaS/fleet docs, Home Assistant integration, nginx/portal tooling, unused web pages.
  `AGENTS.md` and `docs/plan.md` rewritten short. Firmware untouched.

- Phase 2 in progress: Service Mode, PWA, web console, PubSubClient, DevicePassport removed (CI build green).
  Build check runs on GitHub Actions (container network blocks PlatformIO registry).

## Next steps
- Phase 2 remaining: simplify schedule (drop presets/anchors), split `main.cpp`, plain naming. Verify with `pio run` + `tools/test_system.py`.
