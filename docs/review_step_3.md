# Step 3 review - zones and sensors set by the user (changes C-10, C-11), first half of migration step 3

Status 02.11.2026: first half built and on the board; the `OutputBank` module (R-A1) is **not** built yet. Owner test pending.

## Evidence

| Item | Result |
|---|---|
| Requirements | C-10, C-11 (baseline v2.8), reviewed by `sh-req-reviewer`, fixes applied (commits 8f5a487, 65857fa, 23ca03d) |
| Tests first | TC30.1 ... TC30.11 and changed TC27.3, written by `sh-test-writer`, reviewed by `sh-test-reviewer`, frozen in 19749b2; red run on ce8a58b: 11 of 12 failed, TC30.7 skipped (no zone has a sensor yet) |
| Code | caa18c4, e87f3df, bcb8451; reviews by `sh-code-reviewer` and `sh-safety-reviewer` (11 findings, 1 blocker: stale BLE slot, fixed) |
| Board | system test 123 pass, 0 fail, 3 skip (TC29.15, TC29.33 no learned duty above 0 %; TC30.7 no zone has a sensor); free heap 102 KB (NFR-2.2) |
| Problems opened | PB-57 ... PB-65 (risks left open), PB-62 (zone target in the settings file) |
| `sh-assessor`, `sh-compliance-reviewer` | **not run** (owner asked for economy, 02.11.2026); to run before the step is closed |
| Owner acceptance | pending |

## Open

- `OutputBank` module and R-A1 (relay pins only there): second half of step 3.
- Heap margin 2 KB over NFR-2.2; savings listed in architecture §14.2.
- Import does not create zones (until then `no-zone`).
