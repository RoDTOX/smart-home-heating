# Goal: Complete Market-Grade Home Assistant Integration (`custom_components/smart_heating`)

## Original Request (verbatim)
Da, fa-o autonom si poți consulta și pemopus 5.5 low prin ClaudeCodeCLI . /goal important este să iasă ceva demn de market at. Calitativ și impresionant. Fără gres.

## Architecture & Implementation Scope
Build a complete, production-grade Home Assistant custom integration (`custom_components/smart_heating`) adhering to Home Assistant Platinum Integration Quality Scale:
1. **Integration Manifest & HACS Compliance:**
   - `manifest.json` (domain: `smart_heating`, config_flow: true, iot_class: local_polling)
   - `hacs.json` (HACS custom repository standards)
2. **Config Flow & Options Flow:**
   - `config_flow.py` (UI-based setup, IP auto-probe, duplicate prevention via hardware MAC/ID, connection test)
3. **Data Coordinator (`coordinator.py`):**
   - Async `DataUpdateCoordinator` fetching `/api/zones`, `/api/boiler/status`, `/api/ble/sensors`, `/api/diag/status`
   - Resilient error handling, backoff, and non-blocking I/O
4. **Entity Platforms:**
   - `climate.py` (SmartHeatingZoneClimate per zone: temp, target, hum, heating/idle hvac_action, presets)
   - `sensor.py` (Gas m³, daily cost Lei, outdoor temp, boiler modulation/flow, Wi-Fi RSSI, Heap, BLE battery)
   - `binary_sensor.py` (Boiler flame active, zone valve relays, window pause status, frost guard, DTC problem)
   - `number.py` (Configurable parameters: interlockDelay, pumpOverrun, frostTemp, gasPrice)
   - `select.py` (OperatingMode auto/manual/off, SeasonMode)
   - `button.py` (Clear DTC history, clear window pause, run self-test)
5. **Custom Services & Blueprints:**
   - `services.yaml` (set_window_pause, clear_window_pause)
   - Ready-to-use Automation Blueprint for door/window sensors (Sonoff DW2 integration)
6. **Lovelace Dashboard & Theming:**
   - `dashboards/smart_heating_dashboard.yaml` (Pre-configured HA dashboard using our `ha-charcoal` theme)
7. **Verification & Quality Gate:**
   - Unit/integration test script `tools/sh_test_ha_integration.py`
   - Process check validation (`python tools/sh_process_check.py`)
   - Advisory review via Claude CLI

## Stage Tracker
- [x] 1. Consult Claude CLI for architecture review & HA best practices
- [x] 2. Create integration folder structure and manifest/config files
- [x] 3. Implement API client, data models, and DataUpdateCoordinator
- [x] 4. Implement ConfigFlow (UI setup with validation)
- [x] 5. Implement Climate, Sensor, BinarySensor, Number, Select, Button platforms
- [x] 6. Implement services.yaml and DW2 Window Pause automation blueprint
- [x] 7. Create Lovelace dashboard template & comprehensive README.md
- [x] 8. Verify with automated test suite and live board queries
- [x] 9. Run process checks and commit cleanly
