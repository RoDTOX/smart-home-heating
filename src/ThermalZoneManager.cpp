#include "ThermalZoneManager.h"
#include "NetworkConsole.h"
#include "BleSensorScanner.h"
#include "DiagnosticEngine.h"
#include "TimeAndWeatherService.h"
#include <time.h>

ThermalZoneManager thermalZones;

ThermalZoneManager::ThermalZoneManager()
    : _activeAnchor(BIO_ANCHOR_HOME),
      _nextAnchor(BIO_ANCHOR_SLEEP),
      _minsToNextAnchor(0),
      _preHeatActive(false),
      _lastScheduleEvalMs(0),
      _autoMode(true),
      _boilerDemand(false),
      _pumpControlEnabled(true),
      _pumpActive(false),
      _pumpStopScheduledMs(0),
      _overrunKeeperZone(-1),
      _actuatorNormallyClosed(true),
      _freezeProtectionTemp(12.0f),
      _frostProtectionActive(false),
      _antiSeizeRunning(false),
      _antiSeizeStartMs(0),
      _lastAntiSeizeEpoch(0),
      _sensorTimeoutSec(600UL), // 10 minutes default
      _bootWarmupActive(true),
      _bootWarmupStartMs(0),
      _interlockDelayMs(DEFAULT_ACTUATOR_DELAY_MS),
      _pumpOverrunMs(DEFAULT_PUMP_OVERRUN_MS),
      _minBoilerOffMs(DEFAULT_MIN_BOILER_OFF_MS),
      _lastBoilerOffTime(0),
      _lastBoilerOnTime(0),
      _lastEvaluationMs(0) {

    _scheduleConfig = getFactoryDefaultSchedule();
    _scheduleConfig.enabled = false;
    strncpy(_activePresetId, "default", sizeof(_activePresetId));

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _windowStates[i].baselineTemp = -999.0f;
        _windowStates[i].baselineMs = 0;
        _windowStates[i].pauseActive = false;
        _windowStates[i].pauseStartMs = 0;
        _windowStates[i].pauseMinTemp = 999.0f;
    }

    // Default Zone 1
    _configs[0].id = 1;
    strncpy(_configs[0].name, "Living & Bucatarie", sizeof(_configs[0].name) - 1);
    strncpy(_configs[0].sensorMac, "A4:C1:38:A4:C1:DD", sizeof(_configs[0].sensorMac) - 1);
    strncpy(_configs[0].sensorName, "Living Room", sizeof(_configs[0].sensorName) - 1);
    _configs[0].relayPin = PIN_RELAY_ZONE_1;
    _configs[0].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[0].hysteresis = DEFAULT_HYSTERESIS;
    _configs[0].enabled = true;

    // Default Zone 2
    _configs[1].id = 2;
    strncpy(_configs[1].name, "Camera Copilului", sizeof(_configs[1].name) - 1);
    strncpy(_configs[1].sensorMac, "A4:C1:38:84:A1:E8", sizeof(_configs[1].sensorMac) - 1);
    strncpy(_configs[1].sensorName, "Kids Room", sizeof(_configs[1].sensorName) - 1);
    _configs[1].relayPin = PIN_RELAY_ZONE_2;
    _configs[1].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[1].hysteresis = DEFAULT_HYSTERESIS;
    _configs[1].enabled = true;

    // Default Zone 3
    _configs[2].id = 3;
    strncpy(_configs[2].name, "Dormitor Matrimonial", sizeof(_configs[2].name) - 1);
    strncpy(_configs[2].sensorMac, "", sizeof(_configs[2].sensorMac) - 1);
    strncpy(_configs[2].sensorName, "Matrimonial", sizeof(_configs[2].sensorName) - 1);
    _configs[2].relayPin = PIN_RELAY_ZONE_3;
    _configs[2].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[2].hysteresis = DEFAULT_HYSTERESIS;
    _configs[2].enabled = true;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].state = ZONE_IDLE;
        _statuses[i].currentTemp = 0.0f;
        _statuses[i].sensorValid = false;
        _statuses[i].sensorFault = false;
        _statuses[i].frostActive = false;
        _statuses[i].demand = false;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = 0;
    }
}

void ThermalZoneManager::loadFromPreferences() {
    Preferences prefs;
    if (!prefs.begin("zones", true)) {
        return; // Use constructor defaults
    }

    if (!prefs.isKey("z_init")) {
        prefs.end();
        saveToPreferences(); // Save defaults
        return;
    }

    _autoMode = prefs.getBool("autoMode", true);
    _pumpControlEnabled = prefs.getBool("pump_en", true);
    _actuatorNormallyClosed = prefs.getBool("act_nc", true);
    _freezeProtectionTemp = prefs.getFloat("frz_temp", 12.0f);
    _lastAntiSeizeEpoch = prefs.getUInt("as_epoch", 0);
    _sensorTimeoutSec = prefs.getUInt("s_tout", 600UL);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        prefs.getString((p + "name").c_str(), _configs[i].name, sizeof(_configs[i].name));
        prefs.getString((p + "mac").c_str(), _configs[i].sensorMac, sizeof(_configs[i].sensorMac));
        prefs.getString((p + "sname").c_str(), _configs[i].sensorName, sizeof(_configs[i].sensorName));
        _configs[i].relayPin = prefs.getUChar((p + "pin").c_str(), _configs[i].relayPin);
        _configs[i].targetTemp = prefs.getFloat((p + "target").c_str(), _configs[i].targetTemp);
        _configs[i].hysteresis = prefs.getFloat((p + "hyst").c_str(), _configs[i].hysteresis);
        _configs[i].enabled = prefs.getBool((p + "en").c_str(), _configs[i].enabled);
    }
    prefs.end();

    // Ensure Zone 3 is always Dormitor Matrimonial (never Baie mare)
    if (strcmp(_configs[2].name, "Baie mare") == 0 || strlen(_configs[2].name) == 0) {
        strncpy(_configs[2].name, "Dormitor Matrimonial", sizeof(_configs[2].name) - 1);
        _configs[2].name[sizeof(_configs[2].name) - 1] = '\0';
        saveToPreferences();
    }

    Serial.println(F("[Thermal] Zone configs, pump settings, and actuator type loaded from NVS."));
}

void ThermalZoneManager::saveToPreferences() {
    Preferences prefs;
    if (!prefs.begin("zones", false)) {
        netConsole.println(F("[Thermal] ERROR: Failed to open NVS namespace 'zones' for writing!"));
        return;
    }

    prefs.putBool("z_init", true);
    prefs.putBool("autoMode", _autoMode);
    prefs.putBool("pump_en", _pumpControlEnabled);
    prefs.putBool("act_nc", _actuatorNormallyClosed);
    prefs.putFloat("frz_temp", _freezeProtectionTemp);
    prefs.putUInt("as_epoch", _lastAntiSeizeEpoch);
    prefs.putUInt("s_tout", _sensorTimeoutSec);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        prefs.putString((p + "name").c_str(), _configs[i].name);
        prefs.putString((p + "mac").c_str(), _configs[i].sensorMac);
        prefs.putString((p + "sname").c_str(), _configs[i].sensorName);
        prefs.putUChar((p + "pin").c_str(), _configs[i].relayPin);
        prefs.putFloat((p + "target").c_str(), _configs[i].targetTemp);
        prefs.putFloat((p + "hyst").c_str(), _configs[i].hysteresis);
        prefs.putBool((p + "en").c_str(), _configs[i].enabled);
    }
    prefs.end();
    Serial.println(F("[Thermal] Zone configs successfully persisted to NVS."));
}

void ThermalZoneManager::loadSchedulePreferences() {
    Preferences prefs;
    if (!prefs.begin("sched", true)) {
        return;
    }
    if (prefs.isKey("cfg")) {
        BioScheduleConfig loaded;
        size_t len = prefs.getBytes("cfg", &loaded, sizeof(loaded));
        if (len == sizeof(loaded)) {
            _scheduleConfig = loaded;
            Serial.println(F("[Thermal] Smart Bio-Schedule loaded from NVS."));
        }
    }
    if (prefs.isKey("act_preset")) {
        String act = prefs.getString("act_preset", "default");
        strncpy(_activePresetId, act.c_str(), sizeof(_activePresetId) - 1);
        _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    } else {
        strncpy(_activePresetId, "default", sizeof(_activePresetId));
    }
    prefs.end();
}

