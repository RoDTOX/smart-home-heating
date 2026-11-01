# Development process

Lightweight ASPICE-style process: Markdown work products plus one checker,
[`tools/sh_process_check.py`](../tools/sh_process_check.py). No external tracker.
The checker reports; the owner decides and fixes. Heuristic checks are WARN, hard rules are ERROR.

## 1. Work products

| ASPICE | Work product | Where | Checked by |
|---|---|---|---|
| SYS.2 / SWE.1 | Requirements (`FR-x`, `NFR-x`) | `docs/requirements_and_features.md` | `sh_process_check.py reqs`, `trace` |
| SYS.2 / SWE.1 | Requirements review record | `docs/requirements_review.md` | §3 exit criteria |
| SWE.2 | Architecture; requirement → module allocation (§3.2 Requirements column) | `docs/architecture.md` | `trace` |
| SWE.5 | System tests (`SWE.5-TCxx.y`) | `tools/sh_test_system.py` | `trace` (incl. hollow tests) |
| SUP.10 | Decisions | `.agent/memory-bank/decisions.md` | §4 |
| SUP.8 | Change record | `docs/changelog.md`, `.agent/memory-bank/activeContext.md` | `process` |
| SUP.9 | Problems | `docs/problems.md` | `process`, `release` (§8) |
| SYS.5 | Owner acceptance on the board | `docs/acceptance.md` | `release` (§5) |
| SPL.2 | Release | tag `vX.Y.Z` on main = `APP_VERSION` | `release` (§5) |
| — | Board configuration | live, `GET /api/config/system` | `sh_process_check.py board` |

Parallel Claude chats each own a zone of these files (AGENTS.md §5 "One chat, one zone").

## 2. Writing requirements

| Rule | Text | Check (`reqs` / `trace`) |
|---|---|---|
| R1 ID | Every requirement starts with `- **FR-x (Title):**`. An ID is never reused; a retired requirement is struck through with `~~`. IDs are cited in full (`FR-6.1, FR-6.2`), never as a range. | ERROR on duplicate ID |
| R2 One value | A quantity (timeout, interlock, overrun, burner run/pause, frost, heap, …) gets its default and allowed range in **one** requirement, named as a parameter (`sensorTimeout`). Other requirements cite the parameter name, not a number. | WARN "N different values" |
| R3 Measurable | Each requirement states a number with a unit or an observable result (relay state, HTTP code, DTC, API field). Words like *fast, instant, immediately, smart, optimal, robust* are not allowed without a number. | WARN "vague wording" |
| R4 DTC | A DTC is defined once, in the DTC list, as `` `DTC-xxx`: `ERR_NAME` `` (meaning). Every other citation carries the name. The firmware raises only defined DTCs. | WARN on 2 names, unnamed citation, undefined, never raised |
| R5 Testable leaf | One requirement = one behaviour a test can fail on. Tests cite the leaf ID (`FR-10.2`), not the parent. | WARN "only covered via parent" |
| R6 Language | English (AGENTS.md §6). | — |

`reqs` is a heuristic: it finds a quantity by keyword and takes the nearest value. A WARN is a question for the
review, not a verdict. When a new safety quantity appears, add it to `QUANTITIES` in the checker.

## 3. Requirements review (SYS.2)

**When:** before a phase starts, after any requirement change, or when `reqs` / `trace` reports a conflict
on a safety quantity.

**Input:** the requirements doc, output of `sh_process_check.py trace` and `reqs`, firmware reality (code and
`sh_process_check.py board`).

**Steps:**

1. Run the checker; every WARN/ERROR on a requirement becomes a row in `docs/requirements_review.md`.
2. Classify each row: wrong, conflict, untestable, ambiguous, obsolete, missing.
3. Propose: keep, rewrite (with the new text), retire, new.
4. Questions only the owner can answer become `D-n` rows in the review record (§4).
5. The owner approves row by row. Approved rewrites are applied to the requirements doc.
6. Re-run the checker.

**Exit criteria (review closed):**

- `trace`: no ERROR.
- `reqs`: no "different values" WARN on a safety quantity (sensor timeout, interlock, overrun,
  anti-short-cycling, frost), no DTC warning.
- Every `D-n` decided and recorded (§4).
- Owner sign-off: date and "closed" in the review record header.

Only after the review is closed are tests changed to follow the new requirements, then code to pass the tests.

## 4. Decisions

- **Open** decisions are `D-n` rows in the review record (or a line under "Next steps" in `activeContext.md`
  when there is no review). Agents do not act on an open decision.
