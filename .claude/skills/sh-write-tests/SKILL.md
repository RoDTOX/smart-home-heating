---
name: sh-write-tests
description: Start a migration step test-first - the clean-room agent sh-test-writer writes the step's system tests from the requirements and the architecture (it cannot read the code), sh-test-reviewer reviews them, then a red run on the current firmware shows they fail before the code exists. Use at the start of every migration step, before any firmware change, or when the user says write the tests, tests first. Args - the step number, or a list of requirement IDs; "no-board" skips the red run.
---

# sh-write-tests

Order of a step (`docs/process.md` §3.1): **tests first, from the documents; then code until they pass.**
A test written before the code cannot copy the code. A test that fails on the old firmware proves it checks
something.

## 1. Requirements of the step

From `docs/plan.md` (step in progress) and `docs/architecture.md` §18 (its row): the requirement IDs the step
builds. With a requirement list as argument, use it. Show the list to the owner in one line before going on.

If a requirement of the list is open in `docs/requirements_review.md` or has a `D-n` question, stop: tests are not
written against an undecided requirement.

## 2. Write (clean room)

Launch `sh-test-writer` with the ID list and the area module of `tools/sh_tests/` to read (find it with `grep -rn "TC" tools/sh_tests` on
the nearest existing tests of the same requirement group). It reads only the documents and edits only
`tools/sh_tests/` and the runner list in `tools/sh_test_system.py` (a hook enforces it). Do not pass it code or hints from the code. When the list holds a requirement of the
AGENTS.md §3 safety list, launch it and `sh-test-reviewer` with model `opus`, effort `high` (AGENTS.md §5, model per work);
otherwise they keep their own model.

What it reports as not writable (not observable through the API, value not specified, ambiguous) is a finding for
the requirements review (§3), not something to work around.

## 3. Review (clean room)

Launch `sh-test-reviewer` once on the changed tests. A FINDING goes back to `sh-test-writer` once with the finding
text; the fixed tests are checked by the owner's gate, not by a second review run; what remains goes into the
report. Write `python tools/sh_chain.py hash` into `docs/review_step_<n>.md` (`sh-verify-firmware` step 3
skips a second review of the same hash).

## 4. Red run

Skip if `no-board`. The board still runs the firmware **before** this step's code.

```
python tools/sh_process_check.py board
python tools/sh_test_system.py
```

Note the board commit. Expected: every new test **fails** (or is reported as not run because the endpoint does not
exist yet). A new test that passes on the old firmware is either behaviour that exists already - list it for the
owner - or a hollow test - back to step 3. Old tests keep their previous result.

## 5. Freeze

From here the step's tests are fixed. The code session makes them pass by changing `src/`, `include/` or `web/`
only. A test that cannot pass without a different value is a `PB-n` row or a requirements review row (§3.2), and
only `sh-test-writer` changes it, after the owner's decision.

Record in `docs/review_step_<n>.md`: the ID list, the tests written, the board commit of the red run and its
result (`docs/process.md` §9).

Inside `sh-change`, commit the frozen tests alone on the work branch, before any code:
`test(<scope>): step <n> tests, red on <board commit>` - the evidence `sh-assessor` checks that the tests came
first. Outside the chain, record `python tools/sh_chain.py hash` in the step record instead.

## Report

Short, plain Romanian for the owner: how many requirements, how many tests, which failed in the red run (expected)
and which passed (to explain), what could not be written and why. Then the one decision: start the code, or settle
the open requirement first. Inside `sh-change` the chain goes on to the code without asking. Outside it, do
not commit; never push.
