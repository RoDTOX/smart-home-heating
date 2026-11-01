# Process Review — process, requirements, architecture

> ASPICE SUP.1-style review of the process work products, done **before** the code and tests are rebuilt.
> Date: 01.11.2026. Reviewer: main chat (Claude), asked by the owner.
> Status: **approved by the owner, 01.11.2026 — all rows, with the recommended option for QA-12, QA-16 and QA-23** (chat).
> No reviewed file was changed by this review; the rows are implemented by the follow-up sessions.

## 1. Scope and method

**Reviewed:** `AGENTS.md`, `docs/process.md`, `docs/requirements_and_features.md` (baseline v2),
`docs/requirements_review.md`, `docs/architecture.md`, `docs/problems.md`, `docs/acceptance.md`, `docs/plan.md`,
`docs/underfloor_heating_hydraulics_and_failsafes.md`, `.agent/memory-bank/` (`decisions.md`, `activeContext.md`,
`systemPatterns.md`), `.claude/agents/sh-safety-reviewer.md`, `.claude/skills/sh-verify-firmware/SKILL.md`,
`.github/workflows/build.yml`, and the output of `tools/sh_process_check.py` (`all`, `reqs`) at the time of the
review (21 errors, 103 warnings).

**Not reviewed** (owner: rebuilt anyway): firmware (`src/`, `include/`, `web/`) and `tools/sh_test_system.py`.
Code facts below are quoted from the architecture gap list (§17) and the requirements review (§5), not re-checked.

**Checked for:** a rule exists and is followed; work products agree with each other; every source an agent reads
agrees with the approved baseline; traceability is complete in both directions; ASPICE labels mean what an
assessor expects.

**Rows:** `QA-n`. Category: **process** (rule not followed or missing), **conflict** (two sources disagree),
**ambiguous**, **missing**, **stale** (outdated source that agents or assessors still read), **label** (ASPICE
naming). Proposal: **fix**, **add**, **rewrite**, **decide** (owner question). `Owner` column: ✅ / ❌ / date when
decided.

## 2. Blocking — fix before code work starts

| ID | Finding | Evidence | Category | Proposal | Safety | Owner |
|---|---|---|---|---|:-:|:-:|
| QA-1 | `problems.md` is empty while known defects run on the board. Process §8 requires a `PB-n` row for every finding not fixed at once. | Arch §17 G2 (`/api/boiler/on` and console `ON` start the burner in AUTO without interlock), G3 / review C3 (no overrun without pump = reference install), review C1 (frost may enable boiler without interlock), C5 (API clamps from 1 s), board `windowThreshold` 0.3 °C on Z2/Z3, hollow safety tests TC07.2, TC07.3, TC07.4, TC09.1 | process | **add** one `PB-n` per item, `Safety = yes` for G2, G3, C1, C5 and the hollow safety tests. The decision "G2 fixed in steps 4 and 9, no hotfix" stays valid; the open PB row makes it block release as intended. | y | ✅ 01.11.2026 |
| QA-2 | `AGENTS.md` safety list contradicts baseline v2 on three points. Agents read `AGENTS.md` first, with the highest priority. | `AGENTS.md:33` interlock 120–180 s vs §6 120–600 s; `AGENTS.md:36` frost "< 12 °C" vs `frostTemp` 8–16 °C; `AGENTS.md:37` "sensor timeout isolation" vs FR-51 / D35 (zone keeps heating at learned duty). `activeContext.md` lists only the first two as open. | conflict | **rewrite** the list to cite requirement IDs and parameter names instead of numbers: "interlock `interlockDelay` (FR-2), overrun (FR-3), burner guard (FR-4), frost `frostTemp` (FR-16.1), sensor loss → FR-51 fallback with DTC-101, safe boot (NFR-1.4); values and ranges in requirements §6." | y | ✅ 01.11.2026 |
| QA-3 | `sh-safety-reviewer` hard-codes the old rules. It would report a correct FR-51 implementation as BLOCKER, and its reboot rule contradicts NFR-1.4. | `.claude/agents/sh-safety-reviewer.md:32,35,36` (120–180 s, < 12 °C, "zone keeps demanding heat on a dead sensor"); `:41` "boiler OFF **or valves open**" vs NFR-1.4 (valves closed, boiler OFF). | conflict | **rewrite**: the agent reads requirements §6 and architecture §6.2 invariants I-1…I-8 at run time and checks the diff against them; no copied values; code locations only as hints. | y | ✅ 01.11.2026 |
| QA-4 | Requirements review is not formally closed, and its exit criterion cannot be met. | `requirements_review.md:4` still "proposal — awaiting row-by-row approval" although every row is ✅; `process.md:58` demands no "different values" WARN on a safety quantity, but `reqs` reports heuristic false positives: FR-37 "1 s" (control tick), FR-3.2 "0 s", FR-16.1 "+1.5 °C" (release hysteresis), NFR-2.2 / NFR-2.6 heap thresholds. | process | **add** a waiver table to the review record (WARN text, reason, owner ✅); `reqs` lists waived lines as INFO. Then the owner signs "closed" with date in the header (`process.md:61`). | y | ✅ 01.11.2026 |
| QA-5 | Decided items are not in `decisions.md` (process §4). It still holds opposite decisions that were never superseded. | No entry after 2026-10-22. Missing: D1–D36, AR-1…AR-20, O-1…O-11, the 01.11 quiz ("§6 ranges win over AGENTS.md"), the order "code and tests together". Not superseded: Home Assistant, kiosk, MQTT / Galaxy A6, PWA, PocketBase, ESP-NOW master/slave, SaaS. | process | **add** one entry per decision group (with `Decided by`, `Affects`) and one "Superseded" entry naming each retired decision and what replaces it. Optional: `process` check that every ✅ `D-n` / `AR-n` / `O-n` has an entry. | n | ✅ 01.11.2026 |
| QA-6 | Order of tests and code differs between process and architecture; neither has a test review step. The hollow tests and the bent TC16.5 (30.10.2026) both came from tests written to match code — the risk grows when tests follow the code, as the owner decided. | `process.md:63` "tests changed first, then code"; `architecture.md:771` "no separate test step first; each step changes code and tests together"; `activeContext.md` "tests follow the code (not TDD)" (owner, 01.11.2026). | conflict / missing | **rewrite** `process.md` §3 to the owner's order (code, then tests, same step) and **add** a test review gate before the step is flashed: each changed test cites a leaf ID and asserts the value or observable result written in the requirement text (never a value read from the code), no hollow-test WARN; a test that cannot pass without a different value opens a `PB-n` or a requirements review row, it is not adjusted. | y | ✅ 01.11.2026 |

