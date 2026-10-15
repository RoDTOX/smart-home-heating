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

// ==========================================
// 2. Hardware Pin Allocation (ESP32-Relay X4 V1.1 / DOIT ESP32-32E)
// ==========================================
// 433.92 MHz RF Transmitter DATA pin
#define PIN_RF_DATA           19

// Onboard Status LED (Active LOW on GPIO23)
#define PIN_STATUS_LED        23

// Relay driver pins for underfloor heating manifold
#define PIN_RELAY_ZONE_1      32   // Relay 1: Living & Kitchen (Loops 1, 2, 3)
#define PIN_RELAY_ZONE_2      33   // Relay 2: Child Bedroom (Loops 6, 7)
#define PIN_RELAY_ZONE_3      25   // Relay 3: Master Bedroom (Loops 4, 5)
#define PIN_RELAY_SPARE       26   // Relay 4: Spare / Auxiliary (Pump / Bypass)

// Relay logic levels (ESP32-Relay X4 optocouplers are active HIGH)
#define RELAY_ACTIVE_LEVEL    HIGH
#define RELAY_INACTIVE_LEVEL  LOW

// ==========================================
// 3. Multi-Zone Thermal Controller Defaults
// ==========================================
#define DEFAULT_SETPOINT_TEMP 22.0f
#define DEFAULT_HYSTERESIS    0.3f
#define DEFAULT_ACTUATOR_DELAY_MS 5000UL   // 5s test bench (120s in production for NC wax actuators)
#define DEFAULT_PUMP_OVERRUN_MS   10000UL  // 10s test bench (120s in production)
#define DEFAULT_MIN_BOILER_OFF_MS 15000UL  // 15s test bench (180s anti-short cycling in production)


// ==========================================
// 5. Xiaomi BLE Thermometer Mappings (LYWSD03MMC)
// ==========================================
#define BLE_SCAN_INTERVAL_MS  100   // BLE scan interval
#define BLE_SCAN_WINDOW_MS    90    // BLE scan window (90% duty cycle for high-sensitivity capture through multiple walls)

struct BleDeviceMapEntry {
    const char* mac;
    const char* name;
};

static const BleDeviceMapEntry KNOWN_BLE_DEVICES[] = {
    {"A4:C1:38:A4:C1:DD", "Living Room"},
    {"A4:C1:38:84:A1:E8", "Kids Room"},
    {"A4:C1:38:94:0F:44", "Dormitor Matrimonial"}
};
static const size_t KNOWN_BLE_DEVICES_COUNT = sizeof(KNOWN_BLE_DEVICES) / sizeof(KNOWN_BLE_DEVICES[0]);

#endif // CONFIG_H
