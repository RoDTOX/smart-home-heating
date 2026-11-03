# Process review: 43 commits 31b8496 .. 6170f16 (03.11.2026)

Review of the work the owner did with another AI tool (Google AGY) outside the chain `sh-change`. Records and
reviews only: `src/`, `include/`, `web/` and the tests were not changed. The board runs a9c5175 (1.11.03.27).
Every finding not fixed is a `PB-n` row in [`problems.md`](problems.md) (PB-71 ... PB-94, status open).

## 1. What was missing, and what was written

| Gap | Result |
|---|---|
| `decisions.md` cited C-14, `requirements_review.md` §9 ended at C-13 | rows C-14 ... C-20 added (written after the fact, decided by the owner in chat 03.11.2026) |
| FR-35.1 had no module | allocated to `History` (module 23) in `architecture.md` §3.2; its parameters are §6 in `ParamStore` |
| FR-40 is a parent with its own `{SW; test}`; FR-40.2 covered only via the parent | **not changed**: moving the text to a child leaves TC26.1 ... TC26.4 citing the parent, so the child shows as untested; that is a test change (PB-42, PB-90) |
| Two plan files | `docs/plan.md` is canonical (AGENTS.md §2). `.agent/plan.md` was a pointer to it until AGY overwrote it; it is a pointer again and the AGY text is kept below it unchanged as history |
| Step 3 status | `docs/plan.md`: `OutputBank` built and on the board, owner hands-on OK not given; new section "Work outside the chain" |
| Process drift | PB-83 ... PB-85 |
| Owner acceptance | no row written: the owner has not tested 1.11.03.x |

Checker after the changes: `python tools/sh_process_check.py` 0 errors, warnings as before apart from the FR-35.1 line, which is gone.

## 2. Reviews run on the range

| Review | Mode | Result |
|---|---|---|
| `sh-safety-reviewer` on `src/`, `include/` | full | **no BLOCKER**; 11 FINDINGs (PB-71 ... PB-81), 6 NOTEs (PB-94) |
| `sh-code-reviewer` on `src/`, `include/`, `web/` | full | 12 FINDINGs (PB-72, PB-78, PB-80, PB-90 ... PB-94) |
| `sh-test-reviewer` on `t_control.py`, `t_ui_history.py`, `t_export_import.py` | full, clean room | 1 BLOCKER-class finding (PB-86), 7 more (PB-87 ... PB-89) |
| `sh-compliance-reviewer` on the console | targeted | CMP-5 partial, CMP-11 missing; one new PB (PB-82); five owner decisions (section 4) |

The only item the test reviewer ranked as BLOCKER is PB-86 (five new tests never run). The owner decides whether it
stops the release; the safety reviewer found no BLOCKER, so nothing here blocks reading the records.

### 2.1 Safety findings (PB-71 ... PB-81), in plain words

- PB-71 (safety): the 100 % rule at a deficit of 0.4 °C reopens a valve that closed less than 10 min ago; sensor noise
  makes the actuator switch often. The minimum OFF time of FR-47 is not respected.
- PB-72: window detection looks back up to 10 min, the text says 5 min; slow cooling can close a zone for 30 min.
- PB-73: pre-heat still works from the old slots, not from the four moments.
- PB-74 (safety): the stored schedule is read without range checks; a bad record can give a garbage target.
- PB-75: the update replaced the owner's day / night temperatures with factory moment values. **The owner should look at
  the schedule on the board.**
- PB-76, PB-77: the zone runtime file is written every 60 s without a change, and loaded before the file system exists
  (the counters are never restored).
- PB-78, PB-79, PB-80, PB-81: values out of range answered with 200, a TPI cycle restarting every second at 100 %, the
  owner's own meter reading as factory default, moment names that can break the JSON.
- Not on the safety list and unchanged: interlock (FR-2), overrun (FR-3), burner guard (FR-4), frost (FR-16.1), safe
  boot (NFR-1.4); frost still wins over the window pause.
- Size: the image is 1.22 MiB. NFR-2.8 says "< 1.25 MB": in MiB there is room, in 10^6 bytes it is 34 KB over. **Owner decision:
  which unit** (no PB).

### 2.2 Test findings (PB-86 ... PB-89)

13 new tests, none weakened. Five are not in the run list (TC11.5, TC16.6, TC16.7, TC16.8, TC29.38), so they never ran
with the suite. TC21.3 overwrites the real gas meter calibration on the board. TC11.5 uses 0.35 for a rule of 0.4 and
has no negative case. Several UI tests search the page source for identifiers and Romanian texts that are in no
requirement. The tests were derived from the code, not written first.

## 3. Console endpoints (task item 3)

- `/api/console/logs` (GET), `/api/console/send` and `/api/console/cmd` (POST) are registered in `NetworkConsole.cpp`,
  not in `WebApi.cpp`. No authentication, `Access-Control-Allow-Origin: *`.
- A command reaches the boiler (ON / OFF over 433 MHz), the runtime reset and the relay test. The log can hold the
  setup access point password (PB-43).
- Already tracked: PB-43, PB-44, PB-46, PB-47, PB-50, PB-51. New: PB-82 (the live console of the page: it sends no
  credentials and breaks or stays open when PB-44 is fixed).
- `compliance.md` status: CMP-5 partial, CMP-11 missing, CMP-13 missing; no new evidence. `compliance.md` itself was not edited.

## 4. Decisions for the owner (no requirement written)

1. How the console authenticates (HTTP and Telnet); how the page does it (browser prompt, token, login in the page). FR-58.
2. Whether reading `/api/console/logs` needs authentication or redaction (D49, CMP-8).
3. Whether the alias `/api/console/cmd` stays; FR-45 names only `/send`.
4. D42 (Telnet) after item 1.
5. A requirement for security events (failed logins, console command use), CMP-13.
6. Keep the out-of-chain work and fold it into steps 7 / 8 / 11 with tests first, or revert (PB-83).
7. Wording of FR-34 (ranges), FR-40 (sliding window, display), FR-35 counter names, FR-31 schedule model (PB-90, PB-92).
8. Is the 10 min window baseline wanted (PB-72): change the code to 5 min or the requirement.
9. Unit of the 1.25 MB limit (section 2.1).

## 5. What the owner must test hands-on on the board (1.11.03.x)

Plain list, in Romanian, also given in the final report:

1. `OutputBank` (releul și polaritatea vanelor): vanele se deschid și se închid la comandă; abia apoi OK pentru pasul 3.
2. Programul: toate cele patru moduri (constant, săptămânal, 5+2, 7 zile); temperaturile de zi / noapte de dinainte să fie cele de acum (PB-75).
3. O cameră cu 0,5 °C sub țintă: vana rămâne deschisă continuu; la țintă revine la ciclul de o oră.
4. Contorul de gaz: calibrare, istoric, estimarea; nu schimba indexul decât dacă vrei.
5. Graficul: 1h / 12h / 24h / 3 zile / 5 zile, steagul de repornire, bara de fereastră.
6. Fereastra deschisă: cardul cu numărătoare și butonul de anulare.
7. Importul: previzualizarea înainte de confirmare, apoi exportul și importul.
8. Consola din pagină (e fără parolă, vezi decizia 1).
9. După fiecare test, rândul în `docs/acceptance.md` îl scrii tu.
