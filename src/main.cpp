#include "Commands.h"
#include "App.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "DiagnosticEngine.h"
#include "EventLog.h"
#include "NetworkConsole.h"
#include "ConsoleAuth.h"
#include "ThermalZoneManager.h"
#include "TelemetryHistoryManager.h"
#include "TimeAndWeatherService.h"
#include "Maintenance.h"
#include "PlantMonitor.h"
#include "HeatUpModel.h"
#include "SolarThrottle.h"
#include "CycleAligner.h"
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

// FR-62.1: every level change of the status LED is one entry; `log` false for the blink while connecting
static uint8_t _ledLevel = 255;
static void ledWrite(uint8_t level, uint8_t reason, bool log = true) {
  digitalWrite(PIN_STATUS_LED, level);
  if (level != _ledLevel) {
    if (log) eventLog.add(EV_OUT, PIN_STATUS_LED, reason, 0, _ledLevel, level);
    _ledLevel = level;
  }
}

void blinkLed(uint8_t count, uint16_t delayMs) {
  for (uint8_t i = 0; i < count; i++) {
    ledWrite(HIGH, RS_USER); // Active HIGH on ESP32 Relay board
    delay(delayMs);
    ledWrite(LOW, RS_USER);
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

  // FR-10.4: the boot self-test prints the measured checks only, no PASS without a check
  DiagCheck checks[DIAG_CHECK_COUNT];
  diagEngine.runChecks(checks);
  netConsole.println(F("[POST] Power-On Self-Test (FR-10.4):"));
  for (size_t i = 0; i < DIAG_CHECK_COUNT; i++) {
    netConsole.printf("  * %-8s %-5s %-20s %s\n", checks[i].id, checks[i].result,
                      checks[i].value, checks[i].rule);
  }
  netConsole.printf("  * Flash Telemetry: LittleFS %s\n",
                    telemetryHistory.isFsMounted() ? "mounted" : "NOT mounted");
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
    ledWrite(!_ledLevel, RS_WIFI, false);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected successfully! Local IP: %s\n",
                  WiFi.localIP().toString().c_str());
    ledWrite(LOW, RS_WIFI); // LED off when connected
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
    Serial.printf("[WiFi] AP active: '%s', IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
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
      ledWrite(LOW, RS_WIFI);
      netConsole.printf("[WiFi][RECOVERED] Reconnected successfully! Local IP: %s (RSSI: %d dBm)\n",
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
      diagEngine.clearDtc(Dtc::WARN_WIFI_SIGNAL_DEGRADED);
    }
  } else {
    if (_wifiWasConnected) {
      _wifiWasConnected = false;
      _wifiDisconnectMs = now;
      ledWrite(HIGH, RS_WIFI); // Status LED ON indicates connection lost
      netConsole.println(F("[WiFi][WARN] Wi-Fi connection lost! Initiating auto-recovery..."));
      diagEngine.raiseDtc(Dtc::WARN_WIFI_SIGNAL_DEGRADED, "Wi-Fi link disconnected", SEV_WARNING);
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

// Hardware Reset Button (GPIO0 / BOOT) Supervisor (10s hold = credentials reset)
static void checkPhysicalResetButton() {
  static uint32_t pressStartMs = 0;
  static bool wasPressed = false;
  static uint32_t lastBlinkMs = 0;
  static uint8_t blinkState = LOW;

  bool pressed = (digitalRead(PIN_BOOT_BTN) == LOW); // Active LOW on ESP32 BOOT button

  if (pressed) {
    if (!wasPressed) {
      wasPressed = true;
      pressStartMs = millis();
    }
    uint32_t holdMs = millis() - pressStartMs;

    // Visual feedback on Status LED:
    if (holdMs >= 10000) {
      // 10s reached: fast blinking (100 ms toggle = 5 Hz)
      if (millis() - lastBlinkMs >= 100) {
        lastBlinkMs = millis();
        blinkState = (blinkState == HIGH) ? LOW : HIGH;
        ledWrite(blinkState, RS_USER, false);
      }
    } else if (holdMs >= 3000) {
      // 3s..10s: slow blinking (500 ms toggle = 1 Hz)
      if (millis() - lastBlinkMs >= 500) {
        lastBlinkMs = millis();
        blinkState = (blinkState == HIGH) ? LOW : HIGH;
        ledWrite(blinkState, RS_USER, false);
      }
    }
  } else {
    if (wasPressed) {
      uint32_t holdMs = millis() - pressStartMs;
      wasPressed = false;
      ledWrite(LOW, RS_USER, false); // Restore LED off

      if (holdMs >= 10000 && holdMs < 30000) {
        // Credential reset to default
        Serial.println(F("[SYSTEM] BOOT button held 10s+: Resetting admin credentials to default."));
        netConsole.println(F("[SYSTEM] Hardware reset: Admin credentials restored to default (DTC-403)."));
        commands.resetToDefault();
        consoleAuth.unlockAll();
        diagEngine.raiseDtc(Dtc::WARN_DEFAULT_CREDENTIALS, "Default credentials active", SEV_WARNING);
        blinkLed(3, 80);
      } else if (holdMs >= 30000) {
        Serial.println(F("[SYSTEM] BOOT button held 30s+: Full factory reset deferred to Step 14."));
      } else {
        Serial.println(F("[SYSTEM] BOOT button released < 10s (ignored)."));
      }
    }
  }
}

// Returns visible display character count, properly ignoring UTF-8 continuation bytes (0x80..0xBF)
void setup() {
  Serial.setTxBufferSize(1024);  // NFR-2.2: 3 KB of heap back; the log goes to the network console
  Serial.begin(115200);
  delay(200);

  App::init();
  eventLog.add(EV_BOOT, 0, RS_BOOT, 0, 0, (uint16_t)esp_reset_reason());  // FR-62.1: first entry of this run
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  ledWrite(LOW, RS_BOOT); // Off (active HIGH)

  Serial.printf("[Hardware] Initializing RF transmitter on GPIO%d...\n",
                PIN_RF_DATA);
  paramStore.begin();  // first: every module reads its settings from it
  commands.begin();
  boilerRadio.begin(PIN_RF_DATA);

  // Initialize Multi-Zone Thermal Controller (NC actuator relays)
  thermalZones.begin();

  setupWifi();

  // Initialize 2-Way Network Console (Telnet :23 and /api/console)
  netConsole.begin(&server);
  consoleAuth.begin();
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
  // FR-11.6: the sensors a zone uses (by MAC or alias) always have their place in the sensor table
  bleScanner.setAssignedFn([](const char* mac, const char* name) {
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
      const ThermalZoneConfig& z = thermalZones.getConfig(i);
      if (z.sensorMac[0] && strcasecmp(z.sensorMac, mac) == 0) return true;
      if (z.sensorName[0] && name && strcasecmp(z.sensorName, name) == 0) return true;
    }
    return false;
  });
  bleScanner.begin();

  // Initialize Diagnostic Engine
  diagEngine.begin();

  // Initialize Flash Telemetry History Manager (LittleFS wear-leveling ring buffer)
  telemetryHistory.begin();
  maintenance.begin();

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

  // AR-21: Start control task on Core 1 (priority 3, period 1s, TWDT registered)
  App::startControlTask(&boilerRadio, &thermalZones);

  // Feed TWDT during OTA flash writes so watchdog never triggers
  Update.onProgress([](size_t progress, size_t total) {
    esp_task_wdt_reset();
    telemetryHistory.noteShutdown("OTA Update");
  });

  // Enable Web OTA on /update (admin / admin123)
#ifdef SH_DEV_OPEN
  httpUpdater.setup(&server, "/update");  // FR-75.6: development profile, /update open
#else
  httpUpdater.setup(&server, "/update", commands.getUser(), commands.getPass());
#endif
  server.begin();

  printBootBannerAndSelfTest();
}

void loop() {
  uint32_t loopStart = millis();
  App::feedLoopWatchdog();
  server.handleClient();
  netConsole.update();
  bleScanner.update();
  timeAndWeather.update();
  if (timeAndWeather.isTimeSynced()) {
    boilerRadio.updateRuntimeDayMonth(timeAndWeather.getDay(), timeAndWeather.getMonth());
    thermalZones.updateRuntimeDay(timeAndWeather.getDay());
  }
  telemetryHistory.update();
  diagEngine.update();
  maintenance.update();
  plantMonitor.update();
  heatUpModel.update();
  solarThrottle.update();
  cycleAligner.update();
  thermalZones.flushPendingNvs();
  updateWifiWatchdog();
  checkPhysicalResetButton();

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

  App::recordLoopPass(millis() - loopStart);
}
