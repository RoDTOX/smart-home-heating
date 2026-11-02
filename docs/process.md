# Development process

Lightweight ASPICE-style process: Markdown work products plus one checker,
[`tools/sh_process_check.py`](../tools/sh_process_check.py). No external tracker.
The checker reports; the owner decides and fixes. Heuristic checks are WARN, hard rules are ERROR.
The checker never hides a finding: an ERROR explained by an open `PB-n` is shown as `KNOWN` (§8), a WARN the
owner accepted is shown as `INFO waived` (§3). The default run prints each section's WARN lines as one count (owner,
02.11.2026, to save tokens); a named check (`trace`, `reqs`, `arch`, `process`), `--full` and CI list them.

## 1. Work products

| ASPICE | Work product | Where | Checked by |
|---|---|---|---|
| SYS.2 / SWE.1 | Requirements (`FR-x`, `NFR-x`) with level and verification method (R7) | `docs/requirements_and_features.md` | `sh_process_check.py reqs`, `trace`, `sh-req-reviewer` |
| SYS.2 / SWE.1 | Requirements review record, with the accepted checker warnings (its §7) | `docs/requirements_review.md` | §3 exit criteria; `reqs` / `trace` read the waivers |
| SWE.2 | Architecture; requirement → module allocation (§3.2 Requirements column); dependency rules R-A1…R-A7 (§3.3) | `docs/architecture.md` | `trace`, `arch`, `sh-code-reviewer` (code against it) |
| SWE.6 | Qualification tests against the requirements, run on the board. IDs keep their historical `SWE.5-TCxx.y` prefix so they stay stable | `tools/sh_test_system.py` → `docs/system_test_report.md` | `trace` (incl. hollow tests), `sh-test-reviewer` (§3.2, clean room); written first by `sh-test-writer` (§3.1) |
| VAL.1 | Owner acceptance: the owner uses the heating on the board | `docs/acceptance.md` | `release` (§5) |
| SUP.1 | Quality assurance: one review record per migration step | `docs/process_review.md`, then `docs/review_step_<n>.md` (§9) | `sh-assessor` (independent check of the evidence), owner ✅ |
| SUP.8 | Configuration management: branches, commits, tags, build identity, baselines | git, `tools/sh_git_version.py`, `include/Version.h` | `process`, `release`, `board` |
| SUP.9 | Problems | `docs/problems.md` | `process`, `release` (§8) |
| SUP.10 | Change requests and their record: review rows (`QA-n`, `D-n`, `AR-n`, `O-n`), changelog | review records, `docs/changelog.md`, `.agent/memory-bank/activeContext.md` | `process` |
| MAN.3 | Project plan (architecture §18 steps with status) and decisions | `docs/plan.md`, `.agent/memory-bank/decisions.md` | §4, §9 |
| SPL.2 | Release | tag `vX.Y.Z` on main = `APP_VERSION` | `release` (§5) |
| — | Board configuration | live, `GET /api/config/system` | `sh_process_check.py board` |
| — | EU conformity (CE) of the software: obligations, the requirements that meet them, technical file | `docs/compliance.md` | `sh-compliance-reviewer` (§9) |

The owner does not run these steps: what they report or ask starts the chain `sh-change` (AGENTS.md §5), which
goes through §2-§9 in order and asks the owner only to decide a requirement change and to test on the board.

### 1.1 Tailoring

