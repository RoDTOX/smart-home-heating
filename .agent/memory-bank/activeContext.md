# Active Context

## Current focus
Lean refactor on branch `claude/vibrant-ride-95fick`. See [`docs/plan.md`](../../docs/plan.md) for phases.

## Recent changes
- Phases 1–4 (first step) done on the branch: cleanup, firmware lean, level schedule, new UI, TPI control.
- CI (GitHub Actions) compiles every push; container network blocks the PlatformIO registry.
- `main.cpp` split into `WebApi.cpp` / `ConsoleCommands.cpp`; learned TPI duty persisted in NVS.
- 30.10.2026: flashed on the board (OTA), `sh_test_system.py` 78/78 PASS, 48/48 requirements covered.

- `tools/sh_process_check.py` checks traceability (req ↔ test ↔ code) and AGENTS.md process rules; in CI.
  Coverage counts exact leaf IDs only (30/88); safety check includes the `/api/config/system` clamps.
- Claude workflow: `sh-safety-reviewer` agent (`.claude/agents/`) and `sh-verify-firmware` skill (`.claude/skills/`).
- 31.10.2026: all project tools renamed `sh_*` (agents/skills `sh-*`), rule in AGENTS.md §3. Not yet in `docs/requirements_and_features.md` / `requirements_review.md` (old paths).
- 31.10.2026: `docs/process.md` defines the process (requirement rules, review exit criteria, decisions, commit/push gate). Checker gained `reqs` and `board` modes. Board today: zone 2/3 window threshold 0.3 C (FR-40: 0.8-2.0). Build identity fixed: firmware reports its real commit (`sh_git_version.py`), not yet flashed (board still reports 8065534).
- 31.10.2026: chat zones (AGENTS.md §5), `docs/problems.md` + `docs/acceptance.md` (empty), `sh_process_check.py release` gate before a PR to main. Next: owner logs hands-on tests and problems there.
- 01.11.2026: owner continues in the main chat only; AGENTS.md §5 says a single active chat owns every zone.
- [requirements chat, 01.11.2026] Requirements review approved (`docs/requirements_review.md`); `docs/requirements_and_features.md` rewritten as English baseline v2 with `sh_*` tool paths, FR-41..45 defined, sensor-loss fallback decided (learned duty, 20 % if nothing learned), phase 4 included, fallback per feature (NFR-5).
- [architecture chat, 01.11.2026] `docs/architecture.md` (SWE.2) approved by the owner: 23 modules, `SafetySupervisor` sole writer of outputs, validity + fallback per input, `ParamStore`, events in LittleFS, fault injection, 14 migration steps. AGENTS.md §5 has the *architecture* zone.

- [/simplify chat, 01.11.2026 02:10] Requirement → module trace kept only in `architecture.md` §3.2 (§15 matrix gone),
  checked by `sh_process_check.py trace` (103/103 allocated); requirement headings carry no ID lists. Committed 4daec12.
  Owner chose (quiz, 01.11.2026) the requirements §6 ranges over AGENTS.md: interlock 120–600 s, frost 8–16 °C,
  burner run / pause 120–600 s, sensor timeout 1–60 min. Done, uncommitted: `sh_process_check.py` reads safety
  ranges from §6 (no copies). Open: AGENTS.md §3 safety list and `.claude/agents/sh-safety-reviewer.md` still say
  120–180 s / < 12 °C; changelog entry for this step.
- [main chat, 01.11.2026] Process review `docs/process_review.md` (QA-1..QA-28), awaiting owner approval row by
  row; QA-1..QA-6 block code work. Nothing changed in the reviewed files.

## Next steps
- Safety defaults in `Config.h` set to production (120/180/180 s) - not yet flashed / tested on the board.
- Open: FR-41..45 into the requirements doc; API clamp lets interlock/overrun go to 1 s.
- Watch TPI logs / room temps for a few days on the board.
- Decide sensor-loss fallback duty (frost risk vs. runaway); UI on LittleFS with version check.
- [requirements chat, 01.11.2026] Align `tools/sh_test_system.py` with baseline v2: retired FR-13, FR-24..27, FR-33, FR-38 still cited; new requirements FR-47..72 / NFR-5.x have no tests yet.
- [architecture chat, 01.11.2026 01:45] `docs/architecture.md` follows the requirements of 925ea91 (O-1..O-11 applied, `faultFrostOutdoor`, DTC-208, §6 names, FR-58 AUTH list). Process chat: `process.md` §1 SWE.2 row and §7 gap → `docs/architecture.md`; grep check for R-A1..R-A7; §3 last line ("tests changed first, then code") no longer matches the owner's order. Firmware chat: start at migration step 1 (`ParamStore`); each step changes code and its tests together, full `sh_test_system.py` before flashing; G2 is fixed in steps 4 and 9, no separate hotfix.
- [requirements chat, 01.11.2026 01:29] Architecture observations O-1..O-11 closed in the requirements (decision log: `docs/requirements_review.md` §6); O-2 limit `faultFrostOutdoor` = 5 °C. Owner order (01.11.2026): requirements and architecture aligned and covering each other first, then code; tests follow the code (not TDD). Committed as 925ea91.
