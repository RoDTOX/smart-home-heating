# Tech Context

Rewritten 01.11.2026 (process review QA-25). Limits and values are cited, not copied: parameters in requirements §6
([`docs/requirements_and_features.md`](../../docs/requirements_and_features.md)), resources in NFR-2.2, NFR-2.6,
NFR-2.8, NFR-2.9, platform and budget in [`docs/architecture.md`](../../docs/architecture.md) §1 and §14.

## Stack

- **Hardware:** ESP32-WROOM-32E (4 MB flash) on ESP32-Relay X4 V1.1 (`192.168.1.20`); pins and wiring in
  [`docs/hardware_esp32_relay.md`](../../docs/hardware_esp32_relay.md).
- **Framework:** Arduino-ESP32 2.0.14 (`espressif32@6.5.0`, board `esp32dev`) on FreeRTOS; partitions
  `min_spiffs.csv` (NVS, two OTA app slots, LittleFS — sizes in `architecture.md` §1).
- **Libraries:** vendored in `lib/`: `ArduinoJson` 7.0.4 and `NimBLE-Arduino` 3.0.0-dev (observer role only, see
  `platformio.ini` build flags). Core libraries (WiFi, WebServer, HTTPClient, Preferences, LittleFS, Update, Wire) may
  be used; any other library needs the owner's approval (AGENTS.md §3).
- **Web page:** one page, `web/index.html`, gzipped into `include/dashboard_html_gz.h` by `tools/sh_build_web.py`
  (PlatformIO pre-script). `include/console_html_gz.h` and `include/setup_html_gz.h` are dead files
  (`architecture.md` §17, G31).
- **Build identity:** `tools/sh_git_version.py` (PlatformIO pre-script) writes `include/git_version.h` with the
  commit, `-dirty` when firmware inputs have uncommitted changes (`docs/process.md` §6).
- **Host tools:** `tools/sh_esp_cli.py` (CLI), `tools/sh_ota_upload.py` (OTA upload, needs `include/secrets.h`),
  `tools/sh_test_system.py` (system tests against the board), `tools/sh_process_check.py` (trace, reqs, process,
  board and release checks), `tools/sh_dev_server.py` (local page development, tooling only).
- **CI:** `.github/workflows/build.yml` runs `pio run` and `sh_process_check.py` on every push; compile check only,
  the CI binary is never flashed.

## Commands

```bash
pio run                                  # build; also regenerates the page header and the build identity
python tools/sh_ota_upload.py            # flash over Wi-Fi
python tools/sh_test_system.py           # system tests against the board
python tools/sh_process_check.py         # trace, reqs and process checks
python tools/sh_process_check.py board   # live board values against the safety limits
```

The order before a flash is AGENTS.md §4 and the `sh-verify-firmware` skill.

## Constraints

- Firmware image size: NFR-2.8; flash margin: AR-18 (`architecture.md` §14.1).
- Free heap and heap drift: NFR-2.2, NFR-2.6 (`architecture.md` §14.2).
- NVS written only when a value changes: NFR-2.9; only `ParamStore` touches NVS in the target design (AR-5).
- No blocking call in `loop()` except one radio frame (FR-17.6); task watchdog NFR-1.5.
- Safety timings and limits (interlock, overrun, burner guard, frost, sensor timeout): names and ranges in
  requirements §6; the safety layer is `architecture.md` §6.
