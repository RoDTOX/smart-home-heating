#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ==========================================
// 1. Wi-Fi & Access Point Configuration
// ==========================================
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID "Your_WiFi_SSID"
#endif

#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS "Your_WiFi_Password"
#endif

#ifndef AP_SSID
#define AP_SSID "SmartHeating-Setup"
#endif

#ifndef AP_PASS
#define AP_PASS "12345678"
#endif

#ifndef OTA_USER
#define OTA_USER "admin"
#endif

#ifndef OTA_PASS
#define OTA_PASS "admin123"
#endif

#ifndef DEFAULT_ADMIN_PASS
#define DEFAULT_ADMIN_PASS "admin.shh2026"
#endif

// ==========================================
// 2. Hardware Pin Allocation (ESP32-Relay X4 V1.1 / DOIT ESP32-32E)
// ==========================================
// Hardware BOOT / Reset Button (GPIO0 on ESP32)
#define PIN_BOOT_BTN          0

// 433.92 MHz RF Transmitter DATA pin
#define PIN_RF_DATA           19

// Onboard Status LED (Active LOW on GPIO23)
#define PIN_STATUS_LED        23

// Relay driver pins for underfloor heating manifold
#define PIN_RELAY_ZONE_1      32   // Relay 1
#define PIN_RELAY_ZONE_2      33   // Relay 2
#define PIN_RELAY_ZONE_3      25   // Relay 3
#define PIN_RELAY_SPARE       26   // Relay 4: zone or pump (FR-69)

// Relay logic levels (ESP32-Relay X4 optocouplers are active HIGH)
#define RELAY_ACTIVE_LEVEL    HIGH
#define RELAY_INACTIVE_LEVEL  LOW

// ==========================================
// 3. Multi-Zone Thermal Controller Defaults
// ==========================================
#define DEFAULT_SETPOINT_TEMP 22.0f   // manual target of a new zone
// Safety timings, thresholds and their ranges are requirements §6 parameters in src/ParamStore.cpp.


// ==========================================
// 5. Xiaomi BLE Thermometer Mappings (LYWSD03MMC)
// ==========================================
#define BLE_SCAN_INTERVAL_MS  100   // BLE scan interval
#define BLE_SCAN_WINDOW_MS    90    // BLE scan window (90% duty cycle for high-sensitivity capture through multiple walls)
// Sensor MACs and names are set at commissioning and stored in NVS (FR-11.1), never compiled in.

#endif // CONFIG_H
