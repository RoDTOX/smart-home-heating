# Issue Tracker: In-Repo Markdown Work Products

This repository uses a lightweight in-repo ASPICE-style tracking system. There is no external tracker (GitHub Issues, GitLab, Jira) and no `.scratch/` directory.

All issues, defect reports, and tasks are tracked directly in the `docs/` work products.

## Authoritative Files

1. **Defects & Operational Problems**: [`docs/problems.md`](../problems.md)
   - ID format: `PB-n` (sequential, never reused).
   - Columns: `ID | Date | Symptom | Requirements | Board commit | Safety | Cause | Missed by | Checker | Fix | Status`
   - Status lifecycle: `open` → `analysing` → `fixed` (commit) → `closed` (verified on board), or `rejected`.
   - An open safety problem (`Safety: yes`) blocks a release.

2. **Feature Tasks & Migration Steps**: [`docs/plan.md`](../plan.md)
   - Tracks the 14 architecture migration steps (`Step 1` through `Step 14`).
   - Active task execution is mirrored in [`.agent/plan.md`](../../.agent/plan.md).

3. **Decisions Log**: [`.agent/memory-bank/decisions.md`](../../.agent/memory-bank/decisions.md)
   - Append-only ADR log for architectural choices.

## Protocol for Skills

When an engineering skill (such as `/to-spec`, `/to-tickets`, `/wayfinder`, `/triage`, or `/implement-spec`) operates on this repo:

- **When a skill says "publish to the issue tracker"**:
  - If it is a bug, defect, or safety observation: append a new row `PB-n` to [`docs/problems.md`](../problems.md).
  - If it is a feature task or tracer-bullet step: link it to the active migration step in [`docs/plan.md`](../plan.md) and record execution checkpoints in [`.agent/plan.md`](../../.agent/plan.md).
  - **Do not create `.scratch/` directories or individual ticket files outside `docs/`**.

- **When a skill says "fetch the relevant ticket"**:
  - Read the corresponding `PB-n` entry from [`docs/problems.md`](../problems.md) or the relevant step from [`docs/plan.md`](../plan.md).

- **Wayfinding operations (`/wayfinder`)**:
  - Record the decision map in [`.agent/memory-bank/decisions.md`](../../.agent/memory-bank/decisions.md) and update [`docs/plan.md`](../plan.md) rather than creating separate map files.