void ThermalZoneManager::saveSchedulePreferences() {
    Preferences prefs;
    if (!prefs.begin("sched", false)) {
        netConsole.println(F("[Thermal] ERROR: Failed to open NVS namespace 'sched' for writing!"));
        return;
    }
    // Only write if changed to avoid NVS flash wear
    BioScheduleConfig currentSaved;
    size_t len = prefs.getBytes("cfg", &currentSaved, sizeof(currentSaved));
    if (len != sizeof(currentSaved) || memcmp(&currentSaved, &_scheduleConfig, sizeof(_scheduleConfig)) != 0) {
        prefs.putBytes("cfg", &_scheduleConfig, sizeof(_scheduleConfig));
        Serial.println(F("[Thermal] Smart Bio-Schedule persisted to NVS."));
    }
    String oldPreset = prefs.getString("act_preset", "");
    if (oldPreset != _activePresetId) {
        prefs.putString("act_preset", _activePresetId);
    }
    prefs.end();
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active) {
    if (pin == PIN_RELAY_SPARE) {
        // Circulation pump is directly active HIGH (runs when energized)
        digitalWrite(pin, active ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    } else {
        // Thermal zone actuator:
        // NC (Normally Closed): active (open valve) -> energize relay (HIGH)
        // NO (Normally Open):   active (open valve) -> de-energize relay (LOW)
        bool level = _actuatorNormallyClosed ? active : !active;
        digitalWrite(pin, level ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    }
}

void ThermalZoneManager::begin() {
    loadFromPreferences();
    loadSchedulePreferences();
    evaluateSchedule(millis());

    _bootWarmupActive = true;
    _bootWarmupStartMs = millis();

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        pinMode(_configs[i].relayPin, OUTPUT);
        applyRelay(_configs[i].relayPin, false);
        _statuses[i].state = _configs[i].enabled ? ZONE_WARMUP : ZONE_DISABLED;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = millis();
    }

    pinMode(PIN_RELAY_SPARE, OUTPUT);
    applyRelay(PIN_RELAY_SPARE, false);
    _pumpActive = false;

    Serial.printf("[Thermal] Initialized: 3 zones, Actuators: %s, Pump Relay 4: %s\n",
        _actuatorNormallyClosed ? "NC (Normally Closed)" : "NO (Normally Open)",
        _pumpControlEnabled ? "ENABLED" : "DISABLED");
}

const ThermalZoneConfig& ThermalZoneManager::getConfig(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _configs[0];
    return _configs[index];
}

const ThermalZoneStatus& ThermalZoneManager::getStatus(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _statuses[0];
    return _statuses[index];
}

void ThermalZoneManager::setPumpControlEnabled(bool enabled) {
    if (_pumpControlEnabled == enabled) return;
    _pumpControlEnabled = enabled;
    if (!enabled && _pumpActive) {
        applyRelay(PIN_RELAY_SPARE, false);
        _pumpActive = false;
    }
    saveToPreferences();
    netConsole.printf("[Thermal] Circulation pump control via Relay 4 %s\n", enabled ? "ENABLED" : "DISABLED");
}

void ThermalZoneManager::setActuatorNormallyClosed(bool nc) {
    if (_actuatorNormallyClosed == nc) return;
    _actuatorNormallyClosed = nc;
    saveToPreferences();
    // Refresh all zone relays with new logic polarity
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled) {
            applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged);
        }
    }
    netConsole.printf("[Thermal] Actuator type set to: %s\n", nc ? "NC (Normally Closed)" : "NO (Normally Open)");
}

void ThermalZoneManager::setFreezeProtectionTemp(float temp) {
    if (temp >= 5.0f && temp <= 16.0f) {
        if (fabs(_freezeProtectionTemp - temp) < 0.05f) return;
        _freezeProtectionTemp = temp;
        saveToPreferences();
        netConsole.printf("[Thermal] Frost protection threshold set to: %.1f C\n", temp);
    }
}

void ThermalZoneManager::setSensorTimeoutSec(uint32_t sec) {
    if (sec < 60) sec = 60;          // Min 1 minute
    if (sec > 86400) sec = 86400;    // Max 24 hours
    if (_sensorTimeoutSec == sec) return;
    _sensorTimeoutSec = sec;
    saveToPreferences();
    netConsole.printf("[Thermal] Sensor failsafe timeout set to: %u s\n", _sensorTimeoutSec);
}

void ThermalZoneManager::triggerAntiSeizeExercise() {
    if (_antiSeizeRunning) return;
    _antiSeizeRunning = true;
    _antiSeizeStartMs = millis();
    netConsole.println(F("[Thermal] >>> ANTI-SEIZE EXERCISE STARTED (Pumps & Valves) <<<"));

    // Open all enabled actuators
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled) {
            applyRelay(_configs[i].relayPin, true);
        }
    }
    // Start circulation pump if enabled
    if (_pumpControlEnabled) {
        applyRelay(PIN_RELAY_SPARE, true);
        _pumpActive = true;
    }
}

void ThermalZoneManager::setTargetTemperature(size_t index, float target) {
    if (index < MAX_THERMAL_ZONES) {
        if (target < 15.0f) target = 15.0f;
        if (target > 30.0f) target = 30.0f;
        if (fabsf(_configs[index].targetTemp - target) > 0.05f) {
            _configs[index].targetTemp = target;
            saveToPreferences();
        }

        // If Bio-Schedule is active, also adjust the current active anchor's target
        // so the user's manual bump takes immediate effect in the thermodynamic loop!
        if (_scheduleConfig.enabled && timeAndWeather.isTimeSynced()) {
            bool weekend = timeAndWeather.isWeekend();
            BioAnchorId targetAnchor = _preHeatActive ? _nextAnchor : _activeAnchor;
            setAnchorTarget(weekend, targetAnchor, index, target);
            if (_preHeatActive && _activeAnchor != targetAnchor) {
                setAnchorTarget(weekend, _activeAnchor, index, target);
            }
        }

        netConsole.printf("[Thermal] Zone %u (%s) new target temp: %.1f C\n",
            _configs[index].id, _configs[index].name, target);
    }
}

void ThermalZoneManager::setHysteresis(size_t index, float hyst) {
    if (index < MAX_THERMAL_ZONES) {
        if (hyst < 0.1f) hyst = 0.1f;
        if (hyst > 2.0f) hyst = 2.0f;
        _configs[index].hysteresis = hyst;
        saveToPreferences();
    }
}

void ThermalZoneManager::setZoneEnabled(size_t index, bool enabled) {
    if (index < MAX_THERMAL_ZONES) {
        _configs[index].enabled = enabled;
        if (!enabled) {
            _statuses[index].demand = false;
            _statuses[index].state = ZONE_DISABLED;
            if (_statuses[index].relayEngaged) {
                applyRelay(_configs[index].relayPin, false);
                _statuses[index].relayEngaged = false;
            }
        } else {
            if (_statuses[index].state == ZONE_DISABLED) {
                _statuses[index].state = ZONE_IDLE;
            }
        }
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) %s\n",
            _configs[index].id, _configs[index].name, enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setZoneName(size_t index, const char* name) {
    if (index < MAX_THERMAL_ZONES && name) {
        strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        _configs[index].name[sizeof(_configs[index].name) - 1] = '\0';
        saveToPreferences();
    }
}

void ThermalZoneManager::setSensorName(size_t index, const char* sensorName) {
    if (index < MAX_THERMAL_ZONES && sensorName) {
        strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        _configs[index].sensorName[sizeof(_configs[index].sensorName) - 1] = '\0';
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor name '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorName);
    }
}

void ThermalZoneManager::setSensorMac(size_t index, const char* sensorMac) {
    if (index < MAX_THERMAL_ZONES && sensorMac) {
        strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        _configs[index].sensorMac[sizeof(_configs[index].sensorMac) - 1] = '\0';
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor MAC '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac);
    }
}

void ThermalZoneManager::setRelayPin(size_t index, uint8_t pin) {
    if (index < MAX_THERMAL_ZONES) {
        _configs[index].relayPin = pin;
        pinMode(pin, OUTPUT);
        applyRelay(pin, false);
        saveToPreferences();
    }
}

void ThermalZoneManager::mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, float hysteresis, bool enabled) {
    if (index < MAX_THERMAL_ZONES) {
        if (name && strlen(name) > 0) strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        if (sensorMac) strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        if (sensorName) strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        if (relayPin > 0) {
            _configs[index].relayPin = relayPin;
            pinMode(relayPin, OUTPUT);
        }
        if (targetTemp >= 15.0f && targetTemp <= 30.0f) _configs[index].targetTemp = targetTemp;
        if (hysteresis >= 0.1f && hysteresis <= 2.0f) _configs[index].hysteresis = hysteresis;
        _configs[index].enabled = enabled;
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u mapped: '%s' | MAC: %s | Pin: %u | Target: %.1f C\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac, _configs[index].relayPin, _configs[index].targetTemp);
    }
}

