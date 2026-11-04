# REST API Reference

Base URL: `http://192.168.1.20`. All responses are JSON with `Access-Control-Allow-Origin: *`, except
`/api/config/export` and `/api/config/import`, which send no such header (FR-65.7).
POST parameters go as query string or `application/x-www-form-urlencoded` body, except `POST /api/config/import` (JSON body, FR-65.7).
Zone ids are 1-based unless noted.

## Status & telemetry

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/boiler/status` | Firmware (`firmwareVersion` 1.MM.DD.NN, FR-21), boiler state, time, outdoor temperature, heap, RSSI, uptime; object `gasMeter` (FR-35.1): `lastReadingIndex` (m³) and `lastReadingTime` (epoch) of the last typed reading, `estimatedIndex`, `gasFlowMeasured` (m³/h, null until measured), `readingCount`, `gasTodayM3`, `gasYesterdayM3`, `gasThisMonthM3`, `gasFlameRate`, `boilerSetTemp`, `boilerFlowObserved`, `boilerPowerKw`, `gasPrice`; without a reading the first three are `null` |
| GET | `/api/zones` | Per-zone temps, effective target, valve state, `duty` / `learnedDuty` (%), `tempOffset`, alarms, `relayEngaged` (true while the valve is energised), `frostActive` (frost protection heats the zone, FR-16.1), `state` (FR-8.4: `FROST`, `DISABLED`, `FAULT`, `ACQUIRING`, `WINDOW`, `HEATING`, `IDLE`); global schedule state (`away`, `activeLevel`, `nextLevel`, `minsToNextSlot`) and boiler runtime |
| GET | `/api/ble/sensors` | All decoded BLE sensors (temp, humidity, battery, RSSI, age) |
| GET | `/api/history?range=24h\|5d` | Telemetry history from LittleFS (FR-34): `{"range","stepSec" (300 for 24h, 1800 for 5d),"currentSlot","points":[[slot,z1,z2,z3,outdoor,flags],...],"reboots":[...],"windows":[...]}`. Only recorded steps are listed. `slot` is the ring index: 24h = local time of day in seconds / 300 (the most recent occurrence of that time of day, so data at most 24 h old); 5d = floor(epoch / 1800) mod 240 (data at most 5 days old). `flags`: bit 0 boiler ON (heat requested, no flame signal), bits 2 to 4 valve of zone 1 to 3 open, bit 7 recorded |
| POST | `/api/history/clear` | Clear history |
| GET | `/api/diag/status` | `checks[]` of FR-10.4 (`id`, `result` OK / FAIL / INFO, `value`, `rule`), `dtcCount` (active), active `dtcs[]` (each with `code` = `DTC-nnn` of requirements §5, `severity`, `message`, `count`, `ageSec`); no health score |
| GET | `/api/diag/selftest` | Subsystem self-test report |
| GET | `/api/diag/events` | FR-62: event history kept in LittleFS across reboots: `{"count","events":[{"time","timeValid","type" (`dtc-raised`, `dtc-cleared`, `restart`),"code" (`DTC-nnn` of requirements §5 or empty),"zone","value"}]}`, newest first, at most 50 |
| GET | `/api/diag/log` | FR-62.1: event log of safety and I/O, newest first, `?n=` (default 200, max 4096): `{"count","stored","queued","events":[{"time","timeValid","type","subject","zone","reason","old","new","boiler","overrun","pending"}]}`; a `dtc` entry has `code` |
| POST | `/api/web/update` | FR-8.6: multipart file = the gzip page; atomic replace of `/index.html.gz`, no reboot: 200 `{"status":"ok","bytes","fsFree"}`, 400 not gzip, 413 over 61 440 B, 507 free LittleFS under 24 576 B, 401 without credentials in the release profile |
| POST | `/api/web/remove` | FR-8.6: deletes `/index.html.gz` (the stub answers on `/`); same authentication |
| GET | `/api/diag/mem` | FR-10.6, read only: `{"free","minFreeEver","largestBlock","allocated","allocBlocks","freeBlocks","internalFree","internalMin","internalLargest","heapSize","freeIram32","uptime","fsTotal","fsUsed","tasks":[{"name","stackFreeMin"}]}` (bytes); tasks `loopTask`, `wifi`, `tiT`, `btController`, `nimble_host`, `wx` when they exist |
| POST | `/api/diag/clear` | Clear DTCs |

## Comfort control

| Method | Path | Params | Purpose |
|---|---|---|---|
| POST | `/api/zones/setpoint` | `id`, `target` | Temporary boost; expires at the next schedule slot (UI clamps 16–22.5 °C) |
| POST | `/api/zones/hold/clear` | `id` | Back to schedule |
| GET | `/api/schedule` | | Levels per zone, `weekday` / `weekend` slot strings, current/next level |
| POST | `/api/schedule/enable` | `enabled`, `preheat`, `window`, `away` (each `1`/`0`, all optional) | Schedule, pre-heat, open-window guard, global away mode |
| POST | `/api/schedule/level` | `zone`, `level` = `day`\|`night`\|`away`, `temp` (5–28, C-36; above 28 → HTTP 400) | Level temperature for a zone |
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
| POST | `/api/setup/zone-add` | `name` (optional; invalid → 400) | Adds a zone (FR-73), on the lowest free relay output in the order relay 1…4 = GPIO 32, 33, 25, 26: `{"status":"ok","zone":N,"relayPin":P}` (N = its number, 1-based, P = the free relay output it got); HTTP 409 when no relay output is free, nothing changed |
| POST | `/api/setup/zone-delete` | `zone` (1-based number) | Deletes the zone (FR-74); later zones move up one number; HTTP 409 while its valve is energised, 400 for a number that does not exist, nothing changed |
| POST | `/api/setup/sensor-search` | `action` = `start`\|`stop` | Starts or stops the sensor search (FR-11.5); answers like the GET below; another `action` → 400 |
| GET | `/api/setup/discovered-sensors` | | Sensor search result (FR-11.5): `active`, `remainingSec` (0 when not active, at most 600), `sensors[]` (`mac`, `name`, `temp`, `hum`, `bat`, `rssi`, `ageSec`, `zone` = number of the zone it is assigned to or 0). Empty outside a search except the sensors assigned to zones |
| POST | `/api/setup/map-zone` | `zone` (0-based), `name`, `mac`, `sname`, `pin`, `target`, `hyst`, `enabled`, `offset` | Zone commissioning (FR-11.3): all fields checked first, HTTP 400 and nothing changed when `pin` is not a relay output (32, 33, 25, 26), a relay is used by another zone or the pump, a sensor MAC is used by another zone (one zone alone; the page saves all zones together through `/api/config/import`, which checks the result), a zone is switched on without sensor, the name is empty / > 31 bytes / has `"` or `\`, the MAC is malformed, target outside 15–30 °C or offset outside −3..+3 °C |
| POST | `/api/zones/sensor` | | Assign sensor to zone |
| POST | `/api/zones/toggle` | | Enable/disable zone |
| GET/POST | `/api/setup/actuator-type` | `type` = `nc`\|`no` | Actuator polarity |
| GET/POST | `/api/setup/pump` | | GPA pump control (relay 4) |
| GET/POST | `/api/setup/sensor-timeout` | `timeoutSec` | `sensorTimeout` (FR-18); outside 60–3600 s → 400 |
| GET/POST | `/api/config/system` | see below | Safety timings and settings of requirements §6 (FR-42) |
| GET | `/api/config/export` | | Settings file (FR-65.1), see below |
| POST | `/api/config/import` | the settings file | Applies the valid settings, reports the rest (FR-65.3), see below |
| POST | `/api/relay/test` | `pin`, `state` | Relay test (FR-43): `pin` = GPIO of a relay output (32, 33, 25, 26), `state` = `1` opens the valve of that output, `0` closes it, respecting NC / NO (FR-15.1); HTTP 409 unless the mode is MANUAL and the output belongs to a zone or is the pump; 400 for a pin that is no relay output |
| POST | `/api/diag/exercise` | | Anti-seize valve exercise |

