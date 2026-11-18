# Smart Heating - Home Assistant Custom Component

[![hacs_badge](https://img.shields.io/badge/HACS-Custom-orange.svg)](https://github.com/hacs/default)
[![Firmware](https://img.shields.io/badge/ESP32--Relay-v1.11-blue.svg)](https://github.com/bogdan/smart-home-heating)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-2024.1+-brightgreen.svg)](https://www.home-assistant.io/)

Native Home Assistant integration for **Smart Heating** autonomous multi-zone underfloor heating controllers (ESP32-Relay X4 V1.1).

---

## Features

- **Multi-Zone Underfloor Climate Control**:
  - Independent `climate` entities for each thermal zone (`Living + Bucătărie`, `Dormitor Copii`, `Dormitor Mare`).
  - Target temperature control with 0.5°C steppers.
  - Support for `comfort`, `eco`, and `away` presets.
  - Live HVAC action feedback (`heating` vs `idle`).
- **Gas Metering & Energy Cost Analytics**:
  - Gas consumption for today, yesterday, and current month in m³.
  - Financial cost tracking in Lei (configured via live gas price per m³).
  - Gas flow rate telemetry during active burns.
- **Boiler & Hydraulic Safety Monitoring**:
  - Binary sensors for active burner flame demand and circulation pump status.
  - Burner cooldown timer and daily burn duration tracking.
  - Thermal actuator valve states per zone.
- **Window Open Detection & Pause**:
  - Window pause state sensors for each zone.
  - Integration with the controller's automatic temperature drop detection algorithm.
  - One-click service to clear window pause and resume heating.
- **System Diagnostics & Fault Codes (DTC)**:
  - Real-time diagnostic fault counters and warnings.
  - Controller telemetry: free heap, Wi-Fi RSSI signal, uptime.
  - Clear inactive DTC history with a single button press.

---

## Installation

### Method 1: HACS (Recommended)

1. Ensure [HACS](https://hacs.xyz/) is installed on your Home Assistant server.
2. In Home Assistant, open **HACS** > **Integrations** > Three dots in top right > **Custom repositories**.
3. Add repository URL: `https://github.com/bogdan/smart-home-heating` with category **Integration**.
4. Click **Download** and restart Home Assistant.

### Method 2: Manual Copy (ThinkPad L590 Docker Setup)

Copy the `custom_components/smart_heating` directory to your Home Assistant `/config/custom_components/` folder:

```bash
# Example from development workstation to HA server (192.168.1.11)
scp -r custom_components/smart_heating user@192.168.1.11:/path/to/homeassistant/config/custom_components/
```

Restart Home Assistant.

---

## Setup & Configuration

1. In Home Assistant, navigate to **Settings** > **Devices & Services**.
2. Click **Add Integration** and search for **Smart Heating**.
3. Enter your ESP32 controller IP address (default: `192.168.1.20`) and port (`80`).
4. Click **Submit**. Home Assistant will discover all 3 zones, sensors, relays, and diagnostics automatically.

---

## Lovelace Dashboard

A pre-configured dashboard template matching the **HA Charcoal** design specification is available in [`dashboards/smart_heating_dashboard.yaml`](file:///dashboards/smart_heating_dashboard.yaml).

---

## Custom Services

| Service | Description | Parameters |
|---|---|---|
| `smart_heating.clear_dtc_history` | Purges inactive DTC fault records. | None |
| `smart_heating.clear_window_pause` | Clears active window pause and resumes heating. | `zone_id` (1, 2, 3, or 0 for all) |
| `smart_heating.set_window_pause` | Suspends heating on a zone for a given duration. | `zone_id` (1-3), `duration_sec` (default 1800) |
| `smart_heating.set_zone_boost` | Applies a temporary boost (+0.5 to +2.0°C). | `zone_id` (1-3), `boost_temp` (°C) |

---

## Quality Scale & Architecture

Built to meet Home Assistant **Platinum Quality Scale** standards:
- Completely asynchronous using non-blocking `aiohttp` sessions.
- Semaphore concurrency protection for low-latency embedded ESP32 web server safety.
- Full device registry hierarchy linking zones to the main controller hub (`via_device`).
- Zero YAML required for setup.
- Strict type hints and frozen dataclasses throughout.