| Process | Here | Reason |
|---|---|---|
| SYS.2 + SWE.1 | **merged:** one requirements document; the LEVEL attribute (`SW`, `HW`, `install`, R7) says who meets each requirement | one product, one owner; system and software requirements share most IDs |
| SYS.3 System architecture | **merged** into SWE.2: architecture §1 (context) and §12 (abstractions), plus `hardware_esp32_relay.md` and `underfloor_heating_hydraulics_and_failsafes.md` | the hardware is bought, not designed; one controller board |
| SYS.4 System integration test | **merged** into SWE.6 (board tests) and the commissioning checklist (FR-68) | the system is integrated once, in the reference install |
| SYS.5 System qualification test | **merged** into SWE.6 (the tests run against the real plant) and VAL.1 | same board, same plant |
| SWE.3 Detailed design and construction | **merged:** architecture §4–§7 (interfaces, state machines) plus code with header comments | small code base; design detail lives in the architecture |
| SWE.4 Unit verification | **not applied yet:** static checks instead (`pio run`, `sh_process_check.py`, `sh-safety-reviewer`); host unit tests (architecture P9, PlatformIO Unity) can come later | no host build yet; no new library |
| SWE.5 Software integration test | **merged** into SWE.6 | one binary, tested as a whole on the board |
| SWE.6, VAL.1, SUP.1, SUP.8, SUP.9, SUP.10, MAN.3, SPL.2 | **applied**, light (Markdown records, one checker) | — |
| ACQ, SPL.1, MAN.5, MAN.6, REU, HWE | **not applied** | one owner, no supplier, no hardware design |

## 2. Writing requirements

| Rule | Text | Check (`reqs` / `trace`) |
|---|---|---|
| R1 ID | Every requirement starts with `- **FR-x (Title):**`. An ID is never reused; a retired requirement is struck through with `~~`. IDs are cited in full (`FR-6.1, FR-6.2`), never as a range. | ERROR on duplicate ID |
| R2 One value | A quantity (timeout, interlock, overrun, burner run/pause, frost, heap, …) gets its default and allowed range in **one** requirement, named as a parameter (`sensorTimeout`). Other requirements cite the parameter name, not a number. | WARN "N different values" |
| R3 Measurable | Each requirement states a number with a unit or an observable result (relay state, HTTP code, DTC, API field). Words like *fast, instant, immediately, smart, optimal, robust* are not allowed without a number. | WARN "vague wording" |
| R4 DTC | A DTC is defined once, in the DTC list, as `` `DTC-xxx`: `ERR_NAME` `` (meaning). Every other citation carries the name. The firmware raises only defined DTCs. | WARN on 2 names, unnamed citation, undefined, never raised |
| R5 Testable leaf | One requirement = one behaviour a test can fail on. Tests cite the leaf ID (`FR-10.2`), not the parent. | WARN "only covered via parent" |
| R6 Language | English (AGENTS.md §6). | — |
| R7 Level and method | Right after the phase tag (after the label when there is none): `{LEVEL; METHOD}`, e.g. `- **FR-2 (Title):** *[now]* {SW; test} …`. LEVEL is `SW`, `HW` or `install`; METHOD is `test`, `board`, `inspection` or `analysis`; both may be comma lists (`{SW, install; test, inspection}`). `test` and `board` are verified by a test in `tools/sh_test_system.py` (`board`: needs the installed plant or a long run); `inspection` and `analysis` are verified without a test, the result goes into the step review record (§9). A LEVEL without `SW` needs no software module. Retired (`~~`) items have none. | WARN missing / unknown value; `trace` counts coverage per method, only `test` / `board` need a test |

`reqs` is a heuristic: it finds a quantity by keyword and takes the nearest value. A WARN is a question for the
review, not a verdict. When a new safety quantity appears, add it to `QUANTITIES` in the checker.

## 3. Requirements review (SYS.2)

**When:** before a migration step that needs a requirement change, after any requirement change, or when `reqs` /
`trace` reports a conflict on a safety quantity.

**Input:** the requirements doc, output of `sh_process_check.py trace` and `reqs`, firmware reality (code and
`sh_process_check.py board`).

**Steps:**

1. Run `sh_process_check.py reqs` and `trace`; every WARN/ERROR on the **requirement side** (below) becomes a row in
   `docs/requirements_review.md`.
2. Classify each row: wrong, conflict, untestable, ambiguous, obsolete, missing, false positive.
3. Propose: keep, rewrite (with the new text), retire, new, or waive (a false positive of the heuristic, below).
4. Questions only the owner can answer become `D-n` rows in the review record (§4).
5. The owner approves row by row. Approved rewrites are applied to the requirements doc, approved waivers to its
   §7 table.
6. Re-run the checker.

**Requirement side, test side, code side.** `trace` and `reqs` report all three:

