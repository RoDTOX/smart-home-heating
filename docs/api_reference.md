# REST API Reference

Base URL: `http://192.168.1.20`. All responses are JSON with `Access-Control-Allow-Origin: *`, except
`/api/config/export` and `/api/config/import`, which send no such header (FR-65.7).
POST parameters go as query string or `application/x-www-form-urlencoded` body, except `POST /api/config/import` (JSON body, FR-65.7).
Zone ids are 1-based unless noted.

## Status & telemetry

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/boiler/status` | Firmware (`firmwareVersion` 1.MM.DD.NN, FR-21), boiler state, time, outdoor temperature, heap, RSSI, uptime |
| GET | `/api/zones` | Per-zone temps, effective target, valve state, `duty` / `learnedDuty` (%), `tempOffset`, alarms, `relayEngaged` (true while the valve is energised), `state` (FR-8.4: `FROST`, `DISABLED`, `FAULT`, `ACQUIRING`, `WINDOW`, `HEATING`, `IDLE`); global schedule state (`away`, `activeLevel`, `nextLevel`, `minsToNextSlot`) and boiler runtime |
| GET | `/api/ble/sensors` | All decoded BLE sensors (temp, humidity, battery, RSSI, age) |
| GET | `/api/history?range=24h\|5d` | Temperature history from LittleFS |
| POST | `/api/history/clear` | Clear history |
| GET | `/api/diag/status` | `checks[]` of FR-10.4 (`id`, `result` OK / FAIL / INFO, `value`, `rule`), `dtcCount` (active), active `dtcs[]` (each with `code` = `DTC-nnn` of requirements §5, `severity`, `message`, `count`, `ageSec`); no health score |
| GET | `/api/diag/selftest` | Subsystem self-test report |
| POST | `/api/diag/clear` | Clear DTCs |

## Comfort control

| Method | Path | Params | Purpose |
|---|---|---|---|
| POST | `/api/zones/setpoint` | `id`, `target` | Temporary boost; expires at the next schedule slot (UI clamps 16–22.5 °C) |
| POST | `/api/zones/hold/clear` | `id` | Back to schedule |
| GET | `/api/schedule` | | Levels per zone, `weekday` / `weekend` slot strings, current/next level |
| POST | `/api/schedule/enable` | `enabled`, `preheat`, `window`, `away` (each `1`/`0`, all optional) | Schedule, pre-heat, open-window guard, global away mode |
| POST | `/api/schedule/level` | `zone`, `level` = `day`\|`night`\|`away`, `temp` (5–30) | Level temperature for a zone |
| POST | `/api/schedule/slots` | `weekend` (`0`/`1`), `slots` = `HH:MM=level,...` (max 8) | Slot list, stored sorted |
| POST | `/api/schedule/reset` | | Factory schedule (22 / 20 / 16 °C, 06:30 day, 22:30 night) |
| POST | `/api/zones/window/clear` | `id` | End open-window pause |
| POST | `/api/zones/window/threshold` | `threshold` (`id` ignored) | One `windowThreshold` for all zones (FR-40); outside 0.8–2.0 °C → 400 |

Invalid schedule input returns `400`.

## Installation & setup

| Method | Path | Params | Purpose |
|---|---|---|---|
| GET/POST | `/api/setup/location` | `coords` = `lat,lon` | Location for Open-Meteo |
| GET | `/api/setup/time` | `refreshWeather` | NTP time and weather |
| GET | `/api/setup/discovered-sensors` | | BLE sensors with assigned zone (0-based) |
| POST | `/api/setup/map-zone` | `zone` (0-based), `name`, `mac`, `sname`, `pin`, `target`, `hyst`, `enabled`, `offset` | Zone commissioning (FR-11.3): all fields checked first, HTTP 400 and nothing changed when a relay is used by another zone or the pump, a zone is switched on without sensor, the name is empty / > 31 bytes / has `"` or `\`, the MAC is malformed, target outside 15–30 °C or offset outside −3..+3 °C |
| POST | `/api/zones/sensor` | | Assign sensor to zone |
| POST | `/api/zones/toggle` | | Enable/disable zone |
| GET/POST | `/api/setup/actuator-type` | `type` = `nc`\|`no` | Actuator polarity |
| GET/POST | `/api/setup/pump` | | GPA pump control (relay 4) |
| GET/POST | `/api/setup/sensor-timeout` | `timeoutSec` | `sensorTimeout` (FR-18); outside 60–3600 s → 400 |
| GET/POST | `/api/config/system` | see below | Safety timings and settings of requirements §6 (FR-42) |
| GET | `/api/config/export` | | Settings file (FR-65.1), see below |
| POST | `/api/config/import` | the settings file | Applies the valid settings, reports the rest (FR-65.3), see below |
| POST | `/api/relay/test` | | Relay test (manual mode) |
| POST | `/api/diag/exercise` | | Anti-seize valve exercise |