## Boiler (manual / service)

| Method | Path | Purpose |
|---|---|---|
| POST | `/api/mode` | `mode` = `auto`\|`manual` |
| POST | `/api/boiler/on` | Start burner; always subject to the anti-short-cycling guard (`429` + deferred start) |
| POST | `/api/boiler/off` | Stop burner; `force=true` stops immediately |
| POST | `/api/boiler/cancel` | Cancel deferred start/stop |
| POST | `/api/boiler/runtime/reset` | Reset burn-time counters |
| POST | `/api/boiler/gas-meter` | FR-35.1: the user types a meter reading. Form fields `index` (m³, 0 - 999999, 3 decimals, required), `time` (epoch seconds of the reading, optional, default now; not in the future), `dryRun=1` (test mode: validates and answers, stores nothing, writes no entry). `200 {"status":"ok","stored":true/false,"reading":{"time":epoch,"index":m3},"gasMeter":{...}}`; a missing or out-of-range `index`, or a `time` in the future: `400 {"error":...}`, nothing stored. Only this call writes a reading entry. |
| GET | `/api/boiler/gas-readings` | FR-35.1: all stored readings, oldest first: `{"count":n,"replaced":n,"readings":[{"time":epoch,"index":m3,"runtimeSec":s or null},...]}` |

