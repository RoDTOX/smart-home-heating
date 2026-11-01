# Acceptance

The owner's hands-on tests on the board. Append-only; process in [`process.md`](process.md) §5,
checked by `tools/sh_process_check.py release`.

- **Board commit:** exactly what `python tools/sh_process_check.py board` reports. A `-dirty` build can be
  tested, but only a clean commit can be released.
- **Requirements:** the requirement IDs this test covered, cited in full (e.g. `FR-2, FR-3, FR-16.1`).
  The `OK` rows of the released build together must cover every requirement ID of the AGENTS.md §3 safety
  list; `release` errors on a missing one.
- **Checked:** what you looked at, in your words (e.g. "Z2 heated 19 -> 21 C overnight, boiler 4 starts").
- **Result:** `OK` or `NOK`. A `NOK` row names the problem(s) opened in [`problems.md`](problems.md).

| Date | Board commit | Version | Requirements | Checked | Result | Problems |
|---|---|---|---|---|---|---|
