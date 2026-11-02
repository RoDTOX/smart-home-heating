---
name: sh-review
description: Review requirements, tests and code, each against its own reference, and check that they are linked with no gaps (traceability) - requirements by sh-req-reviewer, tests by sh-test-reviewer (clean room, no code access), code by sh-code-reviewer and sh-safety-reviewer, links by sh_process_check.py trace. Use when the user asks for a review, a gap check, a traceability check, or before a step review. Args - reqs | tests | code | trace | assess | compliance | all (default all = reqs, tests, code, trace); add "all-tests" to review every test, not only the changed ones, and "all-rules" for every EU obligation.
---

# sh-review

Each work product is judged by its own reference, never by the one below it (`docs/process.md` §3, §3.2):
requirements by the rules and the architecture, tests by the requirements (without seeing the code), code by the
requirements and the architecture. Reviewers report; nobody fixes a finding here.

## 1. Links (always)

```
python tools/sh_process_check.py trace
```

Requirement → module → test → code tag, hollow tests, untested leaves. Report the ERROR count, the KNOWN `PB-n`
and the coverage lines. When the owner asks for the full table, add `--matrix` (writes `docs/traceability.md`).

## 2. Reviews

Launch the agents the argument asks for, **in parallel** (one message, several Agent calls); they are independent:

| Arg | Agent(s) | Scope to pass |
|---|---|---|
| `reqs` | `sh-req-reviewer` | the whole requirements document, or the IDs the caller names |
| `tests` | `sh-test-reviewer` | tests changed on the branch; `all-tests` - every test |
| `code` | `sh-code-reviewer` and `sh-safety-reviewer` | branch diff against `origin/main` plus uncommitted changes |
| `trace` | none | step 1 only |
| `assess` | `sh-assessor` | the step in progress, or the step the caller names |
| `compliance` | `sh-compliance-reviewer` | obligations the branch touches; `all-rules` - every row of `docs/compliance.md` |
| `all` | requirements, tests and code reviewers | as above |

Pass each agent only its scope; do not paste code into the prompt of `sh-test-reviewer` - it works from the
documents.

## 3. Where findings go

Nothing is fixed in this skill. After the report, with the owner's OK:

- test or code finding not fixed at once - a `PB-n` row in `docs/problems.md` (`docs/process.md` §8; same columns
  as `sh-verify-firmware` "Problems"); an existing open `PB-n` gets no new row;
- requirement finding - a row in `docs/requirements_review.md` (§3); a value or behaviour choice - a `D-n` question;
- a test that disagrees with the code - never a test change: the code is fixed, or a `PB-n` / review row opened
  (§3.2 anti-weakening).

## Report

One short block in plain Romanian for the owner, the house first, few IDs:

- links: errors / untested requirements / hollow tests (counts, the worst three by name);
- requirements: findings by class, the safety ones first;
- tests: findings (hollow, value not from a requirement, weakened), untested requirements;
- code: unspecified behaviour, not as written, safety BLOCKER / RISK;
- evidence of the step: complete, or what is missing (`assess`);
- EU rules: missing requirements, code gaps, technical file (`compliance`);
- the one decision the owner has to make next.

Do not commit or push.
