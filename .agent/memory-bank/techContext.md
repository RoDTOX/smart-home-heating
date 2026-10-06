# Tech Context: Smart Heating System

## Stack
- **Target Hardware:** ESP32 (e.g. ESP32-WROOM-32 or ESP32-based 8-relay board).
- **Core Framework:** Arduino framework on PlatformIO (`platform = espressif32`).
- **Bluetooth Library:** `h2zero/NimBLE-Arduino` (lightweight, non-blocking passive scanning).
- **Networking & Async Web:** `ESPAsyncWebServer`, `AsyncTCP`, `PubSubClient` (or `AsyncMqttClient`).
- **Filesystem:** LittleFS for static Web GUI storage and `/config.json` persistence.
- **Frontend Web:** HTML5, CSS3 (vanilla, responsive dark theme), Vanilla JavaScript, interactive inline SVG floorplan. Zero external CDN dependencies.
- **Upstream Host Bridge:** Samsung Galaxy A6 ("Cinderella", Android 9, Termux/Debian Proot) running Mosquitto MQTT (:1883), PostgreSQL, and Grafana (:3000).

## Build Commands
```bash
# Build firmware
pio run

# Upload firmware to ESP32
pio run --target upload

# Upload LittleFS filesystem
pio run --target uploadfs

# Monitor serial output
pio run --target monitor
```

## Dependencies (PlatformIO `platformio.ini`)
- `h2zero/NimBLE-Arduino`
- `ESPAsyncWebServer`
- `AsyncTCP`
- `bblanchon/ArduinoJson`
- `knolleary/PubSubClient`

## Constraints
- **Flash / RAM limits:** LittleFS partition sized to ~1.5MB; RAM budget carefully monitored when running Wi-Fi + BLE concurrently.
- **230V Mains Safety:** Isolated relays for electrothermal actuators; proper clearance and optocoupled inputs.
- **Timing Deadlines:** Non-blocking asynchronous design so BLE scanning does not stall web requests or relay safety timers.