- **Decided:** append an entry to `.agent/memory-bank/decisions.md` (append-only):
  `## YYYY-MM-DD — Title`, then **Context**, **Decision**, **Decided by** (owner), **Affects**
  (requirement IDs, files), **Consequences**. Mark the `D-n` row with the date.
- A decision that changes a safety value (AGENTS.md §3 list) also needs a requirement change (§3) before code.

## 5. Commit, acceptance and release

Work happens on a branch; `main` only receives accepted builds.

| Step | Gate | Who / how |
|---|---|---|
| 1. Commit | `pio run` clean; `sh_process_check.py` no ERROR; `sh-safety-reviewer` no BLOCKER when `src/`, `include/` or `platformio.ini` changed; changelog + activeContext updated; message `type(scope): subject`, citing `PB-n` when it fixes a problem | `sh-verify-firmware` skill (`no-board`). Agents commit only on explicit owner request. |
| 2. Flash | Build from the commit (board reports a clean hash, not `-dirty`); `sh_test_system.py` pass; `sh_process_check.py board` no ERROR | `sh-verify-firmware` skill |
| 3. Accept | Owner uses the heating for real (hours to days) and adds a row to `docs/acceptance.md`: board commit, what was checked, `OK`/`NOK`. A `NOK` opens `PB-n` rows. | Owner (a chat may write the row from the owner's words) |
| 4. Release gate | `sh_process_check.py release` no ERROR: an `OK` row for a build whose firmware equals HEAD (docs-only commits after it are fine), no open safety problem, `APP_VERSION` not already tagged with other firmware | Any chat, before asking to push |
| 5. Push + PR | Push the branch, open a PR to `main`. PR body: `release` output and the acceptance row. CI green. | Owner says "push" |
| 6. Merge + tag | Owner merges; tag `vAPP_VERSION` on main (`git tag v1.2.0 && git push --tags`). The next firmware change bumps `APP_VERSION` in `include/Version.h`. | Owner |
| CI (every push) | `pio run`, then `sh_process_check.py --base … --require-firmware` (trace + reqs + process) | `.github/workflows/build.yml` |

WARN lines do not block a commit, but each one is either fixed or named in the commit body.
Nothing is pushed before the owner's acceptance (step 3), and only on the owner's explicit request.

## 6. Board check

The board keeps its settings in NVS, so a correct build can still run with unsafe values.

1. Before flashing: `python tools/sh_process_check.py board` — record what the board runs.
2. After flashing: run it again. ERROR (a live value outside the safety limits) blocks "done";
   the owner decides the value (§4) and sets it through the API.
3. The commit the board reports must match `HEAD`; otherwise the test results belong to another build.
   `tools/sh_git_version.py` (PlatformIO pre-script) builds the commit into the firmware as
   `abc1234`, or `abc1234-dirty` when `src/`, `include/`, `web/`, `lib/` or `platformio.ini` had
   uncommitted changes. A `-dirty` build is fine for a hands-on test, but not as the result of record.

## 7. Known process gaps

| Gap | Effect | Status |
|---|---|---|
| Build identity: `include/Version.h` hard-coded `APP_GIT_COMMIT "8065534"` | The board always reported the same commit | **closed 31.10.2026:** `tools/sh_git_version.py` writes it on every build; `process` errors on a hard-coded commit |
| Burner minimum pause not in `GET /api/config/system` | Cannot be checked on the board | open |
| SWE.2 dependency rules R-A1…R-A7 (`architecture.md` §3.3) not checked | Code can cross module boundaries unseen | **partly closed 01.11.2026:** `trace` checks the requirement → module allocation; the R-A grep check (AR-20) is open |
| Decisions (§4) not machine-checked | Relies on discipline | accepted for now |

## 8. Problems (SUP.9)

Anything that goes wrong in operation, or a checker finding that is not fixed at once, gets a row in
[`problems.md`](problems.md) - not only a chat message, which is lost when the chat ends.

1. **Open:** next `PB-n`, date, symptom in the owner's words, the commit the board reports, `Safety` yes/no.
2. **Analyse:** status `analysing`, cause when known. A cause in the requirements goes to the requirements
   review (§3), a decision to §4.
3. **Fix:** the fixing commit cites `PB-n`; the row gets the commit and status `fixed`.
4. **Close:** after the fixed build is accepted on the board (§5 step 3), status `closed`.
   Not a problem: `rejected`, with the reason.

Checks: `process` errors on a malformed row, a `fixed`/`closed` row without a commit, or a commit citing an
unknown `PB-n`; an open safety problem is a WARN, and an ERROR in `release`.