void ThermalZoneManager::setAutoMode(bool autoMode) {
    _autoMode = autoMode;
    saveToPreferences();
    netConsole.printf("[Thermal] Operation mode switched to: %s\n", _autoMode ? "AUTO" : "MANUAL");
}

void ThermalZoneManager::setRelayManual(size_t index, bool on) {
    if (index < MAX_THERMAL_ZONES && !_autoMode) {
        _statuses[index].relayEngaged = on;
        applyRelay(_configs[index].relayPin, on);
        _statuses[index].state = on ? ZONE_HEATING : ZONE_IDLE;
        netConsole.printf("[Thermal][MANUAL] Relay Zone %u switched to %s\n", _configs[index].id, on ? "ON" : "OFF");
    }
}

void ThermalZoneManager::setPumpManual(bool on) {
    if (!_autoMode) {
        _pumpActive = on;
        applyRelay(PIN_RELAY_SPARE, on);
        netConsole.printf("[Thermal][MANUAL] Circulation Pump Relay 4 switched to %s\n", on ? "ON" : "OFF");
    }
}

void ThermalZoneManager::setBoilerManual(BoilerRadioTrigger& boiler, bool on) {
    if (!_autoMode) {
        if (on) {
            if (boiler.sendBoilerOn()) {
                _lastBoilerOnTime = millis();
            }
        } else {
            if (boiler.sendBoilerOff()) {
                _lastBoilerOffTime = millis();
            }
        }
    }
}

const char* ThermalZoneManager::getStateStr(ZoneState state) const {
    switch (state) {
        case ZONE_DISABLED:      return "Disabled";
        case ZONE_IDLE:          return "Idle (Temp OK)";
        case ZONE_WARMING_UP:    return "Actuator Warming Up";
        case ZONE_HEATING:       return "Heating Active";
        case ZONE_FAULT_TIMEOUT: return "Sensor Timeout (Failsafe OFF)";
        case ZONE_FROST_GUARD:   return "Frost Protection Active (<12C)";
        case ZONE_WARMUP:        return "Acquiring Sensor (Warmup)";
        case ZONE_WINDOW_PAUSE:  return "Window Pause (Open Window Detected)";
        default:                 return "Unknown";
    }
}

const char* ThermalZoneManager::getAnchorName(BioAnchorId id) const {
    switch (id) {
        case BIO_ANCHOR_SLEEP: return "SLEEP";
        case BIO_ANCHOR_WAKE:  return "WAKE";
        case BIO_ANCHOR_AWAY:  return "AWAY";
        case BIO_ANCHOR_HOME:  return "HOME";
        default:               return "UNKNOWN";
    }
}

BioScheduleConfig ThermalZoneManager::getFactoryDefaultSchedule() {
    BioScheduleConfig cfg;
    cfg.enabled = true;
    cfg.preHeatEnabled = true;
    cfg.windowDetectionEnabled = true;

    // Weekday defaults (Luni - Vineri: Standard / Confort)
    // Sleep (20:00): 21.0C, 22.0C, 22.0C
    cfg.weekday[BIO_ANCHOR_SLEEP].hour = 20;
    cfg.weekday[BIO_ANCHOR_SLEEP].minute = 0;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[0] = 21.0f;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[1] = 22.0f;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[2] = 22.0f;

    // Wake (07:00): 23.0C, 22.5C, 22.5C
    cfg.weekday[BIO_ANCHOR_WAKE].hour = 7;
    cfg.weekday[BIO_ANCHOR_WAKE].minute = 0;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[0] = 23.0f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[1] = 22.5f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[2] = 22.5f;

    // Away (07:00): 21.0C, 21.0C, 21.0C
    cfg.weekday[BIO_ANCHOR_AWAY].hour = 7;
    cfg.weekday[BIO_ANCHOR_AWAY].minute = 0;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[0] = 21.0f;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[1] = 21.0f;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[2] = 21.0f;

    // Home (17:00): 23.0C, 22.5C, 22.5C
    cfg.weekday[BIO_ANCHOR_HOME].hour = 17;
    cfg.weekday[BIO_ANCHOR_HOME].minute = 0;
    cfg.weekday[BIO_ANCHOR_HOME].targets[0] = 23.0f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[1] = 22.5f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[2] = 22.5f;

    // Weekend defaults (Sambata - Duminica: Standard / Confort)
    // Sleep (21:00): 21.0C, 22.0C, 22.0C
    cfg.weekend[BIO_ANCHOR_SLEEP].hour = 21;
    cfg.weekend[BIO_ANCHOR_SLEEP].minute = 0;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[0] = 21.0f;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[1] = 22.0f;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[2] = 22.0f;

    // Wake (07:00): 23.0C, 22.5C, 22.5C
    cfg.weekend[BIO_ANCHOR_WAKE].hour = 7;
    cfg.weekend[BIO_ANCHOR_WAKE].minute = 0;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[0] = 23.0f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[1] = 22.5f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[2] = 22.5f;

    // Away (12:00): 21.0C, 21.0C, 21.0C
    cfg.weekend[BIO_ANCHOR_AWAY].hour = 12;
    cfg.weekend[BIO_ANCHOR_AWAY].minute = 0;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[0] = 21.0f;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[1] = 21.0f;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[2] = 21.0f;

    // Home (16:00): 23.0C, 22.5C, 22.5C
    cfg.weekend[BIO_ANCHOR_HOME].hour = 16;
    cfg.weekend[BIO_ANCHOR_HOME].minute = 0;
    cfg.weekend[BIO_ANCHOR_HOME].targets[0] = 23.0f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[1] = 22.5f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[2] = 22.5f;

    return cfg;
}

BioScheduleConfig ThermalZoneManager::getFactoryEcoSchedule() {
    BioScheduleConfig cfg = getFactoryDefaultSchedule();
    for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
        cfg.weekday[a].targets[0] = 18.0f;
        cfg.weekday[a].targets[1] = 18.5f;
        cfg.weekday[a].targets[2] = 18.0f;
        cfg.weekend[a].targets[0] = 18.0f;
        cfg.weekend[a].targets[1] = 18.5f;
        cfg.weekend[a].targets[2] = 18.0f;
    }
    cfg.weekday[BIO_ANCHOR_WAKE].targets[0] = 19.0f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[1] = 19.5f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[2] = 19.0f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[0] = 19.0f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[1] = 19.5f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[2] = 19.0f;

    cfg.weekend[BIO_ANCHOR_WAKE].targets[0] = 19.0f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[1] = 19.5f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[2] = 19.0f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[0] = 19.0f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[1] = 19.5f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[2] = 19.0f;
    return cfg;
}

