---
name: sh-change
description: The owner's chain - use whenever the owner tells what they see on the heating (a cold room, the boiler starting too often, something odd on the page, "nu merge") or asks for a change in behaviour, and when they say to go on with the next migration step. Always from the requirement down - log, requirement, micro-plan approval, tests first, code, gates, commit, board, owner test. The owner describes, approves the micro-plan, decides requirement changes and tests on the board.
---

# sh-change

The owner has four moments:

1. **Tell** - what they see or want (this skill starts here).
2. **Approve micro-plan** - before any code or tests are written, a 3-5 bullet micro-plan with estimated impact is presented to the owner for approval.
3. **Decide** - only when a requirement has to change: one question, plain Romanian, options.
4. **Test** - on the board, OK / NOK.

Always top-down, in this order. Never start at the code, never skip a level, never change a test to make it pass.

## Modes (owner, 02.11.2026: tokens)

- **Light - the default.** The chat does every step itself, no review agents. The only agent: a change that touches
  the AGENTS.md §3 safety list (valves, boiler, burner guard, frost, sensor loss, boot) gets one `sh-safety-reviewer`
  run. Step 3: the chat writes the tests itself, before the code. Step 5: `sh-verify-firmware no-board light`. No
  `sh-assessor`, no `sh-compliance-reviewer`.
- **Full.** Only when the owner asks ("serios", "full", "complet") and before a release (`docs/process.md` §5):
  everything below as written, with all agents.

When unsure, light. Say the mode in the first line of the report.

## The guard (both modes, no tokens)

A hook (`.claude/hooks/sh_chain_guard.py`, state `tools/sh_chain.py`) keeps the order: `start "<label>"` before the
tests - the code (`src/`, `include/`, `web/`) is closed; `freeze` after the red run - the test file is closed and
its hash recorded (`sh_process_check.py process` errors when it differs); `done` after the commit. A test or a
requirement that looks wrong or unclear is never adjusted by the chat: stop and ask the owner (moment 2, options in
the house); `unfreeze --because <PB-n|D-n|C-n>` only after their decision.

## 1. Understand and log

- Restate the observation in one sentence about the house (room, valve, boiler, time of day). If it can be read
  two ways, ask with options (part of moment 1).
- `python tools/sh_process_check.py board` - the commit the board runs.
- Something that does not work: a `PB-n` row in `docs/problems.md` (owner's words, board commit, Safety), or the
  open `PB-n` that already covers it. A wish for new behaviour gets no `PB-n`.
- "Next migration step" (`docs/plan.md`): its requirements are already decided - go to step 3 with the step's IDs.

## 2. Requirement

Find the requirement that covers it (`docs/requirements_and_features.md`, values in §6). Exactly one of:

- **Requirement right, firmware wrong** - go on, no question.
- **Requirement missing, wrong or unclear** - moment 2: one question, 2-3 options with what each means in the
  house, recommended first. Then rewrite the requirement (rules `docs/process.md` §2); in full mode run `sh-req-reviewer`
  on the changed IDs only; add a row to
  `docs/requirements_review.md` and an entry to `.agent/memory-bank/decisions.md` (§4). If the change moves work
  between modules, update `docs/architecture.md` §3.2; `trace` shows no ERROR.
- **Works as specified, and the owner agrees** - `PB-n` `rejected` with the reason; stop.

A requirement about access, credentials, updates, user data or the radio is checked against `docs/compliance.md`
before it is written (full mode: `sh-compliance-reviewer` on those IDs; light: the chat reads the matching `CMP-n` rows): the device is to be sold in the EU (AGENTS.md §2).

## 3. Tests first

Start the guard: `python tools/sh_chain.py start "<label>"`; after the red run `python tools/sh_chain.py freeze`.
**Light:** the chat writes the tests itself, before the code, from the requirement text and §6 (leaf ID in the
docstring, the written value asserted) and runs them on the board: the new ones fail. **Full:** skill
`sh-write-tests` with the requirement IDs: clean-room writer, reviewer, red run (new tests fail on the
board's current firmware). An existing test is changed only there, by `sh-test-writer`, citing the `PB-n` or the
requirement change.

## 4. Code

Change `src/`, `include/`, `web/` until the new tests pass. Smallest change (AGENTS.md §3), only this change; code
tags cite the leaf ID. A test that cannot pass sends you back to step 2 (the requirement may be wrong), never to
the test: stop and ask the owner what the requirement should say; the guard blocks edits of the frozen test file.

## 5. Gates, commit, board

1. `sh-verify-firmware no-board` (light: `no-board light`) - build, checker, and in full mode the test, code and
   safety reviews. Fix code findings; what
   is not fixed becomes a `PB-n`. A BLOCKER stops the chain and goes to the owner.
2. Update `docs/changelog.md`, `.agent/memory-bank/activeContext.md`, and `docs/plan.md` when a step changes
   status.
3. Commit on the work branch (owner, 02.11.2026): `type(scope): subject (PB-n)`; the `PB-n` row gets the commit and
   status `fixed`. Never on `main`, never push. Then `python tools/sh_chain.py done`.
4. `sh-verify-firmware` steps 5-7: flash, system test, board config. A failing test sends you back to step 4.
5. Full mode only - end of a migration step: `sh-assessor` (the step's evidence) and `sh-compliance-reviewer` (EU rules) in
   parallel; their results go into `docs/review_step_<n>.md`. Evidence that can still be produced (a review, a
   record) is produced now; the rest becomes a `PB-n`. A missing requirement from the compliance review is
   moment 2. A fix outside a step runs only `sh-compliance-reviewer`, and only when it touches a `CMP-n` row.

## 6. Owner test (moment 3)

Tell the owner in two or three plain sentences what changed in the house and what to watch (which room, when,
what should happen), with OK / NOK buttons.

- **OK** - a row in `docs/acceptance.md` (board commit, what was checked, `OK`); the `PB-n` becomes `closed`.
- **NOK** - the owner's words go into the `PB-n` (or a new one); back to step 2.

Push, PR and `main` only when the owner asks (`docs/process.md` §5 release gate).

## While working

Short plain Romanian, the house first, at most one ID per point. Before non-trivial execution, present the micro-plan
and request approval. Autonomous loops are capped at 10 consecutive tool calls without a checkpoint (enforced by
`tools/sh_cost_guard.py`). An owner decision the chain cannot make (a BLOCKER, a safety value) is moment 3.
