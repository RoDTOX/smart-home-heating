# Gas meter readings and the board's log, 03.11.2026 (record)

Written from the backup of the board's `calibLog` (8 entries) taken at 21:21 on 03.11.2026, before the ring of the
old firmware overwrites it. The owner's decision of 03.11.2026: a reading typed by the user is data, never a
calibration, never overwritten (requirement FR-35.1, change C-25, still to be written and shown to the owner).

State at the backup: `baseIndex` 2265.304 m3, `baseSec` 127312, `baseEpoch` 1793731273, `gasFlameRate` 0.60 m3/h,
`boilerSetTemp` 40, `boilerFlowObserved` 43, `boilerPowerKw` 24, `gasPrice` 3.30.

| Time (local) | Epoch | Index m3 | deltaM3 | burnSec | Origin |
|---|---|---|---|---|---|
| 03.11 15:39 | 1793713140 | 2265.304 | — | — | owner: the physical meter reading, 15:39 (the owner once said 15:30 as a correction, then confirmed 15:39 in chat 03.11.2026; the entry on the board is right); no log entry existed for this time before the log was started. First reading of the persistent log of FR-35.1 (C-25). |
| 03.11 16:25 | 1793715949 | 2265.304 | 0 | 88 | owner (first entry, as told by the owner); the reading itself was put into the code by AGY in 63accff at 15:53:03, so it was taken at or before 15:53 - exact time not recorded, to be asked |
| 03.11 16:26 | 1793715997 | 2265.304 | 0 | 0 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 16:28 | 1793716090 | 2265.304 | 0 | 0 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 19:56 | 1793728606 | 2265.304 | 0 | 9667 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 19:58 | 1793728739 | 2265.304 | 0 | 0 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 20:02 | 1793728969 | 2265.304 | 0 | 0 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 20:39 | 1793731196 | 2265.304 | 0 | 694 | unverified: written by a POST to `/api/boiler/gas-meter`; the time of 19:56 ... 20:39 coincides with full system test runs of the process session (TC21.3 posts index 2265.304); the owner says which are theirs |
| 03.11 20:41 | 1793731273 | 2265.304 | 0.0 | 0 | RESTORE by the process session at 20:41:13 after the flash of 1.11.03.31 (index shown as 0 because the old value was only a code default); not an owner reading |

## Why every save moves the base (answer to the question of the main chat)

`POST /api/boiler/gas-meter` with `index` sets three values together: base index = typed value, base runtime =
current boiler runtime, base epoch = now. So the estimate after a save is `typed value + gas burned since that moment`.
That is by design: the typed value replaces the estimate. What is not lost: `gasToday`, `gasYesterday`,
`gasThisMonth` come from the boiler runtime counters, not from the base. What is wrong:
1. Every POST appends a log entry, also one with no `index` (only a boiler temperature) - PB-99.
2. The entry stores `deltaM3` against the previous base; when the previous base was the code default or the same
   number typed again (tests), `deltaM3` is 0 and the 9667 s of burn at 19:56 stay unexplained: no measured rate can
   be derived - PB-99.
3. The full system test (TC21.3) types index 2265.304 on the live board at every run, which writes log entries and
   moves the base of the owner's data - PB-87, PB-99.

## Reading typed by the owner after the log started (1.11.03.32)

| Time (local) | Epoch | Index m3 | Origin |
|---|---|---|---|
| 03.11 21:59 | 1793735988 | 2265.996 | owner, typed by hand in the page; stored without a runtime (the runtime field came with C-26). In 6 h 20 min the index rose 0.692 m3 against 3.9 h of boiler ON time (history of the board): 0.18 m3 per ON hour |