BioScheduleConfig ThermalZoneManager::getFactoryWfhSchedule() {
    BioScheduleConfig cfg;
    cfg.enabled = true;
    cfg.preHeatEnabled = true;
    cfg.windowDetectionEnabled = true;

    // Weekday defaults (Luni - Vineri: Home Office)
    // Sleep (22:30): 21.0C, 22.0C, 22.0C
    cfg.weekday[BIO_ANCHOR_SLEEP].hour = 22;
    cfg.weekday[BIO_ANCHOR_SLEEP].minute = 30;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[0] = 21.0f;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[1] = 22.0f;
    cfg.weekday[BIO_ANCHOR_SLEEP].targets[2] = 22.0f;

    // Wake (06:30): 23.0C, 22.5C, 22.5C
    cfg.weekday[BIO_ANCHOR_WAKE].hour = 6;
    cfg.weekday[BIO_ANCHOR_WAKE].minute = 30;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[0] = 23.0f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[1] = 22.5f;
    cfg.weekday[BIO_ANCHOR_WAKE].targets[2] = 22.5f;

    // Home Office Day (08:30): 22.5C, 23.0C, 21.0C
    cfg.weekday[BIO_ANCHOR_AWAY].hour = 8;
    cfg.weekday[BIO_ANCHOR_AWAY].minute = 30;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[0] = 22.5f;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[1] = 23.0f;
    cfg.weekday[BIO_ANCHOR_AWAY].targets[2] = 21.0f;

    // Home (17:00): 23.0C, 22.5C, 22.5C
    cfg.weekday[BIO_ANCHOR_HOME].hour = 17;
    cfg.weekday[BIO_ANCHOR_HOME].minute = 0;
    cfg.weekday[BIO_ANCHOR_HOME].targets[0] = 23.0f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[1] = 22.5f;
    cfg.weekday[BIO_ANCHOR_HOME].targets[2] = 22.5f;

    // Weekend defaults (Sambata - Duminica: Home Office)
    // Sleep (23:00): 21.0C, 22.0C, 21.5C
    cfg.weekend[BIO_ANCHOR_SLEEP].hour = 23;
    cfg.weekend[BIO_ANCHOR_SLEEP].minute = 0;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[0] = 21.0f;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[1] = 22.0f;
    cfg.weekend[BIO_ANCHOR_SLEEP].targets[2] = 21.5f;

    // Wake (08:30): 23.0C, 22.5C, 22.5C
    cfg.weekend[BIO_ANCHOR_WAKE].hour = 8;
    cfg.weekend[BIO_ANCHOR_WAKE].minute = 30;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[0] = 23.0f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[1] = 22.5f;
    cfg.weekend[BIO_ANCHOR_WAKE].targets[2] = 22.5f;

    // Home Office Day (12:00): 23.0C, 21.5C, 21.5C
    cfg.weekend[BIO_ANCHOR_AWAY].hour = 12;
    cfg.weekend[BIO_ANCHOR_AWAY].minute = 0;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[0] = 23.0f;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[1] = 21.5f;
    cfg.weekend[BIO_ANCHOR_AWAY].targets[2] = 21.5f;

    // Home (16:00): 23.0C, 22.5C, 22.5C
    cfg.weekend[BIO_ANCHOR_HOME].hour = 16;
    cfg.weekend[BIO_ANCHOR_HOME].minute = 0;
    cfg.weekend[BIO_ANCHOR_HOME].targets[0] = 23.0f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[1] = 22.5f;
    cfg.weekend[BIO_ANCHOR_HOME].targets[2] = 22.5f;

    return cfg;
}

BioAnchorId ThermalZoneManager::parseAnchorId(const char* name) {
    if (!name) return BIO_ANCHORS_COUNT;
    if (strcasecmp(name, "sleep") == 0 || strcmp(name, "0") == 0) return BIO_ANCHOR_SLEEP;
    if (strcasecmp(name, "wake") == 0  || strcmp(name, "1") == 0) return BIO_ANCHOR_WAKE;
    if (strcasecmp(name, "away") == 0  || strcmp(name, "2") == 0) return BIO_ANCHOR_AWAY;
    if (strcasecmp(name, "home") == 0  || strcmp(name, "3") == 0) return BIO_ANCHOR_HOME;
    return BIO_ANCHORS_COUNT;
}

bool ThermalZoneManager::updateAnchor(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]) {
    if (anchor >= BIO_ANCHORS_COUNT || hour > 23 || minute > 59) return false;
    BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    profile[anchor].hour = hour;
    profile[anchor].minute = minute;
    if (targets) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (targets[i] >= 5.0f && targets[i] <= 35.0f) {
                profile[anchor].targets[i] = targets[i];
            }
        }
    }
    strncpy(_activePresetId, "custom", sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    evaluateSchedule(millis());

    // Immediate sync: if active anchor is being edited, update active zone targets immediately
    bool isCurrentAnchor = (anchor == _activeAnchor && weekend == timeAndWeather.isWeekend());
    if (isCurrentAnchor && targets) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            _configs[i].targetTemp = profile[anchor].targets[i];
        }
        saveToPreferences();
    }

    netConsole.printf("[Thermal] Updated anchor %s (%s): %02u:%02u [%.1f, %.1f, %.1f]\n",
                      getAnchorName(anchor), weekend ? "Weekend" : "Weekday",
                      hour, minute, profile[anchor].targets[0], profile[anchor].targets[1], profile[anchor].targets[2]);
    return true;
}

bool ThermalZoneManager::setAnchorTime(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute) {
    return updateAnchor(weekend, anchor, hour, minute, nullptr);
}

bool ThermalZoneManager::setAnchorTargets(bool weekend, BioAnchorId anchor, const float targets[MAX_THERMAL_ZONES]) {
    if (anchor >= BIO_ANCHORS_COUNT) return false;
    BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    return updateAnchor(weekend, anchor, profile[anchor].hour, profile[anchor].minute, targets);
}

bool ThermalZoneManager::setAnchorTarget(bool weekend, BioAnchorId anchor, size_t zoneIndex, float target) {
    if (anchor >= BIO_ANCHORS_COUNT || zoneIndex >= MAX_THERMAL_ZONES || target < 5.0f || target > 35.0f) return false;
    BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    profile[anchor].targets[zoneIndex] = target;
    strncpy(_activePresetId, "custom", sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Anchor %s (%s) Zone %u set to %.1f C\n",
                      getAnchorName(anchor), weekend ? "Weekend" : "Weekday", (unsigned int)(zoneIndex + 1), target);
    return true;
}

String ThermalZoneManager::getPresetsJson() const {
    String json;
    json.reserve(1024);
    json = "{";
    json += "\"status\":\"ok\",";
    json += "\"activePreset\":\"" + String(_activePresetId) + "\",";
    json += "\"presets\":[";

    Preferences prefs;
    bool prefsOpen = prefs.begin("presets", true);

    struct FactoryPresetDef {
        const char* id;
        const char* defaultName;
    };
    FactoryPresetDef factories[] = {
        {"default", "Standard / Confort"},
        {"eco", "Vacan\xC8\x9B\xC4\x83 / Eco"},
        {"wfh", "Home Office"}
    };

    for (size_t i = 0; i < 3; i++) {
        if (i > 0) json += ",";
        char keyCfg[16], keyName[16];
        snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", factories[i].id);
        snprintf(keyName, sizeof(keyName), "%s_name", factories[i].id);
        bool saved = prefsOpen && prefs.isKey(keyCfg);
        String name = (saved && prefs.isKey(keyName)) ? prefs.getString(keyName, factories[i].defaultName) : String(factories[i].defaultName);
        json += "{\"id\":\"" + String(factories[i].id) + "\",\"name\":\"" + name + "\",\"type\":\"factory\",\"saved\":" + (saved ? "true" : "false") + "}";
    }

    const char* slots[] = {"custom1", "custom2", "custom3", "custom4"};
    for (int i = 0; i < 4; i++) {
        char keyCfg[16], keyName[16];
        snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", slots[i]);
        snprintf(keyName, sizeof(keyName), "%s_name", slots[i]);
        bool saved = prefsOpen && prefs.isKey(keyCfg);
        String name = (saved && prefs.isKey(keyName)) ? prefs.getString(keyName, slots[i]) : (String("Slot Liber (") + slots[i] + ")");
        json += ",{\"id\":\"" + String(slots[i]) + "\",\"name\":\"" + name + "\",\"type\":\"user\",\"saved\":" + (saved ? "true" : "false") + "}";
    }

    if (prefsOpen) prefs.end();
    json += "]}";
    return json;
}

