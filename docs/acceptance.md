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
| 02.11.2026 | ce8a58b | 1.11.02.03 | FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8 | Owner tested export and import in the engineering view; answered OK (chat). One open question: the zone target in the file (PB-62) | OK | PB-62 |
| 02.11.2026 | 2ca7459 | 1.11.02.10 | FR-11.3, FR-65.5, FR-65.6 | Owner tested one Save for all zones and the exchange of sensors between zones in the engineering view; answered OK (chat). Zone add / delete and sensor search (C-10, C-11) not yet accepted | OK | |