- *Requirement side* (review rows): duplicate ID, different values of a quantity, vague wording, DTC defined twice,
  with two names, cited without its name or never defined, level / method missing or unknown (R7).
- *Architecture side* (architecture review): a requirement without a module, a module citing an inactive ID.
- *Test and code side:* hollow test, test citing an unknown ID, code tag without a requirement, DTC raised but
  undefined — a defect, so a `PB-n` row (§8). A requirement not built yet (no test, DTC never raised) is work of
  its migration step in `plan.md`, not a problem. Neither keeps the review open.

**Accepted checker warnings (waivers).** §7 of the review record, `## 7. Accepted checker warnings`:

| # | Check | WARN text | Reason | Owner |
|---|---|---|---|---|
| W-1 | reqs | free heap: 3 different values - 100 KB (NFR-2.2: | four heap quantities, not one value stated twice | ✅ 01.11.2026 |

- `Check` is `reqs` or `trace`. `WARN text` is a substring of the WARN line (backticks optional), without line
  numbers. It waives the whole WARN line; for a "different values" WARN it may instead match one value, written
  `quantity: value (ID:line)` (e.g. `interlock: 1 s (FR-37`): that value is left out of the comparison and the
  WARN stays for the others.
- A waiver applies only with ✅ and a date in `Owner`. The checker prints a matching WARN as `INFO waived (#n)`
  and names a waiver that matches nothing, so it can be removed.
- A waiver never covers an ERROR, and never a real second value of a safety quantity: that is a rewrite.

**Exit criteria (review closed)** — requirement-side findings only:

- `trace`: no ERROR on the requirement side.
- `reqs`: no un-waived "different values" WARN on a safety quantity (sensor timeout, interlock, overrun,
  anti-short-cycling, frost), no requirement-side DTC warning ("never raised by the firmware" is code side).
- Every `D-n` decided and recorded (§4).
- Owner sign-off: date and "closed" in the review record header.

### 3.1 Order of work (owner, 02.11.2026 - replaces "tests follow the code" of 01.11.2026)

Requirements → architecture → tests → code, one migration step at a time (architecture §18, AR-19).
A requirement change a step needs goes through §3 before its tests are written.

1. **Tests first, in a clean room.** `sh-test-writer` writes the step's tests from the requirements, the
   architecture and `api_reference.md`; `sh-test-reviewer` reviews them. Both agents cannot read `src/`,
   `include/`, `web/`, `lib/` or the other tools, and the writer can edit only `tools/sh_test_system.py`
   (hook `.claude/hooks/sh_clean_room.py`). Skill `sh-write-tests`.
2. **Red run.** On the firmware before the step, every new test fails. One that passes is behaviour that exists
   already (the owner confirms it) or a hollow test (rewritten).
3. **Freeze.** Inside `sh-change` the frozen tests are committed alone, before the code (evidence for
   `sh-assessor`). The code session makes the tests pass by changing the code only. A test that cannot pass is a
   `PB-n` or a requirements review row (§3.2); only `sh-test-writer` changes it, after the owner's decision.
4. **Green run** on the flashed step (§5 step 2), then the owner's hands-on test.

| Work product | Judged against | Reviewer |
|---|---|---|
| Requirements | rules §2, architecture, safety list | `sh-req-reviewer` |
| Tests | requirements, architecture, API reference - never the code | `sh-test-reviewer` (clean room) |
| Code | requirements, architecture, API reference, current step | `sh-code-reviewer`, `sh-safety-reviewer` |
| Links | requirement → module → test → code tag | `sh_process_check.py trace` |

Skill `sh-review` (`reqs | tests | code | trace | all`) runs them together.

### 3.2 Test rules and test review gate

- **Anti-weakening.** Tests exist to find defects. An assertion, tolerance, timeout or expected value is never
  weakened, removed or copied from the code to make a test pass. A failing test is diagnosed: the code is fixed,
  or a `PB-n` row (code wrong) or a requirements review row (requirement wrong or untestable) is opened; the test
  stays as it is until the owner decides.
- **Zero-gap audit.** At the start of every session that changes firmware or tests, compare code (API endpoints,
  console commands, behaviour), requirements and tests. Behaviour without a requirement, or a requirement of the
  current step without a test, is reported to the owner, who decides whether it is specified and tested before
  work goes on. `trace` lists the untested requirements; the rest is read by hand.

**Test review gate** — before the step is flashed (§5 step 2), for every new or changed test:

1. its docstring cites a leaf ID (`FR-10.2`), not the parent (R5);
2. it asserts the value or observable result written in the requirement text or §6 (default, range, DTC, HTTP
   code, relay state, API field) — never a value read from the code;
3. `trace` shows no hollow-test WARN for it;
4. a test that cannot pass without a different value opens a `PB-n` or a requirements review row; it is not
   adjusted.

`sh-test-reviewer` runs the gate in the clean room (`sh-verify-firmware` step 3 launches it); its result goes into
the step review record (§9).

## 4. Decisions (MAN.3)

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
| 1. Commit | `pio run` clean; `sh_process_check.py` no ERROR (`KNOWN` lines are tracked by a `PB-n` and do not block); `sh-safety-reviewer` no BLOCKER when `src/`, `include/` or `platformio.ini` changed; changelog + activeContext updated; message `type(scope): subject`, citing `PB-n` when it fixes a problem | `sh-verify-firmware` skill (`no-board`). Inside `sh-change` the chat commits on the work branch once the gates pass (owner, 02.11.2026); otherwise only on request. |
| 2. Flash | `sh-verify-firmware` steps 1–4 pass (build, process check, test review §3.2, safety review); built from the commit (a `-dirty` build may be tested, but it is not a result of record, §6); then `sh_test_system.py` pass; `sh_process_check.py board` no ERROR | `sh-verify-firmware` skill. The firmware session may flash itself; only one firmware session at a time (owner, 01.11.2026). |
| 3. Accept | Owner uses the heating for real (hours to days) and adds a row to `docs/acceptance.md`: board commit, requirement IDs covered, what was checked, `OK`/`NOK`. A `NOK` opens `PB-n` rows. | Owner (a chat may write the row from the owner's words) |
| 4. Step review | Review record of the step (§9) with owner ✅; then the next migration step may start | Any chat writes it, owner signs |
| 5. Release gate | `sh_process_check.py release` no ERROR: `OK` rows for a build whose firmware equals HEAD (commits touching only docs after it are fine) that together cover every requirement ID of the AGENTS.md §3 safety list; no open safety problem; `APP_VERSION` not already tagged with other firmware | Any chat, before asking to push |
| 6. Push + PR | Push the branch, open a PR to `main`. PR body: `release` output and the acceptance rows. CI green. | Owner says "push" |
| 7. Merge + tag | Owner merges; tag `vAPP_VERSION` on main (`git tag v1.2.0 && git push --tags`). The next firmware change bumps `APP_VERSION` in `include/Version.h`. | Owner |
| CI (every push) | `pio run`, then `sh_process_check.py --base … --require-firmware` (trace + reqs + arch + process). Fails only on new ERRORs: an ERROR matched by the `Checker` column of an open or rejected `PB-n` is `KNOWN` (§8) | `.github/workflows/build.yml` |

WARN lines do not block a commit, but each one is either fixed or named in the commit body.
Nothing is pushed before the owner's acceptance (step 3), and only on the owner's explicit request.

## 6. Board check

The board keeps its settings in NVS, so a correct build can still run with unsafe values.

1. Before flashing: `python tools/sh_process_check.py board` — record what the board runs.
2. After flashing: run it again. ERROR (a live value outside the safety limits) blocks "done";
   the owner decides the value (§4) and sets it through the API. A value already tracked by an open `PB-n`
   is `KNOWN`.
3. The commit the board reports must match `HEAD`; otherwise the test results belong to another build.
   `tools/sh_git_version.py` (PlatformIO pre-script) builds the commit into the firmware as
   `abc1234`, or `abc1234-dirty` when `src/`, `include/`, `web/`, `lib/`, `tools/` or `platformio.ini` had
   uncommitted changes. `tools/` counts because the pre-build scripts shape the binary and the system test is part
   of the result. A `-dirty` build is fine for a hands-on test, but not as the result of record. The same paths
   decide "same firmware" at the release gate, so a change in `tools/` after the accepted build needs a new
   accepted build.

## 7. Known process gaps

| Gap | Effect | Status |
|---|---|---|
| Build identity: `include/Version.h` hard-coded `APP_GIT_COMMIT "8065534"` | The board always reported the same commit | **closed 31.10.2026:** `tools/sh_git_version.py` writes it on every build; `process` errors on a hard-coded commit |
| Burner minimum pause not in `GET /api/config/system` | Cannot be checked on the board | open |
| SWE.2 dependency rules R-A1…R-A7 (`architecture.md` §3.3) not checked | Code can cross module boundaries unseen | **closed 01.11.2026:** `trace` checks the allocation; `arch` greps R-A1…R-A7 as WARN (today's code does not follow them yet, architecture §17) |
| Acceptance and problems not traced to requirements | A release could skip a safety requirement | **closed 01.11.2026:** `Requirements` columns; `release` checks the safety IDs |
| CI failed on known errors | A new error looked like the known ones | **closed 01.11.2026:** `KNOWN` via the `PB-n` `Checker` column |
| Test review judges by reading | Whether a test value comes from the requirement is not machine-checked | partly: `trace` finds hollow tests; the rest is the §3.2 gate |
| Decisions (§4) not machine-checked | Relies on discipline | accepted for now |

## 8. Problems (SUP.9)

Anything that goes wrong in operation, or a checker finding that is not fixed at once, gets a row in
[`problems.md`](problems.md) - not only a chat message, which is lost when the chat ends.

1. **Open:** next `PB-n`, date, symptom in the owner's words, the requirement IDs it breaks, the commit the board
   reports, `Safety` yes/no, `Missed by` (the test that should have caught it), `Checker` (the ERROR text it
   explains, in backticks, without line numbers).
2. **Analyse:** status `analysing`, cause when known. A cause in the requirements goes to the requirements
   review (§3), a decision to §4.
3. **Fix:** the fixing commit cites `PB-n`; the row gets the commit and status `fixed`.
4. **Close:** after the fixed build is accepted on the board (§5 step 3), status `closed`.
   Not a problem, or accepted as is by the owner: `rejected`, with the reason in Cause.

**Known errors.** An ERROR line containing a `Checker` text of a row in status `open`, `analysing` or `rejected`
is printed as `KNOWN … (PB-n)` and does not fail the run; CI therefore fails only on new errors. A matching ERROR
after `fixed` / `closed` stays an ERROR (regression). Errors of the `release` gate and of the problems log itself
are never `KNOWN`. An open row whose `Checker` text matches nothing is named, so its status can be updated.

Checks: `process` errors on a malformed row (ID, status, Safety, a missing column), a `fixed`/`closed` row without
a commit, a `rejected` row without a cause, or a commit citing an unknown `PB-n`; it warns on an unknown requirement
ID; an open safety problem is a WARN, and an ERROR in `release`.

## 9. Plan and step review (MAN.3, SUP.1)

- **Plan:** [`plan.md`](plan.md) lists the steps of architecture §18 with their status; one step is in progress at
  a time.
- **Step review:** after each migration step, before the next one starts, a review record like
  [`process_review.md`](process_review.md), file `docs/review_step_<n>.md`:
  - checker output of the step (errors, known, warnings, waived);
  - checklist: AGENTS.md §4 done; tests written first (§3.1) with the red run's board commit and result; test
    review (§3.2) with the changed tests and their IDs; `sh-code-reviewer` and `sh-safety-reviewer` result; requirements verified by `inspection` / `analysis` (R7) with their result; `PB-n` opened or fixed;
    requirement and architecture changes went through §3; changelog, activeContext and plan updated;
  - findings as rows, owner ✅ with date.
- **Independent check:** at the end of the step, before the owner's board test, `sh-assessor` checks the evidence
  of this record and `sh-compliance-reviewer` the EU obligations the step touches (`compliance.md`); both results
  go into the record.
