# Active Context

## Current focus
Lean refactor on branch `claude/vibrant-ride-95fick`. See [`docs/plan.md`](../../docs/plan.md) for phases.

## Recent changes
- Phases 1–4 (first step) done on the branch: cleanup, firmware lean, level schedule, new UI, TPI control.
- CI (GitHub Actions) compiles every push; container network blocks the PlatformIO registry.
- Not yet flashed or run against the board: the board is on the user's LAN, no Remote Control folder shared.

## Next steps
- User: `pio run` + `python tools/ota_upload.py` + `python tools/test_system.py` locally (needs `include/secrets.h`).
- Then: split `main.cpp`, persist learned TPI duty, decide sensor-loss fallback duty.
