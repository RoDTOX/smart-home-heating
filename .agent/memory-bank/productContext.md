# Product Context

Rewritten 01.11.2026 (process review QA-25). Behaviour and values are defined in
[`docs/requirements_and_features.md`](../../docs/requirements_and_features.md) (baseline v2); this file only says why.

## Problem

One thermostat for a whole home leaves rooms too hot or too cold and wastes energy. In the reference home the
original wireless room thermostat broke, so the boiler had to be switched by hand. Commercial multi-zone kits cost
hundreds to thousands of euros.

## Users

- **Family:** sees rooms, temperatures and alarms, sets a boost or Home / Away on one page (FR-8.1); otherwise has
  nothing to do.
- **Owner / installer:** commissions once (FR-68), then maintains through the `#admin` view, the API and the console
  (`tools/sh_esp_cli.py`).

## How it behaves

1. Each zone follows its schedule level (day / night / away, FR-29, FR-31); a warmer level starts early depending on
   the outdoor temperature (FR-48).
2. A zone's valve opens for part of each hour (time-proportional control, FR-47).
3. The boiler is switched ON only after a valve has been open long enough for the wax actuator (FR-2); after OFF one
   valve stays open for the overrun (FR-3); the burner guard limits starts and stops (FR-4).
4. Faults are not felt in the rooms: a zone that loses its sensor keeps heating at its learned duty (FR-51), missing
   weather or time data have fallbacks (NFR-5.1, NFR-5.2), alarms show in the page (FR-62).

## UX goals

- **Local only:** no cloud, no app store; the page is served by the controller (FR-8.1, NFR-1.1).
- **One page:** family view plus `#admin` (FR-8.1); family view in Romanian or English (NFR-3.1).
- **Zero maintenance:** the controller recovers from power and network loss by itself (NFR-1.1, NFR-1.4); alarms are
  page-only, there is no push channel (FR-62).