bool ThermalZoneManager::applyPreset(const char* presetId) {
    if (!presetId || strlen(presetId) == 0) return false;
    BioScheduleConfig targetCfg;
    bool found = false;

    // Check if customized version exists in NVS for this preset (including default, eco, wfh, custom1..custom4)
    Preferences prefs;
    if (prefs.begin("presets", true)) {
        char keyCfg[16];
        snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", presetId);
        if (prefs.isKey(keyCfg)) {
            size_t len = prefs.getBytes(keyCfg, &targetCfg, sizeof(targetCfg));
            if (len == sizeof(targetCfg)) {
                found = true;
            }
        }
        prefs.end();
    }

    // If not found in NVS, fallback to factory schedule if it's one of the built-ins
    if (!found) {
        if (strcasecmp(presetId, "default") == 0) {
            targetCfg = getFactoryDefaultSchedule();
            found = true;
        } else if (strcasecmp(presetId, "eco") == 0) {
            targetCfg = getFactoryEcoSchedule();
            found = true;
        } else if (strcasecmp(presetId, "wfh") == 0) {
            targetCfg = getFactoryWfhSchedule();
            found = true;
        }
    }

    if (!found) return false;

    bool currentEn = _scheduleConfig.enabled;
    _scheduleConfig = targetCfg;
    _scheduleConfig.enabled = currentEn;
    strncpy(_activePresetId, presetId, sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    evaluateSchedule(millis());

    // Synchronize individual zone targetTemps to match the new preset's active anchor targets
    bool weekend = timeAndWeather.isTimeSynced() ? timeAndWeather.isWeekend() : false;
    const BioAnchorConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    BioAnchorId anc = _activeAnchor;
    if (anc >= BIO_ANCHORS_COUNT) anc = BIO_ANCHOR_HOME;
    bool zChanged = false;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        float newT = prof[anc].targets[i];
        if (fabsf(_configs[i].targetTemp - newT) > 0.05f) {
            _configs[i].targetTemp = newT;
            zChanged = true;
        }
    }
    if (zChanged) {
        saveToPreferences();
    }

    netConsole.printf("[Thermal] Applied Bio-Schedule preset: '%s'\n", presetId);
    return true;
}

bool ThermalZoneManager::savePreset(const char* presetId, const char* name) {
    if (!presetId || strlen(presetId) == 0) return false;

    // Permit valid preset slots: default, eco, wfh, custom1..custom4
    if (strcasecmp(presetId, "default") != 0 &&
        strcasecmp(presetId, "eco") != 0 &&
        strcasecmp(presetId, "wfh") != 0 &&
        strcasecmp(presetId, "custom1") != 0 &&
        strcasecmp(presetId, "custom2") != 0 &&
        strcasecmp(presetId, "custom3") != 0 &&
        strcasecmp(presetId, "custom4") != 0) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin("presets", false)) return false;

    char keyCfg[16];
    char keyName[16];
    snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", presetId);
    snprintf(keyName, sizeof(keyName), "%s_name", presetId);

    prefs.putBytes(keyCfg, &_scheduleConfig, sizeof(_scheduleConfig));
    if (name && strlen(name) > 0) {
        prefs.putString(keyName, name);
    } else {
        prefs.putString(keyName, presetId);
    }
    prefs.end();

    strncpy(_activePresetId, presetId, sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    netConsole.printf("[Thermal] Saved active Bio-Schedule to preset '%s' (%s)\n", presetId, name ? name : presetId);
    return true;
}

bool ThermalZoneManager::resetPreset(const char* presetId) {
    if (!presetId || strlen(presetId) == 0) return false;

    Preferences prefs;
    if (prefs.begin("presets", false)) {
        char keyCfg[16];
        char keyName[16];
        snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", presetId);
        snprintf(keyName, sizeof(keyName), "%s_name", presetId);
        prefs.remove(keyCfg);
        prefs.remove(keyName);
        prefs.end();
    }

    // If currently active preset was reset, reload it (which falls back to factory default if built-in, or default if custom)
    if (strcasecmp(_activePresetId, presetId) == 0) {
        if (strcasecmp(presetId, "default") == 0 || strcasecmp(presetId, "eco") == 0 || strcasecmp(presetId, "wfh") == 0) {
            applyPreset(presetId);
        } else {
            resetScheduleToDefault();
        }
    }

    netConsole.printf("[Thermal] Reset preset '%s' (NVS custom override removed)\n", presetId);
    return true;
}

void ThermalZoneManager::resetScheduleToDefault() {
    bool en = _scheduleConfig.enabled;
    _scheduleConfig = getFactoryDefaultSchedule();
    _scheduleConfig.enabled = en;
    strncpy(_activePresetId, "default", sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    evaluateSchedule(millis());

    bool weekend = timeAndWeather.isTimeSynced() ? timeAndWeather.isWeekend() : false;
    const BioAnchorConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    BioAnchorId anc = _activeAnchor;
    if (anc >= BIO_ANCHORS_COUNT) anc = BIO_ANCHOR_HOME;
    bool zChanged = false;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        float newT = prof[anc].targets[i];
        if (fabsf(_configs[i].targetTemp - newT) > 0.05f) {
            _configs[i].targetTemp = newT;
            zChanged = true;
        }
    }
    if (zChanged) {
        saveToPreferences();
    }

    netConsole.println(F("[Thermal] Bio-Schedule reset to factory DEFAULT preset."));
}

void ThermalZoneManager::setScheduleConfig(const BioScheduleConfig& cfg) {
    _scheduleConfig = cfg;
    saveSchedulePreferences();
    evaluateSchedule(millis());
}

