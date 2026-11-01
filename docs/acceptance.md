# Acceptance

The owner's hands-on tests on the board. Append-only; process in [`process.md`](process.md) §5,
checked by `tools/sh_process_check.py release`.

- **Board commit:** exactly what `python tools/sh_process_check.py board` reports. A `-dirty` build can be
  tested, but only a clean commit can be released.
- **Checked:** what you looked at, in your words (e.g. "Z2 heated 19 -> 21 C overnight, boiler 4 starts").
- **Result:** `OK` or `NOK`. A `NOK` row names the problem(s) opened in [`problems.md`](problems.md).

| Date | Board commit | Version | Checked | Result | Problems |
|---|---|---|---|---|---|
