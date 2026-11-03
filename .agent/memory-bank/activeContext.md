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

- Firmware session (03.11.2026): C-14 / PB-66 / PB-67 (thermal deficit 100% duty on error >= 0.4 C, weather compensation, UI 2-in-1 sensor toggle, dirty protection with undo, accurate status labels). C-15 / PB-68 resolved: Honeywell multi-moment schedule matrix (P1-P4 per day/group), 4 modes (5+2, constant, unified, 7day), configurable away setpoints, clean 24h UI, SWE.5-TC16.6 PASS. Fix d366a91 applied (mode & tab switching). UI cleanup f7ad961 applied. Fix & feat 04a2a84 applied: restored closing brace in syncSchedInput, implemented always-visible vertical setpoint stepper on all zone cards, widened MAX_T to 30 C. Feat 1f162c5 and fix f5ccb5d (PB-70) applied: aligned PARAM_LIMITS with requirements §6 (rfSafetyHeartbeatSec up to 900 s, overrun 180 s floor), updated FR-8.1 (baseline v2.11), and added SWE.5-TC29.38 and SWE.5-TC19.11 tests for pre-import diff preview and vertical card steppers. Feat 7eae5ee and fix e2536da applied: replaced raw 34-key import dump with clean status card, cleared obsolete report on re-import/export, and disambiguated schedule levels in diff table (Program Zi vs Mod Plecat). Schedule active moment highlighting made day-group aware (no false weekend highlight on weekdays, Constant 24/7 badge updated), FR-10.4 check conditions clarified, operational checks for boiler/relays added, and dedicated active DTC panel with cause explanations implemented (SWE.5-TC19.12 & TC19.13 PASS). Admin panel reorganized into 4 collapsible groups (Confort & Programare default open, Sănătate, Configurare, Debug), Clear DTC button with session history added, and polling throttled when debug is collapsed (SWE.5-TC19.14 PASS). Implemented zone heating runtime telemetry (runtimeTodaySec, runtimeYesterdaySec in /api/zones and UI zone card badges) and closed all 4 admin sections by default (SWE.5-TC19.15 PASS). Optimized room card subtitles (removed duplicate target, added 💧 X% RH and Boost manual), clarified TPI cycle formatting (Încălzire TPI · X min/h, Pauză ciclu menține din șapă), and implemented 24h LittleFS telemetry chart in dedicated collapsible section with dynamic zone names and interactive hover inspection (SWE.5-TC19.16 PASS). `OutputBank` (step 3) committed. Board settings saved before
  the first flash: `side-session-notes/.../board_settings_2b7564f_2026-11-02.json`.

- Tests (02.11.2026): the system tests are split by area: `tools/sh_tests/t_<area>.py` (one mixin class each) and
  `base.py`; `tools/sh_test_system.py` is the runner. Checker, guard, chain hash and clean-room hook cover the
  whole set. Largest module: `t_export_import.py` (1,700 lines), split further when it grows.

- Workflow modes (02.11.2026): light by default (the chat does everything; one safety review for the safety list), full
  (all agents) on the owner's request and before a release - AGENTS.md §5.

## Next
- Owner proposal (03.11.2026): Decouple web UI assets to LittleFS with dual versioning (target migration step 12).
- Owner: hands-on acceptance of step 1 and the page (row in `docs/acceptance.md`), then the step review record and
  step 2 (`Diagnostics`).
- Test findings of the trial review, PB-25 ... PB-42 (9 safety): fix through `sh-change`, tests by `sh-test-writer`.
- `docs/architecture.md` §18 still says "no separate test step first" - align it with `docs/process.md` §3.1.
- PB-43 first (setup access point never closes, open to anyone in range): through `sh-change`, FR-50 needs a
  closing rule (owner decision).
- Owner decisions D40 ... D50 (EU rules), one at a time through the chain; most land in migration steps 9 and 12.
