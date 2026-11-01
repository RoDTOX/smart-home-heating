# System Patterns

This file is a pointer. The design is [`docs/architecture.md`](../../docs/architecture.md) (SWE.2, approved by the
owner 01.11.2026); behaviour, parameter defaults and ranges are in
[`docs/requirements_and_features.md`](../../docs/requirements_and_features.md) (baseline v2, values only in §6).

Rewritten 01.11.2026 (process review QA-25). Earlier versions of this file described a system that no longer exists;
the decisions behind them are listed in [`decisions.md`](decisions.md), entry "Superseded decisions".

| Topic | Where |
|---|---|
| Context, constraints, principles P1…P9 | `architecture.md` §1, §2 |
| Modules, layers, dependency rules R-A1…R-A7 | `architecture.md` §3 |
| Interfaces, user commands | `architecture.md` §4 |
| Tasks, control tick, timing budget, boot sequence | `architecture.md` §5 |
| Safety layer (`SafetySupervisor`), invariants I-1…I-8 | `architecture.md` §6 |
| State machines: zone, valve, boiler, boiler link, mode, season, boot / OTA | `architecture.md` §7 |
| Validity `Ok` / `Held` / `Lost` and the fallback of every input | `architecture.md` §8 |
| Parameter store, NVS budget | `architecture.md` §9; values in requirements §6 |
| DTC and event store | `architecture.md` §10; codes in requirements §5 |
| Fault injection | `architecture.md` §11 |
| Resource budget: flash, RAM, timing, flash wear | `architecture.md` §14 |
| Today's code vs the target, migration steps | `architecture.md` §17, §18; status in [`docs/plan.md`](../../docs/plan.md) |
| Process rules: requirements, tests, reviews, decisions, commits, problems | [`docs/process.md`](../../docs/process.md) |

The rules on test integrity (anti-weakening) and on gaps between code, requirements and tests (zero-gap audit) are in
`docs/process.md` §3.2 (moved there, process review QA-25).
