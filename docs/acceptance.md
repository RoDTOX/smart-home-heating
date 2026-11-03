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
| 02.11.2026 | a2a6968 | 1.11.02.09 | FR-69.1, FR-69.2, FR-69.3, FR-73, FR-74, FR-11.3, FR-11.5, FR-11.6 | Owner tested adding and deleting zones and the sensor search in the engineering view on the board; answered OK (relayed by the main chat). The page fixes after their first test (refresh after delete, list kept after Stop, MAC list) were part of this build | OK | |
| 03.11.2026 | 3945362 | 1.11.02.15 | FR-65.1, FR-65.3, FR-65.4 | Owner tested export and import of the day, night and away setpoints (the zone target is no longer in the file); answered OK (chat) | OK | PB-62 |
| 03.11.2026 | c5eb53d | 1.11.03.29 | FR-40 | Owner answered OK (chat, relayed by the main chat) to the window baseline of 5 min (PB-72); hands-on detail not recorded | OK | PB-72 |
| 03.11.2026 | 2b44ef9 | 1.11.03.35 | FR-35.1 | Owner tested the gas meter on the board (chat, relayed by the main chat): the reading log with his readings 2265.304 (15:39) and 2265.996 (21:59), the measured flow 0.177 m3/h taken from the two readings and the history of the board, the gas card (today 2.41 m3, this month 2.41 m3, no flow or power to type); answered OK. Only the gas meter is accepted | OK | PB-80, PB-87, PB-98, PB-99 |
| 03.11.2026 | c5eb53d | 1.11.03.29 | FR-40 | Correction of the row above of this day for the window baseline: it was written from a one-word answer; the main chat says the owner has not tested the open window yet. Not accepted; PB-72 is `fixed` again, not `closed`. The test of the open window and of `OutputBank` (31b8496) are still to be given | pending | PB-72 |
| 03.11.2026 | 649eb31 | 1.11.03.31 | FR-40, FR-8.4, FR-8.1 | Owner tested the open window on the board (chat, relayed by the main chat): the window baseline of 5 min (PB-72) and the stuck `WINDOW` state with the `Închide pauza` button (PB-97), on 1.11.03.31 and later builds; answered OK. Not part of this: `OutputBank` (31b8496), still not accepted | OK | PB-72, PB-97 |
