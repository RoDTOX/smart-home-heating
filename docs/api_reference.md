# REST API Reference

Base URL: `http://192.168.1.20`. All responses are JSON with `Access-Control-Allow-Origin: *`.
POST parameters go as query string or `application/x-www-form-urlencoded` body.
Zone ids are 1-based unless noted.

## Status & telemetry

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/boiler/status` | Firmware, boiler state, time, outdoor temperature, heap, RSSI, uptime |
| GET | `/api/zones` | Per-zone temps, effective target, valve state, `duty` / `learnedDuty` (%), `tempOffset`, alarms; global schedule state (`away`, `activeLevel`, `minsToNextSlot`) and boiler runtime |
| GET | `/api/ble/sensors` | All decoded BLE sensors (temp, humidity, battery, RSSI, age) |
| GET | `/api/history?range=24h\|5d` | Temperature history from LittleFS |
| POST | `/api/history/clear` | Clear history |
| GET | `/api/diag/status` | Health score and active DTCs |
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
| POST | `/api/setup/map-zone` | `zone` (0-based), `name`, `mac`, `sname`, `pin`, `target`, `hyst`, `enabled`, `offset` | Zone mapping and sensor calibration offset (−3..+3 °C) |
| POST | `/api/zones/sensor` | | Assign sensor to zone |
| POST | `/api/zones/toggle` | | Enable/disable zone |
| GET/POST | `/api/setup/actuator-type` | `type` = `nc`\|`no` | Actuator polarity |
| GET/POST | `/api/setup/pump` | | GPA pump control (relay 4) |
| GET/POST | `/api/setup/sensor-timeout` | `timeoutSec` | `sensorTimeout` (FR-18); outside 60–3600 s → 400 |
| GET/POST | `/api/config/system` | see below | Safety timings and settings of requirements §6 (FR-42) |
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
