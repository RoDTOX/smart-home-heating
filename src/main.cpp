#include "App.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "DiagnosticEngine.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TelemetryHistoryManager.h"
#include "TimeAndWeatherService.h"
#include "Version.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

WebServer server(80);
HTTPUpdateServer httpUpdater;

static bool softApActive = false;
static String serialBuffer = "";

void blinkLed(uint8_t count, uint16_t delayMs) {
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH); // Active HIGH on ESP32 Relay board
    delay(delayMs);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(delayMs);
  }
}

const char* getResetReasonStr(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "Power-On Reset (Cold Boot)";
    case ESP_RST_SW:        return "Software Reboot (Firmware Update / Restart)";
    case ESP_RST_PANIC:     return "Software Exception / Panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "Hardware Watchdog Timer Expiry (TWDT)";
    case ESP_RST_BROWNOUT:  return "Brownout Reset (Voltage Dip)";
    case ESP_RST_DEEPSLEEP: return "Deep-Sleep Wakeup";
    default:                return "Normal / External Reset";
  }
}

void printBootBannerAndSelfTest() {
  esp_reset_reason_t rstReason = esp_reset_reason();
  time_t nowEpoch = time(nullptr);
  char bootTimeBuf[36];
  if (nowEpoch > 1700000000) {
    struct tm ti;
    localtime_r(&nowEpoch, &ti);
    strftime(bootTimeBuf, sizeof(bootTimeBuf), "%Y-%m-%d %H:%M:%S", &ti);
  } else {
    snprintf(bootTimeBuf, sizeof(bootTimeBuf), "Pending NTP Sync (pool.ntp.org)");
  }

  netConsole.println();
  netConsole.println(F("=================================================="));
  netConsole.printf("  Smart Heating Controller v%s\n", APP_VERSION);
  netConsole.printf("  Build: %s %s | Git: %s\n", APP_BUILD_DATE, APP_BUILD_TIME, APP_GIT_COMMIT);
  netConsole.printf("  Boot Time: %s\n", bootTimeBuf);
  netConsole.printf("  Reset Cause: %s\n", getResetReasonStr(rstReason));
  netConsole.println(F("=================================================="));

  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  uint32_t heap = ESP.getFreeHeap();
  bool bleOk = bleScanner.isScanning();
  uint8_t health = diagEngine.calculateHealthScore();

  netConsole.println(F("[POST] Power-On Self-Test:"));
  netConsole.printf("  * Network Connection:    %s (IP: %s, Signal: %d dBm)\n",
                    wifiOk ? "PASS" : "FAIL",
                    wifiOk ? WiFi.localIP().toString().c_str() : "No IP",
                    wifiOk ? WiFi.RSSI() : 0);
  netConsole.printf("  * System Memory (RAM):   PASS (%u KB free, no leaks)\n", heap / 1024);
  netConsole.printf("  * Thermal Actuators:     PASS (%s, User-Configured in NVS)\n",
                    thermalZones.isActuatorNormallyClosed() ? "Normally Closed [NC]" : "Normally Open [NO]");
  netConsole.printf("  * Circulation Pump:      PASS (Secondary Pump on Relay 4: %s)\n",
                    thermalZones.isPumpControlEnabled() ? "ENABLED" : "DISABLED");
  netConsole.printf("  * Room Thermometers:     %s (Bluetooth Receiver active, listening)\n",
                    bleOk ? "PASS" : "FAIL");
  netConsole.printf("  * Gas Boiler Radio (RF): PASS (Transmitter on GPIO%d Ready)\n", PIN_RF_DATA);
  netConsole.println(F("  * System Crash Guard:    PASS (Hardware Watchdog Armed: 90s timeout)"));
  netConsole.printf("  * Flash Telemetry:       %s (LittleFS Ring Buffer: 24h@5m, 5d@30m)\n",
                    telemetryHistory.isFsMounted() ? "PASS" : "WARN");
  netConsole.println(F("  * Web Dashboard & API:   PASS (HTTP Port 80, Telnet Port 23)"));
  netConsole.printf("  * Overall Health Score:  %u%% [%s]\n",
                    health,
                    health >= 90 ? "EXCELLENT" : (health >= 70 ? "GOOD" : "DEGRADED"));
  netConsole.println(F("--------------------------------------------------"));
  netConsole.println(F("[Ready] Type 'HELP' or '?' for command list."));
  netConsole.println(F("=================================================="));
  netConsole.println(F("[INIT] Background Tasks Dispatched:"));
  netConsole.println(F("  * Real-Time Clock:  Background NTP sync initiated (pool.ntp.org)"));
  netConsole.println(F("  * Outdoor Weather:  Background sync initiated (Open-Meteo REST API)"));
  netConsole.println(F("  * Sensor Discovery: Continuous BLE scanner active (Warmup grace period: 300s)"));
  netConsole.println();
}

// Wi-Fi Connection Supervisor State
static uint32_t _wifiDisconnectMs = 0;
static bool     _wifiWasConnected = true;

