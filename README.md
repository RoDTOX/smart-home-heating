<div align="center">

# 🔥 Smart Home Heating — Autonomous Underfloor Heating Controller

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=16&pause=1000&color=F59E0B&center=true&vCenter=true&width=620&lines=Autonomous+Multi-Zone+Underfloor+Heating;TPI+Screed+Pulse+Control+%E2%80%A2+Weather-Shifted+Preheat;100%25+Local+%E2%80%A2+Zero+Cloud+%E2%80%A2+Embedded+Web+UI+%26+REST;Fail-Safe+Hydraulics+%E2%80%A2+Interlock+%26+Burner+Guard;Xiaomi+BLE+Sensors+%E2%80%A2+433.92+MHz+Boiler+RF+Link" alt="Typing SVG" />
</p>

<p align="center">
  <b>⚡ Install once, calibrate once, forget it — Precision climate control for high-inertia hydronic underfloor heating</b>
</p>

<!-- Project Status & Specs (Row 1) -->
<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square" alt="License" /></a>
  <a href="docs/hardware_esp32_relay.md"><img src="https://img.shields.io/badge/Hardware-ESP32--Relay%20X4%20V1.1-orange.svg?style=flat-square&logo=espressif" alt="Hardware" /></a>
  <img src="https://img.shields.io/badge/Connectivity-Wi--Fi%20%7C%20BLE%20%7C%20433.92%20MHz-purple.svg?style=flat-square&logo=bluetooth" alt="Connectivity" />
  <img src="https://img.shields.io/badge/Precision-%C2%B10.3%20%C2%B0C%20Target-brightgreen.svg?style=flat-square" alt="Precision" />
  <img src="https://img.shields.io/badge/Safety-Fail--Safe%20Interlock%20%26%20Overrun-red.svg?style=flat-square" alt="Safety" />
  <img src="https://img.shields.io/badge/PlatformIO-Passing%20(0%20errors)-success.svg?style=flat-square&logo=platformio" alt="Build" />
</p>

<!-- Tech Stack Tags (Row 2) -->
<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B17-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++17" />
  <img src="https://img.shields.io/badge/ESP32-E7352C?style=flat-square&logo=espressif&logoColor=white" alt="ESP32" />
  <img src="https://img.shields.io/badge/FreeRTOS-82BE27?style=flat-square&logo=freertos&logoColor=white" alt="FreeRTOS" />
  <img src="https://img.shields.io/badge/LittleFS-008080?style=flat-square" alt="LittleFS" />
  <img src="https://img.shields.io/badge/REST_API-009688?style=flat-square&logo=fastapi&logoColor=white" alt="REST API" />
  <img src="https://img.shields.io/badge/Python_3.13-3776AB?style=flat-square&logo=python&logoColor=white" alt="Python" />
  <img src="https://img.shields.io/badge/Open--Meteo-F58220?style=flat-square" alt="Open-Meteo" />
</p>

<!-- Navigation Bar -->
<p align="center">
  <a href="#-key-features">✨ <b>Features</b></a> •
  <a href="#%EF%B8%8F-how-it-works--architecture">⚙️ <b>Architecture</b></a> •
  <a href="#-hardware-specifications--reference-install">📐 <b>Hardware</b></a> •
  <a href="#%EF%B8%8F-embedded-web-ui-smartiva-glass">🎛️ <b>Web UI</b></a> •
  <a href="#-quick-start--build-guide">🚀 <b>Quick Start</b></a> •
  <a href="#-wireless-ota-updates">📡 <b>Wireless OTA</b></a> •
  <a href="#%EF%B8%8F-cli-tool--rest-api-reference">🛠️ <b>CLI & API</b></a> •
  <a href="#-safety-invariants--failsafes">🔒 <b>Safety</b></a> •
  <a href="#-documentation--links">📄 <b>Docs</b></a>
</p>

<br/>

<p align="center">
  <img src="docs/smart_heating_banner.png" alt="Smart Home Heating Controller Hardware and Glass Dashboard UI Presentation" width="100%" />
</p>

---

</div>

## 🌟 Key Features

