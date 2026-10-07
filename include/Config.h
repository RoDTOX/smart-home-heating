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
// 4. MQTT Broker Settings (Samsung Galaxy A6 Gateway)
// ==========================================
#ifndef MQTT_BROKER_HOST
#define MQTT_BROKER_HOST      "192.168.1.28"
#endif

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT      1883
#endif

#ifndef MQTT_TOPIC_SUB
#define MQTT_TOPIC_SUB        "home/sensors/ble/#"
#endif

#endif // CONFIG_H