## Console

| Method | Path | Purpose |
|---|---|---|
| — | login (FR-75.1) | `POST /api/console/send` needs HTTP Basic authentication, user `console`; `401` + `WWW-Authenticate` without it, `403 {"error":"password change required"}` while the first password still has to be changed, `429` while locked out (FR-75.4). Without a password set only `cmd=HELP` and `cmd=STATUS` are accepted, anything else `401`. `GET /api/console/logs` and `GET /api/security/events` are public on the home network, on the setup access point they need the login |
| GET | `/api/security/events` | FR-77: `{"count":n,"events":[{"time":epoch,"type":"login-fail\|login-ok\|lockout\|command\|password-change\|factory-reset","source":"http\|telnet\|serial","client":"a.b.c.x"}]}` oldest first, at most 50 |
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
| `zonesCount` | number of zones (§6), 0-32 | — | read only (use `/api/setup/zone-add`, `/api/setup/zone-delete`) |
| `zones[]` | `id`, `name`, `mac`, `relayPin`, `enabled`: one entry per existing zone | — | read only (use `/api/setup/map-zone`) |

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
             "enabled": true, "offset": 0.0}],
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
| `zones[]` | `id` (1-based), `name`, `mac`, `sensorName`, `relayPin`, `enabled`, `offset` (°C); the current target is not in the file (the schedule sets it, the setpoints are in `schedule.levels`) | `POST /api/setup/map-zone`, as a whole, together (below) |
| `schedule.enabled`, `.preheat`, `.window` | schedule, pre-heat, window detection on / off | `POST /api/schedule/enable` |
| `schedule.weekday`, `.weekend` | slot list `HH:MM=level,...` | `POST /api/schedule/slots` (FR-31) |
| `schedule.levels[]` | `id` and `day`, `night`, `away` °C per zone | `POST /api/schedule/level` (5–28 °C, FR-29, C-36) |
| `location` | `lat,lon`; absent while not set | `POST /api/setup/location` (FR-9.2) |
| `learnedDuty[]` | `id` and learned base `duty` (%) per zone (one value per zone until migration step 7 adds the outdoor bands) | 0–60 % (FR-47) |

**Import rules.**

- A key the firmware does not know (top level, in `params`, `schedule` or a zone) is skipped (`unknown`).
- A setting missing from the file keeps its value (`kept`). `location` is a setting only while the controller has one; without a location it is in no list.
- A value that fails its check, or has the wrong JSON type, is skipped (`invalid`) and the value stays.
- A zone is applied or skipped as a whole: one bad field skips the whole zone; its unknown fields are skipped
  alone; its missing fields keep their value. A zone `id` the controller does not have is skipped.
- Zones together: in the configuration that results (the zones of the file that passed, the other zones as they
  are, the pump as it results from `params`) a relay serves one zone and not the pump, and a sensor MAC serves one
  zone (so two zones can exchange their sensors in one file); every zone of the file in a conflict is skipped
  (`relay-taken`, `pump-relay`, `sensor-taken`), and the check repeats until none is left. A zone whose valve is energised keeps its
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