void ThermalZoneManager::setScheduleEnabled(bool enabled) {
    if (_scheduleConfig.enabled != enabled) {
        _scheduleConfig.enabled = enabled;
        saveSchedulePreferences();
        evaluateSchedule(millis());
        if (enabled && timeAndWeather.isTimeSynced()) {
            bool weekend = timeAndWeather.isWeekend();
            const BioAnchorConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
            BioAnchorId anc = _activeAnchor;
            if (anc >= BIO_ANCHORS_COUNT) anc = BIO_ANCHOR_HOME;
            bool zChanged = false;
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                float newT = prof[anc].targets[i];
                if (fabsf(_configs[i].targetTemp - newT) > 0.05f) {
                    _configs[i].targetTemp = newT;
                    zChanged = true;
                }
            }
            if (zChanged) {
                saveToPreferences();
            }
        }
        netConsole.printf("[Thermal] Smart Bio-Schedule %s\n", enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setPreHeatEnabled(bool enabled) {
    if (_scheduleConfig.preHeatEnabled != enabled) {
        _scheduleConfig.preHeatEnabled = enabled;
        saveSchedulePreferences();
        evaluateSchedule(millis());
        netConsole.printf("[Thermal] Predictive Pre-Heat %s\n", enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setWindowDetectionEnabled(bool enabled) {
    if (_scheduleConfig.windowDetectionEnabled != enabled) {
        _scheduleConfig.windowDetectionEnabled = enabled;
        saveSchedulePreferences();
        netConsole.printf("[Thermal] Open Window Detection %s\n", enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::evaluateSchedule(uint32_t now) {
    if (!_scheduleConfig.enabled || !timeAndWeather.isTimeSynced()) {
        _activeAnchor = BIO_ANCHOR_HOME;
        _nextAnchor = BIO_ANCHOR_SLEEP;
        _minsToNextAnchor = 0;
        _preHeatActive = false;
        return;
    }

    int curHour = timeAndWeather.getHour();
    int curMin = timeAndWeather.getMinute();
    bool weekend = timeAndWeather.isWeekend();
    const BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;

    uint32_t curM = curHour * 60 + curMin;

    uint32_t mSleep = profile[BIO_ANCHOR_SLEEP].hour * 60 + profile[BIO_ANCHOR_SLEEP].minute;
    uint32_t mWake  = profile[BIO_ANCHOR_WAKE].hour  * 60 + profile[BIO_ANCHOR_WAKE].minute;
    uint32_t mAway  = profile[BIO_ANCHOR_AWAY].hour  * 60 + profile[BIO_ANCHOR_AWAY].minute;
    uint32_t mHome  = profile[BIO_ANCHOR_HOME].hour  * 60 + profile[BIO_ANCHOR_HOME].minute;

    BioAnchorId current = BIO_ANCHOR_HOME;
    BioAnchorId next = BIO_ANCHOR_SLEEP;
    uint32_t minsToNext = 0;

    if (curM >= mSleep || curM < mWake) {
        current = BIO_ANCHOR_SLEEP;
        next = BIO_ANCHOR_WAKE;
        minsToNext = (curM >= mSleep) ? ((1440 - curM) + mWake) : (mWake - curM);
    } else if (curM >= mWake && curM < mAway) {
        current = BIO_ANCHOR_WAKE;
        next = BIO_ANCHOR_AWAY;
        minsToNext = (mAway >= curM) ? (mAway - curM) : 0;
    } else if (curM >= mAway && curM < mHome) {
        current = BIO_ANCHOR_AWAY;
        next = BIO_ANCHOR_HOME;
        minsToNext = (mHome >= curM) ? (mHome - curM) : 0;
    } else { // curM >= mHome && curM < mSleep
        current = BIO_ANCHOR_HOME;
        next = BIO_ANCHOR_SLEEP;
        minsToNext = (mSleep >= curM) ? (mSleep - curM) : 0;
    }

    BioAnchorId prevAnchor = _activeAnchor;
    _activeAnchor = current;
    _nextAnchor = next;
    _minsToNextAnchor = minsToNext;

    if (prevAnchor != _activeAnchor) {
        netConsole.printf("[Thermal][SCHEDULE] Bio-Schedule transition: %s -> %s (Profile: %s, Time: %02d:%02d)\n",
                          getAnchorName(prevAnchor), getAnchorName(_activeAnchor),
                          weekend ? "Weekend" : "Weekday", curHour, curMin);
    }

    // Predictive Pre-Heat evaluation
    bool shouldPreHeat = false;
    if (_scheduleConfig.preHeatEnabled && (next == BIO_ANCHOR_HOME || next == BIO_ANCHOR_WAKE)) {
        float tout = timeAndWeather.getOutdoorTemperature();
        float k = 1.0f;
        if (timeAndWeather.isWeatherValid()) {
            k = 1.0f + (15.0f - tout) * 0.03f;
            if (k < 0.7f) k = 0.7f;
            if (k > 1.8f) k = 1.8f;
        }

        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (!_configs[i].enabled) continue;
            float targetActive = profile[current].targets[i];
            float targetNext = profile[next].targets[i];
            float deltaT = targetNext - targetActive;
            if (deltaT > 0.4f) {
                int advanceMins = (int)(deltaT * 40.0f * k);
                if (advanceMins < 15) advanceMins = 15;
                if (advanceMins > 150) advanceMins = 150;
                if (minsToNext <= (uint32_t)advanceMins) {
                    shouldPreHeat = true;
                    break;
                }
            }
        }
    }

    if (shouldPreHeat != _preHeatActive) {
        _preHeatActive = shouldPreHeat;
        if (_preHeatActive) {
            netConsole.printf("[Thermal][PRE-HEAT] Anticipating %s anchor (%u min remaining). Pre-heating active!\n",
                              getAnchorName(next), minsToNext);
        } else {
            netConsole.printf("[Thermal][PRE-HEAT] Pre-heat cycle ended. Transitioned to active anchor %s.\n",
                              getAnchorName(_activeAnchor));
        }
    }
}

float ThermalZoneManager::getEffectiveTargetTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return DEFAULT_SETPOINT_TEMP;
    if (!_scheduleConfig.enabled || !timeAndWeather.isTimeSynced()) {
        return _configs[index].targetTemp;
    }
    bool weekend = timeAndWeather.isWeekend();
    const BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    BioAnchorId anchorToUse = _preHeatActive ? _nextAnchor : _activeAnchor;
    return profile[anchorToUse].targets[index];
}

void ThermalZoneManager::evaluateWindowDetection(size_t index, float currentTemp, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return;
    if (!_scheduleConfig.windowDetectionEnabled) {
        _windowStates[index].pauseActive = false;
        return;
    }

    if (!_windowStates[index].pauseActive) {
        if (_windowStates[index].baselineMs == 0 || (now - _windowStates[index].baselineMs > 300000UL)) {
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
        } else if (currentTemp > _windowStates[index].baselineTemp) {
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
        } else if ((_windowStates[index].baselineTemp - currentTemp) >= 1.2f) {
            _windowStates[index].pauseActive = true;
            _windowStates[index].pauseStartMs = now;
            _windowStates[index].pauseMinTemp = currentTemp;
            netConsole.printf("[Thermal][WINDOW] Open window detected on Zone %u (%s): temp dropped %.1f \xC2\xB0""C in %us! Suspending heating for 30m.\n",
                              _configs[index].id, _configs[index].name,
                              _windowStates[index].baselineTemp - currentTemp,
                              (now - _windowStates[index].baselineMs) / 1000);
        }
    } else {
        if (currentTemp < _windowStates[index].pauseMinTemp) {
            _windowStates[index].pauseMinTemp = currentTemp;
        }
        bool timeoutExpired = (now - _windowStates[index].pauseStartMs >= 1800000UL); // 30 min
        bool rebounded = (currentTemp >= _windowStates[index].pauseMinTemp + 0.6f);
        if (timeoutExpired || rebounded) {
            _windowStates[index].pauseActive = false;
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
            netConsole.printf("[Thermal][WINDOW] Zone %u (%s) window pause cleared (%s). Resuming normal thermostat loop.\n",
                              _configs[index].id, _configs[index].name,
                              timeoutExpired ? "30 min timeout" : "temp rebounded +0.6 \xC2\xB0""C");
        }
    }
}

bool ThermalZoneManager::isWindowPauseActive(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return false;
    return _windowStates[index].pauseActive;
}

uint32_t ThermalZoneManager::getWindowPauseRemainingSec(size_t index) const {
    if (index >= MAX_THERMAL_ZONES || !_windowStates[index].pauseActive) return 0;
    uint32_t elapsed = millis() - _windowStates[index].pauseStartMs;
    if (elapsed >= 1800000UL) return 0;
    return (1800000UL - elapsed) / 1000;
}

String ThermalZoneManager::getScheduleJson() const {
    String json;
    json.reserve(2048);
    json = "{";
    json += "\"status\":\"ok\",";
    json += "\"enabled\":" + String(_scheduleConfig.enabled ? "true" : "false") + ",";
    json += "\"preHeatEnabled\":" + String(_scheduleConfig.preHeatEnabled ? "true" : "false") + ",";
    json += "\"windowDetectionEnabled\":" + String(_scheduleConfig.windowDetectionEnabled ? "true" : "false") + ",";
    json += "\"activePreset\":\"" + String(_activePresetId) + "\",";
    json += "\"activeAnchor\":\"" + String(getActiveAnchorName()) + "\",";
    json += "\"nextAnchor\":\"" + String(getAnchorName(_nextAnchor)) + "\",";
    json += "\"minsToNextAnchor\":" + String(_minsToNextAnchor) + ",";
    json += "\"preHeatActive\":" + String(_preHeatActive ? "true" : "false") + ",";
    json += "\"isWeekend\":" + String(timeAndWeather.isWeekend() ? "true" : "false") + ",";

    // Zones status array
    json += "\"zones\":[";
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + String(_configs[i].id) + ",";
        json += "\"name\":\"" + String(_configs[i].name) + "\",";
        json += "\"effectiveTarget\":" + String(getEffectiveTargetTemp(i), 1) + ",";
        json += "\"staticTarget\":" + String(_configs[i].targetTemp, 1) + ",";
        json += "\"windowPauseActive\":" + String(isWindowPauseActive(i) ? "true" : "false") + ",";
        json += "\"windowPauseRemainingSec\":" + String(getWindowPauseRemainingSec(i));
        json += "}";
    }
    json += "],";

    // Weekday profile
    const char* anchorKeys[] = {"sleep", "wake", "away", "home"};
    json += "\"weekday\":{";
    for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
        if (a > 0) json += ",";
        json += "\"" + String(anchorKeys[a]) + "\":{";
        json += "\"hour\":" + String(_scheduleConfig.weekday[a].hour) + ",";
        json += "\"minute\":" + String(_scheduleConfig.weekday[a].minute) + ",";
        json += "\"targets\":[" +
                String(_scheduleConfig.weekday[a].targets[0], 1) + "," +
                String(_scheduleConfig.weekday[a].targets[1], 1) + "," +
                String(_scheduleConfig.weekday[a].targets[2], 1) + "]";
        json += "}";
    }
    json += "},";

    // Weekend profile
    json += "\"weekend\":{";
    for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
        if (a > 0) json += ",";
        json += "\"" + String(anchorKeys[a]) + "\":{";
        json += "\"hour\":" + String(_scheduleConfig.weekend[a].hour) + ",";
        json += "\"minute\":" + String(_scheduleConfig.weekend[a].minute) + ",";
        json += "\"targets\":[" +
                String(_scheduleConfig.weekend[a].targets[0], 1) + "," +
                String(_scheduleConfig.weekend[a].targets[1], 1) + "," +
                String(_scheduleConfig.weekend[a].targets[2], 1) + "]";
        json += "}";
    }
    json += "}";

    json += "}";
    return json;
}

uint32_t ThermalZoneManager::getBootWarmupElapsedSec() const {
    if (!_bootWarmupActive) return 0;
    return (millis() - _bootWarmupStartMs) / 1000;
}

uint32_t ThermalZoneManager::getBootWarmupRemainingSec() const {
    if (!_bootWarmupActive) return 0;
    uint32_t elapsed = millis() - _bootWarmupStartMs;
    if (elapsed >= 300000UL) return 0;
    return (300000UL - elapsed) / 1000;
}

size_t ThermalZoneManager::getPairedZoneCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled && (strlen(_configs[i].sensorMac) > 0 || strlen(_configs[i].sensorName) > 0)) {
            count++;
        }
    }
    return count;
}