<table>
  <tr>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/PRIVACY-100%25%20LOCAL-success?style=flat-square" alt="Privacy" /><br/>
      <b>🛡️ 100% Local & Cloud-Independent</b><br/>
      Operates fully autonomously on the ESP32. <b>Zero external cloud accounts</b>, zero subscription fees, and no remote telemetry tracking. High-performance REST API and single-page web UI run self-contained out of Flash LittleFS.
    </td>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/CONTROL-TPI%20SCREED%20PULSE-orange?style=flat-square" alt="TPI Control" /><br/>
      <b>⏱️ TPI Screed Pulse Modulation</b><br/>
      Calculates hourly pulse-width modulation (PWM) fractions specifically tuned for heavy concrete screed inertia. Automatically prevents the severe overshooting and undershooting inherent to traditional bang-bang thermostats.
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/WEATHER-PREDICTIVE%20PREHEAT-blue?style=flat-square" alt="Weather" /><br/>
      <b>🌤️ Weather-Shifted Predictive Pre-Heat</b><br/>
      Integrates directly with Open-Meteo outdoor temperature telemetry. Shifts scheduled temperature transitions earlier based on outdoor chill (<b>2.0h earlier at +5 °C, 3.5h earlier at −5 °C</b>) so rooms reach target temperature right on time.
    </td>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/SAFETY-MULTI--TIER%20FAILSAFE-red?style=flat-square" alt="Safety" /><br/>
      <b>🔒 Multi-Tier Hydraulic Safety Invariants</b><br/>
      Hardcoded embedded interlocks: <b>actuator opening delay</b> before boiler ignition, burner anti-short-cycling guard, minimum pump overrun dissipation, <b>frost protection (&lt;12 °C)</b>, and sensor loss fallback duty.
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/SENSORS-PASSIVE%20BLE%20NIMBLE-blueviolet?style=flat-square" alt="BLE" /><br/>
      <b>📡 Passive Xiaomi BLE Scanning</b><br/>
      Reads room temperature & humidity directly from off-the-shelf Xiaomi LYWSD03MMC sensors (PVVX / BTHome V2 firmware). Background passive listening requires zero Bluetooth pairing and consumes <b>zero sensor battery life</b>.
    </td>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/BOILER%20LINK-433.92%20MHZ%20OOK-brightgreen?style=flat-square" alt="Boiler Link" /><br/>
      <b>🔥 Wireless 433.92 MHz Boiler Actuation</b><br/>
      Emulates wireless thermostat radio commands via on-board 433.92 MHz OOK transmitter to toggle condensing gas boiler dry contacts. <b>Eliminates wall drilling and cable runs</b> between the heating manifold and the boiler.
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/UI-SMARTIVA%20GLASS-informational?style=flat-square" alt="UI" /><br/>
      <b>💎 Modern Glass Dashboard Web UI</b><br/>
      Stunning single-page application with 5 dark and glassmorphism themes (<i>Smartiva Glass, HA Charcoal, Glass Glow, Smartiva Dark, Nordic Light</i>). Features interactive radial zone dials, 24h historical telemetry curves, and real-time gas cost estimation.
    </td>
    <td width="50%" valign="top">
      <img src="https://img.shields.io/badge/DIAGNOSTICS-DTC%20FLIGHT%20RECORDER-purple?style=flat-square" alt="Diagnostics" /><br/>
      <b>🩺 Automotive DTC Diagnostic Engine</b><br/>
      Standardized Diagnostic Trouble Codes (e.g. <code>DTC-101</code> sensor timeout, <code>DTC-201</code> interlock violation) stored in a persistent LittleFS flight recorder ring buffer with instant CLI / UI inspection and clearing tools.
    </td>
  </tr>
</table>

---

## ⚙️ How It Works & Architecture

Hydronic underfloor heating possesses enormous thermal mass. Once a concrete screed warms up, it continues emitting heat for hours; conversely, heating a cold floor takes hours before room air temperature responds.

```text
┌─────────────────────────┐
│ Xiaomi BLE Sensors      │──(Passive BLE Scan)──┐
│ (LYWSD03MMC / BTHome)   │                      │
└─────────────────────────┘                      ▼
┌─────────────────────────┐           ┌─────────────────────┐         ┌──────────────────────┐
│ Open-Meteo Weather API  │──(REST)──►│   ESP32-Relay X4    │──Relays─► Thermal Actuators    │
│ (Outdoor Temperature)   │           │  Heating Controller │         │ (230V NC Manifold)   │
└─────────────────────────┘           └──────────┬──────────┘         └──────────────────────┘
                                                 │
                                                 └──(433.92 MHz OOK)─► Gas Boiler (Dry Contact)
```

### The Three-Tier Control Loop

1. **Target Schedules & Offsets:** Each zone defines individual setpoints for **Day**, **Night**, and **Away** modes. Slots (e.g., `06:30=day, 22:30=night`) coordinate transitions. Temporary +/- adjustments (**Boost**) remain active until the next slot boundary.
2. **Predictive Weather Compensation:** Rather than waiting for the morning slot to trigger heating, the controller samples outdoor temperature and shifts startup earlier (e.g., 2.0h at 5 °C; 3.5h at −5 °C).
3. **TPI Screed Pulse Modulation:** Instead of crude thermostat cycling, every 60 minutes each zone opens its electrothermal valve for a calculated fraction of the hour (`error / 1.5 °C + learned base duty`, minimum pulse 10 min).

