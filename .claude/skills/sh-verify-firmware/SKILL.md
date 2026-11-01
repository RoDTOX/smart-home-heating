---
name: sh-verify-firmware
description: Run the AGENTS.md "verification before flashing" pipeline for this firmware - build, process check, safety review of the diff, optional OTA flash, system test and live config check on the board. Use when the user says verify, check before flash, flash and test, or before any commit that touches src/, include/ or platformio.ini. Args - "no-board" skips flash and board test; "flash" flashes without asking again.
---

# sh-verify-firmware

Gate between "code changed" and "running on the boiler". Stop at the first failing step and report;
do not fix findings on your own - report them and let the user decide.

## 1. Build

```
pio run
```

Must finish without errors. Report flash usage from the output; firmware must be < 1.25 MB (OTA).

## 2. Process check (no board)

```
python tools/sh_process_check.py
```

Traceability, requirement rules (`docs/process.md` §2) and AGENTS.md rules (safety defaults,
changelog/activeContext, commit format, debug leftovers, secrets, libraries, size). Any ERROR stops the
pipeline. List WARN lines in the report.

## 3. Safety review of the diff

Launch the `sh-safety-reviewer` agent on the branch diff (against `origin/main`, plus uncommitted changes).
Any **BLOCKER** stops the pipeline - never flash with a blocker open.
Skip this step only if the diff touches nothing in `src/`, `include/` or `platformio.ini`.

## 4. Flash (OTA) - needs user confirmation

Skip if the user passed `no-board`. First record what the board runs now:

```
python tools/sh_process_check.py board
```

Unless the user passed `flash`, ask first:
"Flash `.pio/build/esp32dev/firmware.bin` to 192.168.1.20?"

```
python tools/sh_ota_upload.py
```

Run it in the background and wait for the board to come back (`GET http://192.168.1.20/api/status`)
before step 5. If the board does not return within ~90 s, stop and report - do not retry the flash.

## 5. System test on the board

Skip if `no-board`.

```
python tools/sh_test_system.py
```

Writes `docs/system_test_report.md`. Report PASS/FAIL count and every failing test id with its requirement.
A failing test is a finding about the code or the requirement - never edit the test to make it pass.

## 6. Board config check

Skip if `no-board`.

```
python tools/sh_process_check.py board
```

The board keeps settings in NVS, so a correct build can still run unsafe values. Any ERROR (live value
outside the safety limits) is a finding for the user - do not change the value through the API yourself.
Report the WARN if the board's commit does not match HEAD.

## Report

One short block, in Romanian for the user:

- build: OK / FAIL (flash size)
- process check: errors / warnings
- safety review: blockers / risks (one line each, `file:line`)
- flash: done / skipped
- system test: X/Y PASS, failing ids
- board config: errors (key = value vs limit), board commit vs HEAD

End with the one decision the user has to make (e.g. fix blocker, flash, commit). Do not commit or push.
