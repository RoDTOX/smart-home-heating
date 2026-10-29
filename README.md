# Smart Home Heating

ESP32 firmware for autonomous multi-zone underfloor heating — **install once, calibrate once, forget it**.

```
Xiaomi BLE sensors (PVVX / BTHome) ──► ESP32-Relay X4 ──► relays ──► 230 V NC actuators (zones)
Open-Meteo (outdoor temp) ─────────►       │
                                           └──► 433.92 MHz OOK ──► boiler receiver (ON/OFF)
```

## How it controls

- **Levels:** each zone has a *day*, *night* and *away* temperature. One weekday and one weekend slot list
  (e.g. `06:30=day,22:30=night`) applies to all zones. A global **Away** button overrides the schedule.
- **TPI valve control:** every 60 min a zone's valve opens for a fraction of the hour
  (error / 1.5 °C + a slowly learned base duty, min 10 min). No ON/OFF overshoot on slow screed.
- **Weather-shifted pre-heat:** the next warmer level starts early — 2 h at 5 °C outside, 3.5 h at −5 °C.
- **Boost:** +/− on a room is temporary and expires at the next slot.
- **Safety (always on):** actuator opening interlock before the boiler fires, anti-short-cycling burner guard
  (no forced start), pump overrun, frost guard (<12 °C), sensor timeout → zone closed + `DTC-101`,
  open-window pause, weekly anti-seize exercise.

## Build & flash

Requires `include/secrets.h` with your Wi-Fi and OTA credentials (not in git):

```c
#define DEFAULT_WIFI_SSID "..."
#define DEFAULT_WIFI_PASS "..."
#define OTA_USER "admin"
#define OTA_PASS "..."
```

```bash
pio run
```

```bash
python tools/ota_upload.py
```

`ota_upload.py` refuses to flash without `secrets.h` (a build without it would drop the board off Wi-Fi).
GitHub Actions builds every push as a compile check only — do not flash the CI binary.

After flashing, verify against the board:

```bash
python tools/test_system.py
```

## Use

- **Web UI:** `http://192.168.1.20/` — room cards, boost, Away. `#admin` for levels, slots, sensor mapping,
  calibration offsets, TPI duty, system info.
- **Console:** Telnet `192.168.1.20:23` or `python tools/esp_cli.py exec "<cmd>"`. `HELP` lists commands, e.g.
  `SCHEDULE`, `SCHEDULE LEVEL 2 night 19`, `SCHEDULE SLOTS weekday 06:30=day,08:30=away,17:00=day,22:30=night`,
  `AWAY ON`, `OFFSET 1 -0.3`, `LOCATION 45.65,25.60`, `STATUS`.

## Docs

- [Plan](docs/plan.md) · [Changelog](docs/changelog.md) · [REST API](docs/api_reference.md)
- [Hardware & pinout](docs/hardware_esp32_relay.md) · [Hydraulics & failsafes](docs/underfloor_heating_hydraulics_and_failsafes.md) · [BOM](docs/bom_hardware_costs.md)
- [Agent instructions](AGENTS.md)