## 3. Requirements (baseline v2)

| ID | Finding | Evidence | Category | Proposal | Safety | Owner |
|---|---|---|---|---|:-:|:-:|
| QA-7 | No SYS / SW level. The title says "System and Software Requirements", but no requirement says whether it is met by software, hardware or the installer, so the architecture allocates installer and hardware items to software modules. | Line 1; FR-5.2 (radio range), FR-55 wired branch (dry NO contact), FR-68 (commissioning), §2 preconditions; arch §3.2 allocates FR-5.2 to `BoilerLink` / `Maintenance`, FR-68 to `Maintenance`. | missing | **add** an attribute per requirement: `SW` / `HW` / `install` (or a column in a short index table). Tailoring statement in `process.md`: one document for SYS.2 and SWE.1. | n | ✅ 01.11.2026 |
| QA-8 | No verification method per requirement. Requirements that only inspection, analysis or a multi-day measurement can verify stay "has no test" forever, so coverage numbers are wrong. | FR-68, FR-5.2, NFR-3.1, NFR-3.2, NFR-5.1, NFR-5.2, NFR-4.1 (days of data), NFR-2.6 (24 h). | missing | **add** `Verify: test / board / inspection / analysis`; `trace` counts coverage per method. | n | ✅ 01.11.2026 |
| QA-9 | FR-4 states fixed values that §6 makes a default with a range (rule R2). Read literally, FR-4 forbids the 120 s that §6 allows. | FR-4 (line 117) "minimum run time 180 s and minimum pause 180 s"; §6 `burnerMinRun` / `burnerMinPause` 120–600 s. FR-2 repeats "default 180 s" as well. | conflict | **rewrite** FR-4: "at least `burnerMinRun` ON and `burnerMinPause` OFF"; FR-2 without the default number. | y | ✅ 01.11.2026 |
| QA-10 | "Flame lit" is not observable on a tier-A (ON/OFF) boiler: the controller has no flame feedback. | FR-3 (line 112), FR-3.2 (line 114); arch §7.2 `HOLD`, §7.3 "send ON, flame lit". | ambiguous | **rewrite**: define "flame lit" = an ON frame was sent and not cancelled (tier A); tier B may use real feedback later. | y | ✅ 01.11.2026 |
| QA-11 | No glossary. "Cycle" means both the 1 s control tick (FR-37) and the 60 min TPI cycle (FR-47); "demand" is not defined against TPI ON time and FR-51 fault duty. | FR-37 line 120, FR-47 line 70. Arch §0 already defines request / command. | ambiguous | **add** a glossary to requirements §0 (tick, TPI cycle, demand, request, command, accepted reading, flame lit) and use it in FR-37. | y | ✅ 01.11.2026 |
| QA-12 | OTA image validation is ambiguous: "sensors received within 60 s" — all or any? One sensor with a dead battery would roll back a good image. | FR-60 line 353–355; arch §7.7 `PENDING_VERIFY`. | ambiguous | **decide**, recommended: "at least one advert from a mapped sensor within 60 s". | n | ✅ 01.11.2026 |
| QA-13 | Burner pause across a reboot is undefined. With `burnerMinPause` up to 600 s and `interlockDelay` from 120 s, the burner can restart 2 min after an OFF sent just before a reset. | NFR-1.4, FR-4, §6 ranges. | missing | **add** to NFR-1.4: "the boot OFF starts `burnerMinPause`". | y | ✅ 01.11.2026 |
| QA-14 | Scope tags do not match the plan. About 25 `[now]` requirements are new features (authentication, OTA rollback, season logic, event history, plausibility, fault injection), while `AGENTS.md` §2 says the current phase is "make it lean" and forbids features outside it; `[phase 4]` is not the same as AGENTS phase 4. | `AGENTS.md` §2; requirements §0 phase tags; arch §18 steps 1–14. | conflict | **rewrite** `AGENTS.md` §2 and `plan.md`: the plan is the arch §18 migration; phase tags map to its steps. | n | ✅ 01.11.2026 |
| QA-15 | Review record errors and an unmanaged source. B-01 cites "FR-41..43" (wrong IDs and a range, rule R1). The owner's blueprint, source of B-01…B-21, is not in the repository. | `requirements_review.md:238`; `underfloor_heating_blueprint_2.pdf` not in `git ls-files`. | process | **fix** B-01 (zones = FR-11.3, FR-69); **add** the PDF to `docs/` or record its version and location. | n | ✅ 01.11.2026 |