---

## 📐 Hardware Specifications & Reference Install

The reference installation controls a 7-circuit underfloor heating manifold partitioned into 3 independent zones, paired with an **Immergas Victrix Tera 24** condensing boiler.

```text
       +-------------------------------------------------------+
       | [Power Supply: 230V AC or 5V DC]                      |
       |                                                       |
       |   +--------------------------+    [Status LED: IO23]  |
       |   |    ESP32-WROOM-32E       |    [RF 433 MHz: IO19]  |
       |   | Dual-Core 240MHz, 4MB FL |                        |
       |   +--------------------------+    [Relay 1: GPIO32]   |
       |                                   [Relay 2: GPIO33]   |
       |   [Prog Header: TX1, RX3, GND]    [Relay 3: GPIO25]   |
       |   [Buttons: IO0 (Boot), RST]      [Relay 4: GPIO26]   |
       +-------------------------------------------------------+
```

| Component | Designator / Pin | Function / Reference Assignment |
|---|---|---|
| **MCU Board** | ESP32-Relay X4 V1.1 | ESP32-D0WD-V3, 4 MB SPI Flash, 320 KB SRAM |
| **Relay 1** | `GPIO32` (Active HIGH) | **Zone 1:** Living Room + Kitchen + Hallway (3 manifold circuits) |
| **Relay 2** | `GPIO33` (Active HIGH) | **Zone 2:** Children's Bedroom (2 manifold circuits) |
| **Relay 3** | `GPIO25` (Active HIGH) | **Zone 3:** Master Bedroom (2 manifold circuits) |
| **Relay 4** | `GPIO26` (Active HIGH) | Auxiliary dry contact / Secondary GPA pump |
| **RF Transmitter** | `GPIO19` (Data) | 433.92 MHz OOK wireless boiler trigger (train of 409 transitions) |
| **Status LED** | `GPIO23` (Active LOW) | Onboard indicator (fast pulses during command activity, solid standby) |
| **Sensors** | BLE Passive Scan | Xiaomi Mijia LYWSD03MMC running custom PVVX / BTHome V2 firmware |
| **Bypass** | Hydraulic | Bathroom towel radiators remain permanently open for continuous circulation |

---

## 🎛️ Embedded Web UI ("Smartiva Glass")

The on-board web interface is served directly from the ESP32 Flash via **LittleFS** with zero external dependencies (embedded fonts and CSS, no internet CDN calls required).

<p align="center">
  <img src="docs/ui_dashboard.jpg" alt="Smartiva Glass Live Dashboard Web UI Preview" width="100%" />
</p>

### Interface Capabilities
- **Glassmorphism Design:** Five selectable visual themes stored in `localStorage` (*Smartiva Glass*, *HA Charcoal*, *Glass Glow*, *Smartiva Dark*, *Nordic Light*).
- **Zone Room Cards:** Real-time temperature, humidity, target setpoint, radial dial adjustments, and valve pulse status.
- **Telemetry Charts:** 24-hour historical temperature trendlines read from `/api/history`.
- **Boiler & Gas Telemetry:** Burner runtime, pause counter, estimated gas volume (`m³/h`), and real-time heating cost calculation.
- **Admin Management Console (`#admin`):** Full configuration of schedules, slot times, sensor MAC pairings, temperature offsets, and diagnostic event history.

---

## 🚀 Quick Start & Build Guide

### 1. Configuration (`include/secrets.h`)

Create an untracked `include/secrets.h` file containing your local network credentials:

```c
#pragma once

#define DEFAULT_WIFI_SSID "Your_WiFi_SSID"
#define DEFAULT_WIFI_PASS "Your_WiFi_Password"
#define OTA_USER          "admin"
#define OTA_PASS          "Your_Admin_Password"
```

### 2. Compile & Flash Initial Binary

```bash
# Compile firmware using PlatformIO
pio run -e esp32dev

# Upload via USB serial bridge (first time flash)
pio run -e esp32dev --target upload
```

### 3. Verify System Operations

After flashing, run the comprehensive automated validation suite against the active device:

```bash
python tools/sh_process_check.py
python tools/sh_test_system.py
```

---

## 📡 Wireless OTA Updates

Once the initial binary is flashed, all subsequent firmware and web interface updates are performed wirelessly over Wi-Fi without opening the electrical enclosure.

### Option A: Python CLI Tool (Recommended)

```bash
# Compile and stream firmware over the local network
pio run -e esp32dev
python tools/sh_ota_upload.py
```

