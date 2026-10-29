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
| POST | `/api/zones/window/threshold` | `id`, `threshold` | Open-window drop threshold |

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
| GET/POST | `/api/setup/sensor-timeout` | | Sensor offline failsafe timeout |
| GET/POST | `/api/config/system` | | Hydraulic timings and wiring |
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

OTA firmware upload: `/update` (Basic auth from `include/secrets.h`), or `python tools/ota_upload.py`.
