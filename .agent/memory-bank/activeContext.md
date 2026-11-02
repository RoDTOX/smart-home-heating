# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- Migration of `docs/architecture.md` §18, status in [`docs/plan.md`](../../docs/plan.md). Step 1 (`ParamStore`)
  committed (413fa60, fix 7743c26) and flashed; the owner's page test fixes committed (43ffe48 PB-15, 2b7564f
  PB-17 ... PB-21). Not pushed.
- Zones erased on the board by the owner (01.11.2026): heating off until the owner commissions them from the page.
- Old NVS keys are kept for a rollback of step 1; remove them in a later step, after acceptance.
- Process (02.11.2026): what the owner tells or asks starts skill `sh-change` (requirement down, tests first in a clean
  room, commit on the work branch after the gates, push / `main` on request). The chain has not run yet. Old log
  entries are in `docs/archive/`; the checker's default run counts WARN per section. The process changes of
  01.11 - 02.11 are not committed.

- The device is prepared for sale in the EU under the owner's brand (02.11.2026): EU obligations of the software in
  `docs/compliance.md`. First full review 02.11.2026: 12 partial, 19 missing; PB-43 ... PB-54 (4 safety),
  `requirements_review.md` §10 with owner questions D40 ... D50 (open). `sh-assessor` has not run yet.

- Model per work (02.11.2026): AGENTS.md §5 table; agents carry `model` / `effort` in their frontmatter (Opus only
  for the safety reviewer and safety tests).

- Firmware session (02.11.2026): change C-9 (config export / import, FR-65.1 ... FR-65.8) in progress, then migration step 3
  (`OutputBank`, with zones added / deleted by the user C-10 and sensor search C-11), then step 2. Board settings saved before
  the first flash: `side-session-notes/.../board_settings_2b7564f_2026-11-02.json`.

- Tests (02.11.2026): the system tests are split by area: `tools/sh_tests/t_<area>.py` (one mixin class each) and
  `base.py`; `tools/sh_test_system.py` is the runner. Checker, guard, chain hash and clean-room hook cover the
  whole set. Largest module: `t_export_import.py` (1,700 lines), split further when it grows.

- Workflow modes (02.11.2026): light by default (the chat does everything; one safety review for the safety list), full
  (all agents) on the owner's request and before a release - AGENTS.md §5.

## Next
- Owner: hands-on acceptance of step 1 and the page (row in `docs/acceptance.md`), then the step review record and
  step 2 (`Diagnostics`).
- Test findings of the trial review, PB-25 ... PB-42 (9 safety): fix through `sh-change`, tests by `sh-test-writer`.
- `docs/architecture.md` §18 still says "no separate test step first" - align it with `docs/process.md` §3.1.
- PB-43 first (setup access point never closes, open to anyone in range): through `sh-change`, FR-50 needs a
  closing rule (owner decision).
- Owner decisions D40 ... D50 (EU rules), one at a time through the chain; most land in migration steps 9 and 12.
