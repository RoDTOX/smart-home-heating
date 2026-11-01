---
name: sh-verify-firmware
description: Run the AGENTS.md "verification before flashing" pipeline for this firmware - build, process check, test review, safety review of the diff, OTA flash, system test and live config check on the board. Use when the user says verify, check before flash, flash and test, or before any commit that touches src/, include/ or platformio.ini. Args - "no-board" skips flash and board test (default - flash after steps 1-4 pass, owner decision 01.11.2026).
---

# sh-verify-firmware

Gate between "code changed" and "running on the boiler" (`docs/process.md` §5, AGENTS.md §4).
Stop at the first failing step and report; do not fix findings on your own - report them and let the user decide.
A finding that is not fixed at once gets a `PB-n` row (see "Problems" below).

## 1. Build

```
pio run
```

Must finish without errors. Report flash usage from the output; firmware below the NFR-2.8 limit (OTA).

## 2. Process check (no board)

```
python tools/sh_process_check.py
```

Traceability, requirement rules (`docs/process.md` §2), architecture rules R-A1 ... R-A7 and AGENTS.md rules
(safety ranges, changelog/activeContext, commit format, debug leftovers, secrets, libraries, size).
Any ERROR stops the pipeline. `KNOWN (PB-n)` lines are errors already tracked by an open problem - they do not
stop it; list them by `PB-n`. List WARN lines in the report.

## 3. Test review (changed tests)

For every test in `tools/sh_test_system.py` changed on the branch (`git diff origin/main -- tools/sh_test_system.py`),
apply `docs/process.md` §3.2:

- the docstring cites a leaf ID (`FR-10.2`), not a parent;
- it asserts the value or observable result written in the requirement text or §6 - compare with the
  requirement, never with the code;
- `trace` shows no hollow-test WARN for it.

A test that fails this review stops the pipeline. Never edit a test to make it pass.

## 4. Safety review of the diff

Launch the `sh-safety-reviewer` agent on the branch diff (against `origin/main`, plus uncommitted changes).
Any **BLOCKER** stops the pipeline - never flash with a blocker open.
Skip this step only if the diff touches nothing in `src/`, `include/` or `platformio.ini`.

## 5. Flash (OTA)

Skip if the user passed `no-board`. Owner decision of 01.11.2026: the firmware session may flash the board
itself once steps 1-4 pass. **Only one firmware session flashes at a time** - if another session is working on
the board, stop and report.

First record what the board runs now:

```
python tools/sh_process_check.py board
```

Note the commit the build carries (`[sh_git_version] commit ...` in the build output). A `-dirty` build is fine
for a hands-on test, but its results are **not a result of record** (`docs/process.md` §6): say so in the report
and do not use them for a commit or an acceptance row.

```
python tools/sh_ota_upload.py
```

Run it in the background and wait for the board to come back (`GET http://192.168.1.20/api/status`)
before step 6. If the board does not return within ~90 s, stop and report - do not retry the flash.

## 6. System test on the board

Skip if `no-board`.

```
python tools/sh_test_system.py
```

Writes `docs/system_test_report.md`. Report PASS/FAIL count and every failing test id with its requirement.
A failing test is a finding about the code or the requirement - never edit the test to make it pass.

## 7. Board config check

Skip if `no-board`.

```
python tools/sh_process_check.py board
```

The board keeps settings in NVS, so a correct build can still run unsafe values. Any ERROR (live value
outside the safety limits) is a finding for the user - do not change the value through the API yourself.
Report the WARN if the board's commit does not match HEAD or is `-dirty`.

## Problems

Every finding of steps 2-7 that is not fixed in this session (a checker ERROR, a BLOCKER or RISK, a failing test,
an unsafe board value) gets a row in `docs/problems.md` (`docs/process.md` §8): next free `PB-n`, date, symptom,
requirement IDs, the commit the board reports, Safety yes/no, Missed by (the test that should have caught it),
Checker (the ERROR text in backticks, without line numbers, when a checker line shows it), status `open`.
A finding that already has an open `PB-n` gets no new row; name the existing one.

## Report

One short block, in Romanian for the user:

- build: OK / FAIL (flash size), build commit (`-dirty` = not a result of record)
- process check: errors / known (PB-n) / warnings
- test review: changed tests OK / findings
- safety review: blockers / risks (one line each, `file:line`)
- flash: done / skipped
- system test: X/Y PASS, failing ids
- board config: errors (key = value vs limit), board commit vs HEAD
- problems: new `PB-n` rows opened

End with the one decision the user has to make (e.g. fix blocker, accept on the board, commit). Do not commit or push.