## 4. Architecture

| ID | Finding | Evidence | Category | Proposal | Safety | Owner |
|---|---|---|---|---|:-:|:-:|
| QA-16 | Blocking in `loop()` contradicts itself and FR-37. §1 accepts only one radio frame; §5.1 accepts a slow HTTP client holding `loop()` about 5 s and §5.4 OTA up to 30 s. A 5 s hold breaks "valve closed within one tick (1 s)". | `architecture.md:41`, `:197–199`, `:260`; FR-37, FR-17.6. | conflict | **decide** a maximum reaction latency as a requirement (e.g. NFR "safety reaction ≤ 2 s except during OTA with boiler OFF"), then bound the `WebServer` timeout in the design. | y | ✅ 01.11.2026 |
| QA-17 | SWE.3, SWE.4 and SYS.3 / SYS.4 are referenced or implied but not tailored. | Arch §0 "parts are SWE.3", P9 "SWE.4 readiness"; no system architecture (receiver, sensors, wiring, boiler interface). | label | **add** a tailoring table to `process.md`: kept, merged (e.g. SWE.3 = code + header comments), or not applied, with the reason. | n | ✅ 01.11.2026 |
| QA-18 | RAM margin is about zero and there is no risk record. | §14.2: 110 KB measured, about +10 KB planned, NFR-2.2 ≥ 100 KB. | missing | **add** a short risk list (`docs/process.md` or `plan.md`): RAM, flash margin, receiver hardware, flow-probe library. | n | ✅ 01.11.2026 |
| QA-19 | Invariant I-3 does not name the FR-51 fault duty as a reason for an energised valve. | `architecture.md:301`. | ambiguous | **rewrite** I-3 with "∨ fault duty", or define demand = any valve request (QA-11). | y | ✅ 01.11.2026 |

## 5. ASPICE labels, traceability, tooling

