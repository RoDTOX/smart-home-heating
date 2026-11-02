# AGENTS.md

> Agent instructions for the `smart-heating` repository. Format: <https://agents.md>.

## 1. Mission

Firmware for autonomous multi-zone underfloor heating — **install once, calibrate once, forget it**.

- **Hardware:** ESP32-WROOM-32E on ESP32-Relay X4 V1.1 (`192.168.1.20`).
- **Inputs:** Xiaomi BLE room sensors (NimBLE passive scan), Open-Meteo outdoor temperature.
- **Outputs:** 230V NC thermal actuators (relays), optional GPA pump (relay 4, unused in reference install), gas boiler ON/OFF via 433.92 MHz OOK.
- **Interface:** headless REST/JSON API + CLI ([`tools/sh_esp_cli.py`](tools/sh_esp_cli.py)). UI is secondary.

Reference install: Immergas Victrix Tera 24, 7 circuits → 3 zones (Z1 living+kitchen, Z2 kids bedroom, Z3 master bedroom).
Zones, circuits and boiler interface must stay **configuration**, not hard-coded, so other installs work too.

## 2. Current plan: the architecture migration

Done: dead docs, tools and integrations removed; firmware made lean; one UI page.

The plan is the migration of [`docs/architecture.md`](docs/architecture.md) §18 (steps 1–14), with its status in
[`docs/plan.md`](docs/plan.md). **One step at a time** (AR-19), on a branch: code and its tests change in the same
step, the step passes §4, the owner tests it hands-on on the board, then the next step starts.
Requirement phase tags say in which step a requirement is built: `[now]` = steps 1–13, `[phase 4]` = step 14,
`[later]` and `[nice to have]` after step 14.

**Do not add features outside the current step.** No cloud, SaaS, fleet, shop, Home Assistant or kiosk work.
The device is prepared for sale in the EU under the owner's brand (owner, 02.11.2026): what EU law asks of the
software is in [`docs/compliance.md`](docs/compliance.md) and becomes a requirement only through the chain (§5).

## 3. Engineering rules

- **Smallest change that works.** Reuse existing modules before writing new ones.
- **No new libraries** without user confirmation. Vendored: `ArduinoJson`, `NimBLE-Arduino`.
- **Never cut safety corners** (defaults and ranges only in requirements §6; cite the ID and the parameter,
  never a number):
  - actuator opening interlock `interlockDelay` before boiler ON (FR-2)
  - overrun: the last open valve stays open while the boiler is ON or `PENDING_OFF`, and for `overrun` after
    a lit flame (FR-3)
  - burner anti-short-cycling `burnerMinRun` / `burnerMinPause` on every ON/OFF source (FR-4)
  - frost protection below `frostTemp`, every zone, every mode (FR-16.1)
  - sensor loss: after `sensorTimeout` the zone keeps heating at its fallback duty and raises DTC-101 (FR-51)
  - safe boot: all valves closed, one boiler OFF, warm-up before any heating command (NFR-1.4)
- **Resources:** NVS writes only on value change, no blocking calls that starve `loop()`,
  bounded buffers, no heap growth, firmware < 1.25 MB (OTA).
- **Tool naming:** every new project tool or script (in `tools/`, agents, skills, MCP servers) starts with
  `sh_` (smart home), e.g. `tools/sh_process_check.py`, so it is never confused with external MCPs or
  temporary scripts. Agents and skills use `sh-` (their names allow only hyphens), e.g. `sh-verify-firmware`.
  Temporary scripts go to the scratchpad, not the repo.

## 4. Verification before flashing

1. `pio run` builds clean (also runs on GitHub Actions for every push).
2. `python tools/sh_process_check.py` has no ERROR (`KNOWN` lines are errors already tracked by an open `PB-n`).
3. Test review: `sh-test-reviewer` (no access to the code) finds no FINDING - every changed test cites a leaf ID
   and asserts the value written in the requirement ([`docs/process.md`](docs/process.md) §3.2). The step's
   tests were written first, by `sh-test-writer`, and failed on the old firmware (§3.1).
4. `sh-code-reviewer` and `sh-safety-reviewer` report no FINDING / BLOCKER when `src/`, `include/`, `web/` or
   `platformio.ini` changed.
5. `python tools/sh_test_system.py` passes against the board.
6. `python tools/sh_process_check.py board` has no ERROR (live NVS values within the safety limits).
7. Read your own `git diff` for debug leftovers and the safety list above.

The `sh-verify-firmware` skill runs these steps. The firmware session may flash once steps 1–4 pass; only one
firmware session runs at a time (owner, 01.11.2026). A finding not fixed at once becomes a `PB-n` row in
[`docs/problems.md`](docs/problems.md).

The full process (requirement rules, requirements review, test review, decisions, commit/push gate) is in
[`docs/process.md`](docs/process.md).

## 5. Workflow

- Explain the *why* briefly (analogy welcome), keep tasks atomic.
- Ask when intent is unclear (>10% ambiguity) instead of guessing.
- On task completion update [`docs/changelog.md`](docs/changelog.md) and `.agent/memory-bank/activeContext.md`.
- **The owner's chain.** When the owner tells what they see on the heating or asks for a change (or for the next
  migration step), run skill `sh-change`: log → requirement → tests first → code → gates → commit → board.
  The owner is asked only to decide a requirement change and to test on the board; everything else runs without
  asking. At the end of a migration step `sh-assessor` checks the step's evidence and `sh-compliance-reviewer`
  the EU rules, before the owner tests.
- **Commit** on the work branch inside `sh-change` once its gates pass (owner, 02.11.2026); outside the chain only
  on request. **Push, PR and `main` only on explicit request.** Deletions need confirmation.
- Logs (`docs/changelog.md`, `docs/problems.md`, `docs/acceptance.md`, `.agent/memory-bank/decisions.md`) are
  append-only: add an entry, never rewrite an old one. Old entries may move unchanged to `docs/archive/` (owner,
  02.11.2026). `activeContext.md` holds only what holds now and what comes next.
- **Model per work** (owner, 02.11.2026: tokens). The strongest model only where a mistake can burn the boiler or
  the floor; a session started from the main chat gets the model of its work, an agent carries its own in its
  frontmatter. Read by section (`Grep`, then `Read` with `offset` / `limit`), never whole documents.

  | Work | Model, effort |
  |---|---|
  | Main chat: talk with the owner, log, start sessions, report | Sonnet, medium |
  | Code that touches the safety list (§3), a safety decision, design across modules | Opus, high |
  | Other code, tests, requirements, architecture, process documents | Sonnet, medium |
  | Lookups, board check, git, records (`sh-assessor`) | Haiku, low |

## 6. Language

- Code, identifiers, API, logs, docs: **English**.
- Conversation with the user: **Romanian**, concise.
