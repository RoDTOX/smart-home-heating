# Problems

Problems seen in operation or found by the checks (cold room, boiler starting at the wrong time, unsafe value
on the board). Append-only; process in [`process.md`](process.md) §8, checked by `tools/sh_process_check.py`.

- **ID:** `PB-n`, next free number, never reused.
- **Board commit:** what the board reported (`sh_process_check.py board`) when the problem was seen.
- **Safety:** `yes` if it touches the AGENTS.md safety list (interlock, overrun, short-cycling, frost,
  sensor timeout / safe state). An open safety problem blocks a release.
- **Status:** `open` → `analysing` → `fixed` (Fix = commit) → `closed` (fix confirmed on the board),
  or `rejected` (not a problem, say why in Cause).
- The fixing commit cites the ID in its message, e.g. `fix(control): keep valve open during overrun (PB-3)`.

| ID | Date | Symptom | Board commit | Safety | Cause | Fix | Status |
|---|---|---|---|---|---|---|---|