## Boiler (manual / service)

| Method | Path | Purpose |
|---|---|---|
| POST | `/api/mode` | `mode` = `auto`\|`manual` |
| POST | `/api/boiler/on` | Start burner; always subject to the anti-short-cycling guard (`429` + deferred start) |
| POST | `/api/boiler/off` | Stop burner; `force=true` stops immediately |
| POST | `/api/boiler/cancel` | Cancel deferred start/stop |
| POST | `/api/boiler/runtime/reset` | Reset burn-time counters |

## Console

| Method | Path | Purpose |
|---|---|---|
| POST | `/api/console/send` | `cmd` = console command (same as Telnet :23), returns new log lines |
| GET | `/api/console/logs` | `since` = sequence number |

OTA firmware upload: `/update` (Basic auth from `include/secrets.h`), or `python tools/sh_ota_upload.py`.

## `/api/config/system`

All values come from `ParamStore` (requirements §6, `src/ParamStore.cpp`). A POST is checked as a whole: if one
field is unknown, read-only or outside its §6 range, the request returns HTTP 400 with the offending `fields` and
**nothing** is changed.

| Field | §6 parameter | Range | POST |
|---|---|---|---|
| `actuatorDelaySec` | `interlockDelay` | 120–600 s | yes |
| `pumpOverrunSec` | `overrun` | 180–600 s | yes |
| `burnerMinRunSec`, `burnerMinPauseSec` | `burnerMinRun`, `burnerMinPause` | 120–600 s | yes |
| `rfSafetyHeartbeatSec` | `hbInterval` | 30–900 s | yes |
| `rfSafetyHeartbeatEnabled` | `boilerLinkHeartbeat` | true / false | yes |
| `freezeProtectionTemp` | `frostTemp` | 8–16 °C | yes |
| `sensorTimeoutSec` | `sensorTimeout` | 60–3600 s | yes |
| `hysteresis` | `hysteresis` | 0.1–1.0 °C | yes |
| `windowThreshold` | `windowThreshold` | 0.8–2.0 °C | yes |
| `openLoopBypass` | `openLoopBypass` | true / false | yes |
| `actuatorNormallyClosed` | `actuatorType` | true = NC | yes |
| `pumpControlEnabled` | `pumpOutput` | true = relay 4 | yes |
| `rfMinCooldownSec` | FR-17.2 (fixed 3 s) | — | read only |
| `pumpRelayPin` | relay 4 (GPIO26) | — | read only |
| `zones[]` | `id`, `name`, `mac`, `relayPin`, `enabled` | — | read only (use `/api/setup/map-zone`) |

Removed in migration step 1: `boilerDelaySec`, `turReturSensors` (stored but never used), `shortCyclingSec`
(replaced by `burnerMinRunSec` / `burnerMinPauseSec`), per-zone `hysteresis` and `windowDropThreshold` (one global
value each; `/api/zones` still reports `windowDropThreshold` per zone, equal to the global value).

## Configuration export / import (FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8)

| Method | Path | Body | Purpose |
|---|---|---|---|
| GET | `/api/config/export` | | The settings file (format 1, below) |
| POST | `/api/config/import` | the settings file, `Content-Type: application/json`, at most 16 384 bytes | Applies each valid setting, reports the rest; answer within 2 s |

**File, format 1.** Credentials, counters and state (burner counters, mode, away, boosts, DTCs, events, history,
sensor readings) are never in it (FR-65.2).