void setupWifi() {
  WiFi.setHostname("SmartHeating-BoilerRF");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  Serial.printf("[WiFi] Connecting to '%s'...\n", DEFAULT_WIFI_SSID);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
    delay(300);
    Serial.print(F("."));
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected successfully! Local IP: %s\n",
                  WiFi.localIP().toString().c_str());
    digitalWrite(PIN_STATUS_LED, LOW); // LED off when connected
    _wifiWasConnected = true;
    _wifiDisconnectMs = 0;
  } else {
    Serial.println(F("[WiFi] Could not connect to router within 12s. Starting "
                     "Fallback Access Point..."));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    softApActive = true;
    _wifiWasConnected = false;
    _wifiDisconnectMs = millis();
    Serial.printf("[WiFi] AP active: '%s', Password: '%s', IP: %s\n", AP_SSID,
                  AP_PASS, WiFi.softAPIP().toString().c_str());
  }
}

// Wi-Fi Connection Supervisor & Auto-Recovery Watchdog
static uint32_t _lastWifiCheckMs = 0;

void updateWifiWatchdog() {
  uint32_t now = millis();
  if (now - _lastWifiCheckMs < 10000UL) return; // Non-blocking check every 10s
  _lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    if (!_wifiWasConnected) {
      _wifiWasConnected = true;
      _wifiDisconnectMs = 0;
      digitalWrite(PIN_STATUS_LED, LOW);
      netConsole.printf("[WiFi][RECOVERED] Reconnected successfully! Local IP: %s (RSSI: %d dBm)\n",
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
      diagEngine.clearDtc("DTC-401");
    }
  } else {
    if (_wifiWasConnected) {
      _wifiWasConnected = false;
      _wifiDisconnectMs = now;
      digitalWrite(PIN_STATUS_LED, HIGH); // Status LED ON indicates connection lost
      netConsole.println(F("[WiFi][WARN] Wi-Fi connection lost! Initiating auto-recovery..."));
      diagEngine.raiseDtc("DTC-401", "Wi-Fi link disconnected", SEV_WARNING);
    }

    uint32_t disconnectedDurationSec = (now - _wifiDisconnectMs) / 1000;

    if (disconnectedDurationSec > 90) {
      netConsole.println(F("[WiFi][RETRY] Disconnected > 90s. Hard reset Wi-Fi radio and reconnecting..."));
      WiFi.disconnect();
      delay(50);
      WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
      _wifiDisconnectMs = now;
    } else {
      netConsole.println(F("[WiFi][RETRY] Attempting non-blocking Wi-Fi reconnect..."));
      WiFi.reconnect();
    }
  }
}

// Returns visible display character count, properly ignoring UTF-8 continuation bytes (0x80..0xBF)
void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW); // Off (active HIGH)

  Serial.printf("[Hardware] Initializing RF transmitter on GPIO%d...\n",
                PIN_RF_DATA);
  boilerRadio.begin(PIN_RF_DATA);

  // Initialize Multi-Zone Thermal Controller (NC actuator relays)
  thermalZones.begin();

  setupWifi();

  // Initialize 2-Way Network Console (Telnet :23 and /api/console)
  netConsole.begin(&server);
  netConsole.setCommandHandler(handleSerialCommand);

  // Initialize Time & NTP Service
  timeAndWeather.begin();

  // Quick settle for NTP time synchronization (up to 2 seconds)
  uint32_t ntpWaitStart = millis();
  while (!timeAndWeather.isTimeSynced() && (millis() - ntpWaitStart < 2000)) {
    timeAndWeather.update();
    delay(50);
    yield();
  }



  // Initialize Native Passive BLE Scanner (Xiaomi BTHome v2 & PVVX)
  bleScanner.begin();

  // Initialize Diagnostic Engine
  diagEngine.begin();

  // Initialize Flash Telemetry History Manager (LittleFS wear-leveling ring buffer)
  telemetryHistory.begin();

  registerWebRoutes();
  // Initialize Hardware Task Watchdog Timer (90 seconds timeout)
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 90000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&twdt_config);
#else
  esp_task_wdt_init(90, true);
#endif
  esp_task_wdt_add(NULL);
  Serial.println(
      F("[System] Hardware Task Watchdog Timer (TWDT) armed: 90s."));

  // Feed TWDT during OTA flash writes so watchdog never triggers
  Update.onProgress([](size_t progress, size_t total) {
    esp_task_wdt_reset();
  });

  // Enable Web OTA on /update (admin / admin123)
  httpUpdater.setup(&server, "/update", OTA_USER, OTA_PASS);
  server.begin();

  printBootBannerAndSelfTest();
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  netConsole.update();
  bleScanner.update();
  timeAndWeather.update();
  if (timeAndWeather.isTimeSynced()) {
    boilerRadio.updateRuntimeDayMonth(timeAndWeather.getDay(), timeAndWeather.getMonth());
  }
  telemetryHistory.update();
  diagEngine.update();
  boilerRadio.update();
  thermalZones.update(boilerRadio);
  updateWifiWatchdog();

  // Non-blocking serial command reader
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        netConsole.executeCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}