> [!NOTE]
> `sh_ota_upload.py` performs rigorous safety validation before initiating upload, verifying image integrity, `secrets.h` presence, and partition size boundaries (working margin &le; 1 306 624 B).

### Option B: Browser Web OTA Interface

Navigate to `http://192.168.1.20/update`, log in with your configured admin credentials, select `.pio/build/esp32dev/firmware.bin`, and click **Update Firmware**. The system validates the binary, writes to the inactive OTA bank, and reboots in ~15 seconds.

---

## 🛠️ CLI Tool & REST API Reference

Full headless automation is available via the companion CLI tool [`tools/sh_esp_cli.py`](tools/sh_esp_cli.py) or direct REST endpoints.

### Companion CLI Examples

```bash
# Display live system status and zone telemetry
python tools/sh_esp_cli.py status

# Set Zone 1 target temperature to 22.5 °C
python tools/sh_esp_cli.py zone 1 target 22.5

# Toggle global Away mode
python tools/sh_esp_cli.py away on
python tools/sh_esp_cli.py away off

# Display active schedules and slot definitions
python tools/sh_esp_cli.py schedule

# Inspect active DTC alarms and flight recorder history
python tools/sh_esp_cli.py diag
python tools/sh_esp_cli.py diag clear-events
```

### Core REST API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | `GET` | Complete controller telemetry: boiler state, flame, outdoor temp, and zones. |
| `/api/zones` | `GET`, `POST` | Read all zone states or update target setpoints / temporary boost trims. |
| `/api/schedule` | `GET`, `POST` | Inspect or update weekday/weekend slot boundaries and level setpoints. |
| `/api/diag` | `GET` | Active Diagnostic Trouble Codes (DTCs), sensor RSSI, and uptime. |
| `/api/diag/events` | `GET`, `POST` | Retrieve persistent DTC ring-buffer event history or clear event log. |
| `/api/history` | `GET` | 24-hour historical room temperature and outdoor temperature curves. |

*Telnet console access is also enabled by default on port 23 (`telnet 192.168.1.20 23`).*

---

## 🔒 Safety Invariants & Failsafes

Heating controllers must be engineered with absolute defense-in-depth against hardware failure and thermal hazards:

| Safety Guard | Invariant Rule | Failure Protection |
|---|---|---|
| **Actuator Interlock (FR-2)** | Delays boiler ON command by `interlockDelay` (~180s). | Prevents boiler pump from firing against fully closed electrothermal wax actuators. |
| **Overrun Dissipation (FR-3)** | Last open valve remains open while boiler fires + `overrun` period. | Absorbs residual latent heat from the boiler heat exchanger. |
| **Anti-Short-Cycling (FR-4)** | Enforces `burnerMinRun` and `burnerMinPause` cooldown timers. | Protects boiler combustion chamber and electronics from rapid cycling fatigue. |
| **Frost Protection (FR-16)** | Activates zone valves and boiler immediately if any room drops &lt; 12.0 °C. | Prevents hydraulic pipe freezing during prolonged winter vacancies or Away mode. |
| **Sensor Loss Guard (FR-51)** | Fallback to safe baseline duty cycle after `sensorTimeout` + raises `DTC-101`. | Prevents unmonitored freezing or continuous boiling if a wireless sensor battery dies. |
| **Safe Boot State (NFR-1.4)** | Boots with all relays open, fires boiler OFF pulse, completes warm-up period. | Guarantees safe zero-heat state upon power restoration after electrical outages. |

---

## 📄 Documentation & Links

- **[Project Migration Plan](docs/plan.md)** — Architectural migration status and roadmap.
- **[Requirements & Specifications](docs/requirements_and_features.md)** — Complete functional and safety requirement catalog.
- **[System Architecture](docs/architecture.md)** — Deep-dive into software design, TPI maths, and memory layout.
- **[REST API Reference](docs/api_reference.md)** — Exhaustive documentation of all JSON endpoints.
- **[Hardware & Wiring Guide](docs/hardware_esp32_relay.md)** — Pin assignments, PCB revisions, and serial programming.
- **[Hydraulics & Failsafes](docs/underfloor_heating_hydraulics_and_failsafes.md)** — Mechanical plumbing design and thermal failsafes.
- **[Changelog](docs/changelog.md)** — Detailed historical log of updates and verification records.

---

## 📜 License & Credits

- **Firmware & Tools:** [MIT License](LICENSE). Developed by [Bogdan Seceleanu](https://github.com/RoDTOX) with Google DeepMind Antigravity.
- **Reference Hardware:** ESP32-Relay X4 V1.1 / Espressif Systems.
- **Sensors:** Xiaomi Mijia BLE (PVVX open-source firmware / BTHome V2).
