# Domain Documentation

How engineering skills consume this repository's domain knowledge and architectural decisions.

## Authoritative Domain Sources

Before exploring or generating proposals, skills must read these authoritative documents:

1. **Domain Terms & System Abstractions**:
   - [`docs/requirements_and_features.md`](../requirements_and_features.md) §2 (Terminology, Concepts, Operating Modes)
   - [`docs/architecture.md`](../architecture.md) §1 (Context) and §12 (Abstractions & Physical Topology)
   - [`docs/underfloor_heating_hydraulics_and_failsafes.md`](../underfloor_heating_hydraulics_and_failsafes.md) (Hydraulic rules, interlock, overrun)

2. **Architecture Decisions (ADRs)**:
   - [`.agent/memory-bank/decisions.md`](../../.agent/memory-bank/decisions.md): Append-only ADR log containing all architectural decisions (D1..Dn, QA-1..QA-28).
   - Historical decisions prior to 2026-10-22 are archived in [`docs/archive/decisions_until_2026-10-22.md`](../archive/decisions_until_2026-10-22.md).

## File Structure & Placement

This is a **single-context** repository.
- **Do not create a separate `GLOSSARY.md`**: the official vocabulary is defined in `docs/requirements_and_features.md`.
- **Do not create a separate `docs/adr/` directory**: all architectural decisions are logged in `.agent/memory-bank/decisions.md`.

## Terminology Conventions

- **Use the repo's established terms**:
  - Actuator opening interlock (`interlockDelay`)
  - Pump / last valve overrun (`overrun`)
  - Burner anti-short-cycling guard (`burnerMinRun`, `burnerMinPause`)
  - Diagnostic Trouble Codes (`DTC-xxx`)
  - Zones: Z1 (Living+Kitchen), Z2 (Kids bedroom), Z3 (Master bedroom)
- **Flag ADR and Safety Conflicts**:
  If a proposal conflicts with an existing entry in `.agent/memory-bank/decisions.md` or touches any item on the `AGENTS.md` §3 safety list, surface it explicitly before proceeding.
