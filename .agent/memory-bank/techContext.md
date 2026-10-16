# Tech Context: Smart Heating System

## Stack
- **Target Hardware:** ESP32-WROOM-32E (240MHz, 4MB Flash) on ESP32-Relay X4 V1.1 board (`192.168.1.20`).
- **Core Framework:** Arduino framework on PlatformIO (`platform = espressif32`, board `esp32dev`).
- **Bluetooth Library:** `NimBLE-Arduino @ 3.0.0-dev` (lightweight, non-blocking passive scanning).
- **Networking & Web Server:** Native ESP32 `<WebServer.h>`, `<HTTPUpdateServer.h>`, `ArduinoJson @ 7.0.4`.
- **Web Storage & Assets:** Gzipped PROGMEM C++ headers (`include/dashboard_html_gz.h`, `include/console_html_gz.h`, `include/setup_html_gz.h`) generated automatically by `tools/build_web.py` (<40KB total footprint).
- **Frontend Web:** HTML5, CSS3 (high-density dark theme, responsive 16:9 + 720p), Vanilla JavaScript, interactive inline SVG floorplan. Zero external CDN dependencies.
- **Upstream Host Bridge:** Samsung Galaxy A6 ("Cinderella", Android 9, Termux/Debian Proot) running Mosquitto MQTT (:1883), PostgreSQL, and Grafana (:3000).
- **Local Dev Server & Testing:** `tools/dev_server.py` (reverse proxy on `:8080`), `tools/ota_upload.py` (direct socket OTA streaming), `tools/test_system.py` (ASPICE SWE.5 68 atomic tests, 100% pass rate).

## Build Commands
```bash
# Build firmware
pio run

# Generate/refresh gzipped web headers
python tools/build_web.py

# Flash firmware over OTA (Wi-Fi)
python tools/ota_upload.py

# Run ASPICE SWE.5 full test suite
python tools/test_system.py

# Start local reverse-proxy dev server
python tools/dev_server.py
```

## Dependencies (PlatformIO `platformio.ini`)
- `Preferences @ 2.0.0`
- `WiFi @ 2.0.0`
- `ArduinoJson @ 7.0.4`
- `HTTPClient @ 2.0.0`
- `HTTPUpdateServer @ 2.0.0`
- `WebServer @ 2.0.0`
- `NimBLE-Arduino @ 3.0.0-dev`

## Constraints
- **Flash / RAM limits:** OTA partition limit < 1.25 MB (currently ~1.22 MB / 62.4%); Free RAM budget > 110 KB (currently 19.9% used).
- **230V Mains Safety:** Isolated relays for electrothermal actuators; 120s interlock opening delay; 180s pump overrun heat dissipation.
- **Anti-Short-Cycling Guard:** Minimum 60s flame burn time, minimum 60s cooldown pause guard.
- **Timing Deadlines:** Non-blocking asynchronous design with hardware TWDT (90s limit).

