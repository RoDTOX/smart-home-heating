# Progress

Rewritten 01.11.2026 (process review QA-25). The plan and the status of every migration step are in
[`docs/plan.md`](../../docs/plan.md); the owner's board tests in [`docs/acceptance.md`](../../docs/acceptance.md),
problems in [`docs/problems.md`](../../docs/problems.md).

## On the board today

The lean firmware flashed on 30.10.2026 (it reports `8065534`, the old hard-coded build identity): TPI control,
weather-shifted pre-heat, sensor offsets, level schedule, one page, REST API, OTA, Telnet console, NTP and
Open-Meteo, passive BLE scan. It predates baseline v2 and has known safety defects (`docs/plan.md`, "Current state").
No board test result counts as evidence yet (process review QA-1, QA-26).

## Done

- Lean phases 1–3 and a first control step (29.10.2026, [`docs/changelog.md`](../../docs/changelog.md)).
- Requirements baseline v2 (31.10.2026), architecture (approved 01.11.2026), process review (approved 01.11.2026).

## Pending

- Apply the process review rows, then migration steps 1–14 one at a time (`docs/plan.md`).