size_t ThermalZoneManager::getAcquiredZoneCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled && _statuses[i].sensorValid) {
            count++;
        }
    }
    return count;
}

void ThermalZoneManager::checkAntiSeizeSchedule(uint32_t now) {
    time_t nowEpoch = time(nullptr);
    if (nowEpoch < 1700000000) {
        return; // Wait until NTP is synced before scheduling anti-seize
    }
    // First boot with NTP synchronized: record now to prevent immediate triggering
    if (_lastAntiSeizeEpoch == 0) {
        _lastAntiSeizeEpoch = (uint32_t)nowEpoch;
        saveToPreferences();
        return;
    }
    // If boiler/pumps ran within the last 7 days, defer anti-seize
    if (_lastBoilerOnTime > 0 && (now - _lastBoilerOnTime < 604800000UL)) {
        return;
    }
    // Run anti-seize exercise every 7 days (604800 seconds)
    if ((uint32_t)nowEpoch - _lastAntiSeizeEpoch >= 604800UL) {
        triggerAntiSeizeExercise();
    }
}

void ThermalZoneManager::update(BoilerRadioTrigger& boiler) {
    uint32_t now = millis();
    if (now - _lastEvaluationMs < 1000) {
        return;
    }
    _lastEvaluationMs = now;

    // Handle ongoing Anti-Seize Exercise (20s duration)
    if (_antiSeizeRunning) {
        if (now - _antiSeizeStartMs >= 20000UL) {
            _antiSeizeRunning = false;
            time_t nowEpoch = time(nullptr);
            if (nowEpoch >= 1700000000) {
                _lastAntiSeizeEpoch = (uint32_t)nowEpoch;
            }
            saveToPreferences();

            // Turn off pump
            if (_pumpControlEnabled) {
                applyRelay(PIN_RELAY_SPARE, false);
                _pumpActive = false;
            }
            // Restore relays based on demand
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                if (_configs[i].enabled) {
                    applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged);
                }
            }
            netConsole.println(F("[Thermal] Anti-seize exercise completed successfully."));
        }
        return; // Don't run standard logic during exercise
    }

    // Periodic Bio-Schedule evaluation (every 10s)
    if (_scheduleConfig.enabled && (now - _lastScheduleEvalMs >= 10000UL || _lastScheduleEvalMs == 0)) {
        _lastScheduleEvalMs = now;
        evaluateSchedule(now);
    }

    // Periodic check for anti-seize
    checkAntiSeizeSchedule(now);

    // 1. Evaluate heat demand per zone
    _frostProtectionActive = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        float currentTemp = -999.0f;
        bool found = false;
        uint32_t timeoutMs = _sensorTimeoutSec * 1000UL;

        // Direct Native BLE Sensor Resolution
        if (strlen(_configs[i].sensorMac) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByMac(_configs[i].sensorMac);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < timeoutMs)) {
                currentTemp = ble->temperature;
                found = true;
            }
        }

        if (!found && strlen(_configs[i].sensorName) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByName(_configs[i].sensorName);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < timeoutMs)) {
                currentTemp = ble->temperature;
                found = true;
            }
        }

        if (found) {
            _statuses[i].sensorValid = true;
            _statuses[i].sensorFault = false;
            _statuses[i].currentTemp = currentTemp;
        } else {
            _statuses[i].sensorValid = false;
        }

        if (!_configs[i].enabled) {
            _statuses[i].state = ZONE_DISABLED;
            _statuses[i].demand = false;
            _statuses[i].sensorFault = false;
            _statuses[i].frostActive = false;
            if (_statuses[i].relayEngaged) {
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
            }
            continue;
        }

        if (found) {
            // Evaluate Open Window Detection
            evaluateWindowDetection(i, currentTemp, now);

            // Transition from warmup, timeout or re-enabled to normal state
            if (_statuses[i].state == ZONE_WARMUP || _statuses[i].state == ZONE_DISABLED) {
                _statuses[i].state = _statuses[i].demand ? ZONE_HEATING : ZONE_IDLE;
                size_t acquired = getAcquiredZoneCount();
                size_t paired = getPairedZoneCount();
                uint32_t elapsed = (now - _bootWarmupStartMs) / 1000;
                netConsole.printf("[INIT]   * Sensor Discovery: Acquired (%u/%u): %s -> %.1f \xC2\xB0""C at %us\n",
                                  (unsigned int)acquired, (unsigned int)paired,
                                  _configs[i].name, currentTemp, elapsed);
            } else if (_statuses[i].state == ZONE_FAULT_TIMEOUT) {
                _statuses[i].state = _statuses[i].demand ? ZONE_HEATING : ZONE_IDLE;
                netConsole.printf("[Thermal] Zone %u (%s) sensor recovered: %.1f \xC2\xB0""C\n",
                                  _configs[i].id, _configs[i].name, currentTemp);
            }

            // Failsafe: Emergency Frost Protection Guard (< 12C)
            if (currentTemp < _freezeProtectionTemp) {
                _statuses[i].frostActive = true;
                _frostProtectionActive = true;
                _statuses[i].demand = true;
                _statuses[i].state = ZONE_FROST_GUARD;
                netConsole.printf("[Thermal][FROST GUARD] Emergency heating triggered on Zone %u (%s): %.1f C < %.1f C!\n",
                    _configs[i].id, _configs[i].name, currentTemp, _freezeProtectionTemp);
            } else if (_statuses[i].frostActive && currentTemp >= (_freezeProtectionTemp + 1.5f)) {
                _statuses[i].frostActive = false;
            }

            // Normal Hysteresis loop (only if autoMode and not forced by frost guard)
            if (_autoMode && !_statuses[i].frostActive) {
                if (_windowStates[i].pauseActive) {
                    _statuses[i].demand = false;
                    _statuses[i].state = ZONE_WINDOW_PAUSE;
                } else {
                    float target = getEffectiveTargetTemp(i);
                    if (!_statuses[i].demand) {
                        if (_statuses[i].currentTemp <= (target - _configs[i].hysteresis)) {
                            _statuses[i].demand = true;
                            netConsole.printf("[Thermal] Zone %u (%s) heat demand: %.1f C <= %.1f C (target: %.1f C%s)\n",
                                _configs[i].id, _configs[i].name,
                                _statuses[i].currentTemp,
                                target - _configs[i].hysteresis,
                                target,
                                _scheduleConfig.enabled ? (_preHeatActive ? ", PRE-HEAT" : ", BIO") : "");
                        }
                    } else {
                        if (_statuses[i].currentTemp >= (target + _configs[i].hysteresis)) {
                            _statuses[i].demand = false;
                            _statuses[i].state = ZONE_IDLE;
                            netConsole.printf("[Thermal] Zone %u (%s) target reached: %.1f C >= %.1f C (target: %.1f C%s)\n",
                                _configs[i].id, _configs[i].name,
                                _statuses[i].currentTemp,
                                target + _configs[i].hysteresis,
                                target,
                                _scheduleConfig.enabled ? (_preHeatActive ? ", PRE-HEAT" : ", BIO") : "");
                        }
                    }
                }
            }
        } else {
            if (_bootWarmupActive) {
                _statuses[i].sensorValid = false;
                _statuses[i].sensorFault = false;
                _statuses[i].state = ZONE_WARMUP;
            } else {
                // Sensor missing > timeout: CRITICAL FAILSAFE
                _statuses[i].sensorValid = false;
                _statuses[i].sensorFault = true;
                _statuses[i].state = ZONE_FAULT_TIMEOUT;

                if (_statuses[i].demand) {
                    netConsole.printf("[Thermal] CRITICAL FAILSAFE: Zone %u (%s) sensor timeout >%u s! Shutting down actuator.\n",
                        _configs[i].id, _configs[i].name, _sensorTimeoutSec);
                    _statuses[i].demand = false;
                    applyRelay(_configs[i].relayPin, false);
                    _statuses[i].relayEngaged = false;
                    char dtcMsg[64];
                    snprintf(dtcMsg, sizeof(dtcMsg), "Zone %u sensor timeout >%us - zone shutdown", _configs[i].id, _sensorTimeoutSec);
                    diagEngine.raiseDtc("DTC-101", dtcMsg, SEV_ERROR);
                }
            }
        }
    }

    // Evaluate Boot Warmup completion or expiration (5 min = 300,000 ms)
    if (_bootWarmupActive) {
        size_t paired = getPairedZoneCount();
        size_t acquired = getAcquiredZoneCount();
        if (paired > 0 && acquired >= paired) {
            _bootWarmupActive = false;
            netConsole.printf("[INIT]   * Sensor Discovery: All %u room sensors acquired in %us! System fully operational.\n",
                              (unsigned int)paired, (unsigned int)((now - _bootWarmupStartMs) / 1000));
        } else if (now - _bootWarmupStartMs >= 300000UL) {
            _bootWarmupActive = false;
            netConsole.printf("[INIT]   * Sensor Discovery: Warmup cutoff (300s): %u/%u sensors acquired. Activating system for operational zones.\n",
                              (unsigned int)acquired, (unsigned int)paired);
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                if (_configs[i].enabled && !_statuses[i].sensorValid) {
                    _statuses[i].state = ZONE_FAULT_TIMEOUT;
                    _statuses[i].sensorFault = true;
                    char dtcMsg[64];
                    snprintf(dtcMsg, sizeof(dtcMsg), "Zone %u sensor missing at boot warmup cutoff", _configs[i].id);
                    diagEngine.raiseDtc("DTC-101", dtcMsg, SEV_ERROR);
                }
            }
        }
        // Acquired operational zones can begin normal heating immediately;
        // zones still waiting for BLE sensors stay safe in ZONE_WARMUP (valves closed).
    }



    if (!_autoMode && !_frostProtectionActive) {
        return; // In manual mode without frost emergency, skip automatic actuator & boiler sequencing
    }

    // 2. Actuator Relay State Machine with Warm-up Delay
    bool anyZoneDemanding = false;
    bool anyZoneReadyForBoiler = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled || _statuses[i].sensorFault) continue;

        if (_statuses[i].demand) {
            anyZoneDemanding = true;
            if (_statuses[i].state == ZONE_IDLE || _statuses[i].state == ZONE_FAULT_TIMEOUT) {
                // Begin actuator warm-up
                _statuses[i].state = ZONE_WARMING_UP;
                _statuses[i].stateChangedMs = now;
                applyRelay(_configs[i].relayPin, true);
                _statuses[i].relayEngaged = true;
                netConsole.printf("[Thermal] Actuator Zone %u (%s) opening on pin %u. Waiting %u s...\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin, _interlockDelayMs / 1000);
            } else if (_statuses[i].state == ZONE_WARMING_UP) {
                if (now - _statuses[i].stateChangedMs >= _interlockDelayMs) {
                    _statuses[i].state = ZONE_HEATING;
                    _statuses[i].stateChangedMs = now;
                    netConsole.printf("[Thermal] Actuator Zone %u (%s) fully OPEN. Circuit ready!\n",
                        _configs[i].id, _configs[i].name);
                }
            }

            if (_statuses[i].state == ZONE_HEATING || _statuses[i].state == ZONE_FROST_GUARD) {
                anyZoneReadyForBoiler = true;
                _overrunKeeperZone = (int8_t)i; // Track last active zone for hydraulic overrun heat dissipation
            }
        } else {
            // Demand is false
            if (_statuses[i].state == ZONE_HEATING || _statuses[i].state == ZONE_WARMING_UP) {
                // If pump overrun is in progress and this is the keeper zone, keep it open until pump stops
                if (_pumpActive && (int8_t)i == _overrunKeeperZone) {
                    // Keeper zone stays engaged for residual heat dissipation
                } else {
                    _statuses[i].state = ZONE_IDLE;
                    _statuses[i].stateChangedMs = now;
                    applyRelay(_configs[i].relayPin, false);
                    _statuses[i].relayEngaged = false;
                    netConsole.printf("[Thermal] Zone %u (%s) closed relay on pin %u.\n",
                        _configs[i].id, _configs[i].name, _configs[i].relayPin);
                }
            } else if (_statuses[i].state == ZONE_WINDOW_PAUSE && _statuses[i].relayEngaged) {
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
                netConsole.printf("[Thermal] Zone %u (%s) closed relay due to window pause.\n",
                    _configs[i].id, _configs[i].name);
            }
        }
    }

    // 3. Boiler Flame & Secondary Circulation Pump Control
    if (anyZoneReadyForBoiler) {
        _boilerDemand = true;

        // If boiler is not running, command ignition as soon as safety timers permit
        if (!boiler.isLastCommandOn()) {
            if (boiler.canTransmit(true)) {
                if (boiler.sendBoilerOn()) {
                    _lastBoilerOnTime = now;
                    netConsole.println(F("[Thermal] BOILER START commanded via RF 433MHz!"));
                }
            } else if (boiler.getPendingAction() != PENDING_ON) {
                boiler.setPendingAction(PENDING_ON);
                netConsole.println(F("[Thermal] BOILER START queued (Anti-short-cycling guard active)..."));
            }
        }

        // Start circulation pump if enabled (Interlock satisfied: at least 1 actuator is open!)
        if (_pumpControlEnabled && !_pumpActive) {
            applyRelay(PIN_RELAY_SPARE, true);
            _pumpActive = true;
            netConsole.println(F("[Thermal] Circulation Pump (Relay 4) STARTED."));
        }
    } else {
        // No zones ready for boiler
        if (_boilerDemand || boiler.isLastCommandOn()) {
            _boilerDemand = false;
            _lastBoilerOffTime = now;
            if (boiler.isLastCommandOn()) {
                boiler.sendBoilerOff();
                netConsole.println(F("[Thermal] BOILER STOP commanded via RF 433MHz!"));
            } else if (boiler.getPendingAction() == PENDING_ON) {
                boiler.cancelPendingAction();
            }

            // Schedule Pump Overrun (Post-circulation)
            if (_pumpControlEnabled && _pumpActive) {
                _pumpStopScheduledMs = now + _pumpOverrunMs;
                netConsole.printf("[Thermal] Pump Overrun active: pump will run for %u s to dissipate heat.\n", _pumpOverrunMs / 1000);
            }
        }

        // Handle pump overrun termination
        if (_pumpActive && _pumpControlEnabled) {
            if (_pumpStopScheduledMs > 0 && now >= _pumpStopScheduledMs) {
                applyRelay(PIN_RELAY_SPARE, false);
                _pumpActive = false;
                _pumpStopScheduledMs = 0;
                netConsole.println(F("[Thermal] Pump Overrun completed. Circulation pump stopped."));

                // Now close the keeper zone that was kept open for dissipation
                if (_overrunKeeperZone >= 0 && _overrunKeeperZone < MAX_THERMAL_ZONES) {
                    if (!_statuses[_overrunKeeperZone].demand) {
                        applyRelay(_configs[_overrunKeeperZone].relayPin, false);
                        _statuses[_overrunKeeperZone].relayEngaged = false;
                        _statuses[_overrunKeeperZone].state = ZONE_IDLE;
                        netConsole.printf("[Thermal] Overrun keeper Zone %u closed.\n", _configs[_overrunKeeperZone].id);
                    }
                    _overrunKeeperZone = -1;
                }
            }
        }
    }
}