| ID | Finding | Evidence | Category | Proposal | Safety | Owner |
|---|---|---|---|---|:-:|:-:|
| QA-20 | Process labels an assessor will read differently. | `process.md:14–18`: SUP.10 is change request management (not decisions); SUP.8 is configuration management (git, tags, build identity, baselines), not the changelog; board tests against requirements are SWE.6 qualification tests, not SWE.5 integration tests; the owner's hands-on test is acceptance / validation, not SYS.5. | label | **rewrite** the §1 table; test IDs may keep their `SWE.5-` prefix if the table says what they are. | n | ✅ 01.11.2026 |
| QA-21 | No quality assurance (SUP.1) and no current project plan (MAN.3). Nobody checks the process is followed except the checker, and `plan.md` is not the arch §18 migration. | `process.md` §1; `plan.md:36–39`. | missing | **add** SUP.1 = this review record type, repeated per migration step (checker output + checklist + owner ✅); MAN.3 = `plan.md` rewritten as the 14 steps with status. | n | ✅ 01.11.2026 |
| QA-22 | Acceptance and problems are not traced to requirements. | `acceptance.md` columns (no requirement IDs); `problems.md` columns (no requirement, no "test that should have caught it"). | missing | **add** a `Requirements` column to both and a `Missed by` column to problems; `release` checks that accepted rows cover the safety IDs. | y | ✅ 01.11.2026 |
| QA-23 | CI fails on every push by design, so a new error looks the same as the known ones. | `.github/workflows/build.yml` runs `sh_process_check.py --require-firmware`; 21 known errors. | process | **decide**: known errors linked to an open `PB-n` (QA-1) are reported as KNOWN; CI fails only on new errors. | n | ✅ 01.11.2026 |
| QA-24 | `sh-verify-firmware` does not apply process §8 / §5. | `SKILL.md` step 4–6: a finding is only reported (no `PB-n` row); a `-dirty` flash is not marked as "not a result of record". | process | **add** both to the skill report. | n | ✅ 01.11.2026 |

## 6. Stale sources that agents still read

| ID | Finding | Evidence | Category | Proposal | Safety | Owner |
|---|---|---|---|---|:-:|:-:|
| QA-25 | `systemPatterns.md` describes the old system: MQTT / Galaxy A6, `DevicePassport`, `/console`, hysteresis loop, 60 s burner guard, 30 min sensor timeout. The anti-weakening and zero-gap rules exist only here. | `.agent/memory-bank/systemPatterns.md` | stale | **rewrite** as a pointer to `architecture.md`; **move** the two rules into `process.md`. Same check for `techContext.md`, `productContext.md`, `projectbrief.md`, `progress.md`. | y | ✅ 01.11.2026 |
| QA-26 | `activeContext.md` "Next steps" holds closed items and repeats invalid evidence. | `:10` "78/78 PASS, 48/48 covered" (invalid: tests did not follow the requirements, hollow safety tests); `:31` FR-41..45 (done); `:33` sensor-loss decision (decided, D35). | stale | **rewrite** the line at `:10` as "78/78, not valid as evidence (see QA-1)" and remove closed items. | n | ✅ 01.11.2026 |
| QA-27 | `plan.md` contradicts approved decisions. | `:36` 78/78 marked done; `:38` "sensor-loss needs a decision" (D35); `:54` boost 16–22.5 °C (D10: +0.5…2.0 °C). | stale | **rewrite** with QA-21. | n | ✅ 01.11.2026 |
| QA-28 | The hydraulics document contradicts the baseline although requirements cite it (NFR-1.7 moved there). | `underfloor_heating_hydraulics_and_failsafes.md:83` TWDT 8 s (NFR-1.5: 90 s); `:84` sensor loss closes the valve after 30 min (FR-16.2 / FR-51); Home Assistant and `/setup` references; Romanian (NFR-3.1). | stale | **rewrite** in English against baseline v2, or mark the failure table as superseded by architecture §8.2. | y | ✅ 01.11.2026 |

## 7. What is sound

- Requirement rules R1–R6, one parameter table (§6), DTCs defined once (§5), a fallback clause per feature (NFR-5).
- `SafetySupervisor` as the only writer of outputs, invariants I-1…I-8, validity `Ok` / `Held` / `Lost` per input.
- Release gate tied to the owner's acceptance and to the build identity (`sh_git_version.py`).
- Checker modes `trace`, `reqs`, `process`, `board`, `release`, including hollow-test detection.
- Chat zones (`AGENTS.md` §5) and append-only shared logs.

## 8. Recommended order

1. QA-1…QA-6 (blocking).
2. QA-7, QA-8 (they change the shape of the requirements document, so do them before more rows are written).
3. QA-9…QA-19, then QA-20…QA-28.