```json
{
  "format": 1,
  "firmwareVersion": "1.11.02.01",
  "params": {"actuatorDelaySec": 180, "pumpOverrunSec": 180, "burnerMinRunSec": 180, "burnerMinPauseSec": 180,
             "rfSafetyHeartbeatSec": 60, "rfSafetyHeartbeatEnabled": true, "freezeProtectionTemp": 12.0,
             "sensorTimeoutSec": 1200, "hysteresis": 0.3, "windowThreshold": 1.2, "openLoopBypass": false,
             "actuatorNormallyClosed": true, "pumpControlEnabled": false},
  "zones": [{"id": 1, "name": "Living", "mac": "A4:C1:38:00:00:01", "sensorName": "", "relayPin": 32,
             "enabled": true, "offset": 0.0, "target": 21.0}],
  "schedule": {"enabled": true, "preheat": true, "window": true,
               "weekday": "06:30=day,22:30=night", "weekend": "08:00=day,23:00=night",
               "levels": [{"id": 1, "day": 23.0, "night": 21.0, "away": 18.0}]},
  "location": "44.5500,26.0667",
  "learnedDuty": [{"id": 1, "duty": 12.5}]
}
```

| Key | Content | Checked like |
|---|---|---|
| `format` | file format version, `1` | missing or other → HTTP 400, nothing changed |
| `firmwareVersion` | firmware that wrote the file (FR-21) | information only, not checked |
| `params.<field>` | every POST field of `/api/config/system`, same name, unit and type (JSON number or `true` / `false`) | the §6 range of that field in `POST /api/config/system` (FR-42), each field alone; `pumpControlEnabled` not on while a zone uses relay 4, checked first against the zones as they are |
| `zones[]` | `id` (1-based), `name`, `mac`, `sensorName`, `relayPin`, `enabled`, `offset` (°C), `target` (°C) | `POST /api/setup/map-zone`, as a whole, together (below) |
| `schedule.enabled`, `.preheat`, `.window` | schedule, pre-heat, window detection on / off | `POST /api/schedule/enable` |
| `schedule.weekday`, `.weekend` | slot list `HH:MM=level,...` | `POST /api/schedule/slots` (FR-31) |
| `schedule.levels[]` | `id` and `day`, `night`, `away` °C per zone | `POST /api/schedule/level` (5–30 °C, FR-29) |
| `location` | `lat,lon`; absent while not set | `POST /api/setup/location` (FR-9.2) |
| `learnedDuty[]` | `id` and learned base `duty` (%) per zone (one value per zone until migration step 7 adds the outdoor bands) | 0–60 % (FR-47) |

**Import rules.**

- A key the firmware does not know (top level, in `params`, `schedule` or a zone) is skipped (`unknown`).
- A setting missing from the file keeps its value (`kept`). `location` is a setting only while the controller has one; without a location it is in no list.
- A value that fails its check, or has the wrong JSON type, is skipped (`invalid`) and the value stays.
- A zone is applied or skipped as a whole: one bad field skips the whole zone; its unknown fields are skipped
  alone; its missing fields keep their value. A zone `id` the controller does not have is skipped.
- Zones together: in the configuration that results (the zones of the file that passed, the other zones as they
  are, the pump as it results from `params`) a relay serves one zone and not the pump; every zone of the file
  in a conflict is skipped, and the check repeats until none is left. A zone whose valve is energised keeps its
  relay and stays on: a file that changes either skips that zone (FR-11.3).
- Answer HTTP 200:

```json
{"status": "ok",
 "applied": ["params.actuatorDelaySec", "zones[1]", "schedule.weekday", "schedule.levels[1].day", "location",
             "learnedDuty[1]"],
 "kept": ["params.hysteresis", "zones[2]"],
 "skipped": [{"name": "params.fooBar", "reason": "unknown"},
             {"name": "zones[3]", "reason": "pump-relay"}]}
```

  Names: `params.<field>`, `zones[<id>]` (a whole zone), `zones[<id>].<field>` (unknown or missing field),
  `schedule.<field>`, `schedule.levels[<id>].<level>`, `location`, `learnedDuty[<id>]`, `<key>` (unknown top-level
  key). Each setting appears in exactly one list.
- A key holding a credential is skipped like an unknown key; an import never changes a credential (FR-65.2).
- Not a JSON object, or `format` missing or not `1` → HTTP 400; body over 16 384 bytes → HTTP 413; `Content-Type`
  other than `application/json` → HTTP 415; nothing changed.
