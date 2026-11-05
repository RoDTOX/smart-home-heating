# Triage Labels & Status Vocabulary

This repository maps the five canonical triage roles used by agent skills directly to the native status column in [`docs/problems.md`](../problems.md).

## Role Mapping

| Skill Canonical Role | In-Repo Problem Status | Meaning in this Repository |
|----------------------|------------------------|----------------------------|
| `needs-triage`       | `open`                 | Newly reported problem; needs investigation and requirement mapping |
| `needs-info`         | `analysing`            | Root cause or board behavior is being investigated |
| `ready-for-agent`    | `open`                 | Problem cause and requirements are clear; ready for automated test and fix |
| `ready-for-human`    | `open`                 | Requires owner hands-on testing on the ESP32 board or physical plant |
| `wontfix`            | `rejected`             | Accepted as-is or determined not to be a defect (cause documented in row) |

## Resolution States

In addition to triage roles, [`docs/problems.md`](../problems.md) uses two terminal resolution states:

- **`fixed`**: Fix committed to the branch, citing the `PB-n` ID in the commit message (e.g. `fix(control): keep valve open during overrun (PB-2)`).
- **`closed`**: Fix verified and confirmed running on the physical hardware (`192.168.1.20`).
