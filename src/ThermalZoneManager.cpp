#include "ThermalZoneManager.h"
#include "NetworkConsole.h"
#include "BleSensorScanner.h"
#include "DiagnosticEngine.h"
#include "TimeAndWeatherService.h"
#include <time.h>
#include <algorithm>

ThermalZoneManager thermalZones;

ThermalZoneManager::ThermalZoneManager()
    : _activeSlotKey(-1),
      _activeLevel(LEVEL_DAY),
      _nextLevel(LEVEL_DAY),
      _minsToNextSlot(0),
      _preHeatActive(false),
      _lastScheduleEvalMs(0),
      _autoMode(true),
      _boilerDemand(false),
      _pumpControlEnabled(false),
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
      _lastEvaluationMs(0),
      _nvsSavePending(false),
      _nvsSavePendingMs(0) {

    _scheduleConfig = getFactoryDefaultSchedule();
    _scheduleConfig.enabled = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _windowStates[i].baselineTemp = -999.0f;
        _windowStates[i].baselineMs = 0;
        _windowStates[i].pauseActive = false;
        _windowStates[i].pauseStartMs = 0;
        _windowStates[i].pauseMinTemp = 999.0f;
        _windowStates[i].dropInsightLogged = false;
        _windowStates[i].lastDetectionEpoch = 0;
        _windowStates[i].lastDetectionDurationSec = 0;
        _tpi[i] = {0, 0, 0.0f, 0.0f, 0.0f, 0.0f};
        _configs[i].tempOffset = 0.0f;
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
    _configs[0].windowDropThreshold = 1.2f;

    // Default Zone 2
    _configs[1].id = 2;
    strncpy(_configs[1].name, "Camera Copilului", sizeof(_configs[1].name) - 1);
    strncpy(_configs[1].sensorMac, "A4:C1:38:84:A1:E8", sizeof(_configs[1].sensorMac) - 1);
    strncpy(_configs[1].sensorName, "Kids Room", sizeof(_configs[1].sensorName) - 1);
    _configs[1].relayPin = PIN_RELAY_ZONE_2;
    _configs[1].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[1].hysteresis = DEFAULT_HYSTERESIS;
    _configs[1].enabled = true;
    _configs[1].windowDropThreshold = 1.2f;

    // Default Zone 3
    _configs[2].id = 3;
    strncpy(_configs[2].name, "Dormitor Matrimonial", sizeof(_configs[2].name) - 1);
    strncpy(_configs[2].sensorMac, "", sizeof(_configs[2].sensorMac) - 1);
    strncpy(_configs[2].sensorName, "Matrimonial", sizeof(_configs[2].sensorName) - 1);
    _configs[2].relayPin = PIN_RELAY_ZONE_3;
    _configs[2].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[2].hysteresis = DEFAULT_HYSTERESIS;
    _configs[2].enabled = true;
    _configs[2].windowDropThreshold = 1.2f;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].state = ZONE_IDLE;
        _statuses[i].currentTemp = 0.0f;
        _statuses[i].sensorValid = false;
        _statuses[i].sensorFault = false;
        _statuses[i].frostActive = false;
        _statuses[i].demand = false;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = 0;
        _statuses[i].isManualHold = false;
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
    _pumpControlEnabled = prefs.getBool("pump_en", false);
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
        _configs[i].windowDropThreshold = prefs.getFloat((p + "wdrop").c_str(), 1.2f);
        _configs[i].tempOffset = prefs.getFloat((p + "ofs").c_str(), 0.0f);
        _tpi[i].integral = _tpi[i].savedIntegral = prefs.getFloat((p + "tpi").c_str(), 0.0f);
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
        prefs.putFloat((p + "wdrop").c_str(), _configs[i].windowDropThreshold);
        prefs.putFloat((p + "ofs").c_str(), _configs[i].tempOffset);
    }
    prefs.end();
    Serial.println(F("[Thermal] Zone configs successfully persisted to NVS."));
}

void ThermalZoneManager::loadSchedulePreferences() {
    Preferences prefs;
    if (!prefs.begin("sched", true)) {
        return;
    }
    ScheduleConfig loaded;
    bool valid = prefs.getBytes("cfg2", &loaded, sizeof(loaded)) == sizeof(loaded) &&
                 loaded.weekdayCount >= 1 && loaded.weekdayCount <= MAX_SCHEDULE_SLOTS &&
                 loaded.weekendCount >= 1 && loaded.weekendCount <= MAX_SCHEDULE_SLOTS;
    if (valid) {
        _scheduleConfig = loaded;
        Serial.println(F("[Thermal] Schedule loaded from NVS."));
    } else if (prefs.isKey("cfg")) {
        // Legacy bio-schedule: keep only its on/off flags (first 3 bytes), slots reset to defaults
        uint8_t legacy[1024];
        size_t len = prefs.getBytesLength("cfg");
        if (len >= 3 && len <= sizeof(legacy) && prefs.getBytes("cfg", legacy, len) == len) {
            _scheduleConfig.enabled = legacy[0];
            _scheduleConfig.preHeatEnabled = legacy[1];
            _scheduleConfig.windowDetectionEnabled = legacy[2];
        }
        Serial.println(F("[Thermal] Legacy bio-schedule replaced by default level schedule."));
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
    ScheduleConfig currentSaved;
    size_t len = prefs.getBytes("cfg2", &currentSaved, sizeof(currentSaved));
    if (len != sizeof(currentSaved) || memcmp(&currentSaved, &_scheduleConfig, sizeof(_scheduleConfig)) != 0) {
        prefs.putBytes("cfg2", &_scheduleConfig, sizeof(_scheduleConfig));
        Serial.println(F("[Thermal] Schedule persisted to NVS."));
    }
    // One-time cleanup of the legacy bio-schedule and presets
    if (prefs.isKey("cfg")) {
        prefs.remove("cfg");
        prefs.remove("act_preset");
        Preferences presets;
        if (presets.begin("presets", false)) {
            presets.clear();
            presets.end();
        }
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
            // Debounce NVS flash write by 3000ms to protect flash and smooth slider changes
            _nvsSavePending = true;
            _nvsSavePendingMs = millis() + 3000;
        }

        // If the schedule is active, mark as Temporary Hold (holds until next slot)
        if (_scheduleConfig.enabled && timeAndWeather.isTimeSynced()) {
            _statuses[index].isManualHold = true;
            netConsole.printf("[Thermal] Zone %u (%s) TEMPORARY HOLD set to %.1f C (holds until next slot: %s in %u min)\n",
                _configs[index].id, _configs[index].name, target, getLevelName(_nextLevel), _minsToNextSlot);
        } else {
            _statuses[index].isManualHold = false;
            netConsole.printf("[Thermal] Zone %u (%s) new target temp: %.1f C\n",
                _configs[index].id, _configs[index].name, target);
        }
    }
}

void ThermalZoneManager::clearManualHold(size_t index) {
    if (index >= MAX_THERMAL_ZONES) return;
    _statuses[index].isManualHold = false;
    if (_scheduleConfig.enabled && timeAndWeather.isTimeSynced()) {
        _configs[index].targetTemp = getEffectiveTargetTemp(index);
        _nvsSavePending = true;
        _nvsSavePendingMs = millis() + 3000;
        netConsole.printf("[Thermal] Zone %u (%s) manual hold CLEARED. Reverted to schedule target %.1f C\n",
            _configs[index].id, _configs[index].name, _configs[index].targetTemp);
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

bool ThermalZoneManager::setTempOffset(size_t index, float offset) {
    if (index >= MAX_THERMAL_ZONES || offset < -3.0f || offset > 3.0f) return false;
    if (fabsf(_configs[index].tempOffset - offset) > 0.01f) {
        _configs[index].tempOffset = offset;
        saveToPreferences();
    }
    netConsole.printf("[Thermal] Zone %u (%s) sensor offset %+.1f C\n", _configs[index].id, _configs[index].name, offset);
    return true;
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


void ThermalZoneManager::setOpenLoopBypass(bool en) {
    _openLoopBypass = en;
}

void ThermalZoneManager::setInterlockDelayMs(uint32_t ms) {
    _interlockDelayMs = ms;
}

void ThermalZoneManager::setBoilerDelayMs(uint32_t ms) {
    _boilerDelayMs = ms;
}

void ThermalZoneManager::setPumpOverrunMs(uint32_t ms) {
    _pumpOverrunMs = ms;
}

void ThermalZoneManager::setManifoldTurReturSensors(bool en) {
    _hasManifoldTurReturSensors = en;
}

void ThermalZoneManager::setPumpRelayPin(uint8_t pin) {
    _pumpRelayPin = pin;
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

const char* ThermalZoneManager::getLevelName(uint8_t level) {
    switch (level) {
        case LEVEL_DAY:   return "day";
        case LEVEL_NIGHT: return "night";
        case LEVEL_AWAY:  return "away";
        default:          return "unknown";
    }
}

int ThermalZoneManager::parseLevel(const char* name) {
    if (!name) return -1;
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
        if (strcasecmp(name, getLevelName(l)) == 0) return l;
    }
    return -1;
}

ScheduleConfig ThermalZoneManager::getFactoryDefaultSchedule() {
    ScheduleConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enabled = true;
    cfg.preHeatEnabled = true;
    cfg.windowDetectionEnabled = true;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        cfg.levelTemps[LEVEL_DAY][i] = 22.0f;
        cfg.levelTemps[LEVEL_NIGHT][i] = 20.0f;
        cfg.levelTemps[LEVEL_AWAY][i] = 16.0f;
    }
    cfg.weekdayCount = 2;
    cfg.weekday[0] = {6, 30, LEVEL_DAY};
    cfg.weekday[1] = {22, 30, LEVEL_NIGHT};
    cfg.weekendCount = 2;
    cfg.weekend[0] = {8, 0, LEVEL_DAY};
    cfg.weekend[1] = {23, 0, LEVEL_NIGHT};
    return cfg;
}

bool ThermalZoneManager::setLevelTemp(size_t index, ComfortLevel level, float temp) {
    if (index >= MAX_THERMAL_ZONES || level >= LEVEL_COUNT || temp < 5.0f || temp > 30.0f) return false;
    _scheduleConfig.levelTemps[level][index] = temp;
    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Zone %u (%s) %s level set to %.1f C\n",
                      _configs[index].id, _configs[index].name, getLevelName(level), temp);
    return true;
}

bool ThermalZoneManager::setSlots(bool weekend, const char* spec) {
    if (!spec) return false;
    ScheduleSlot parsed[MAX_SCHEDULE_SLOTS];
    uint8_t n = 0;
    char buf[160];
    strlcpy(buf, spec, sizeof(buf));
    for (char* tok = strtok(buf, ", "); tok; tok = strtok(nullptr, ", ")) {
        unsigned h, m;
        char lvl[8];
        if (n >= MAX_SCHEDULE_SLOTS || sscanf(tok, "%u:%u=%7s", &h, &m, lvl) != 3 || h > 23 || m > 59) return false;
        int level = parseLevel(lvl);
        if (level < 0) return false;
        parsed[n++] = {(uint8_t)h, (uint8_t)m, (uint8_t)level};
    }
    if (n == 0) return false;
    std::sort(parsed, parsed + n, [](const ScheduleSlot& a, const ScheduleSlot& b) {
        return a.hour * 60 + a.minute < b.hour * 60 + b.minute;
    });
    memcpy(weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday, parsed, n * sizeof(ScheduleSlot));
    (weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount) = n;
    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] %s slots: %s\n", weekend ? "Weekend" : "Weekday", getSlotsString(weekend).c_str());
    return true;
}

String ThermalZoneManager::getSlotsString(bool weekend) const {
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    const ScheduleSlot* slots = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    String out;
    char buf[16];
    for (uint8_t i = 0; i < count; i++) {
        snprintf(buf, sizeof(buf), "%s%02u:%02u=%s", i ? "," : "", slots[i].hour, slots[i].minute, getLevelName(slots[i].level));
        out += buf;
    }
    return out;
}

void ThermalZoneManager::setAway(bool away) {
    if (_scheduleConfig.away == away) return;
    _scheduleConfig.away = away;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].isManualHold = false;
    }
    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Away mode %s\n", away ? "ON (all zones at away level)" : "OFF (back to schedule)");
}

void ThermalZoneManager::resetScheduleToDefault() {
    bool en = _scheduleConfig.enabled;
    _scheduleConfig = getFactoryDefaultSchedule();
    _scheduleConfig.enabled = en;
    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.println(F("[Thermal] Schedule reset to factory default."));
}

void ThermalZoneManager::setScheduleEnabled(bool enabled) {
    if (_scheduleConfig.enabled != enabled) {
        _scheduleConfig.enabled = enabled;
        saveSchedulePreferences();
        evaluateSchedule(millis());
        netConsole.printf("[Thermal] Schedule %s\n", enabled ? "ENABLED" : "DISABLED");
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
    (void)now;
    if (!timeAndWeather.isTimeSynced()) return;

    bool weekend = timeAndWeather.isWeekend();
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count == 0) return;
    const ScheduleSlot* slots = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    uint32_t curMinutes = timeAndWeather.getHour() * 60 + timeAndWeather.getMinute();

    // Active slot = latest slot at or before now (slots are sorted); before the first one, the last carries over
    size_t activeIdx = count - 1;
    for (size_t i = 0; i < count; i++) {
        if (curMinutes >= (uint32_t)(slots[i].hour * 60 + slots[i].minute)) activeIdx = i;
    }
    size_t nextIdx = (activeIdx + 1) % count;
    uint32_t nextMinutes = slots[nextIdx].hour * 60 + slots[nextIdx].minute;
    _minsToNextSlot = (nextMinutes > curMinutes) ? nextMinutes - curMinutes : 1440 - curMinutes + nextMinutes;
    _activeLevel = slots[activeIdx].level;
    _nextLevel = slots[nextIdx].level;

    // Pre-Heat: start early (weather-dependent lead) if the next level is warmer in any zone
    _preHeatActive = false;
    if (_scheduleConfig.preHeatEnabled && _minsToNextSlot <= getPreHeatLeadMinutes()) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (_scheduleConfig.levelTemps[_nextLevel][i] > _scheduleConfig.levelTemps[_activeLevel][i] + 0.2f) {
                _preHeatActive = true;
                break;
            }
        }
    }

    // Slot transition: manual holds expire
    int16_t key = (weekend ? 16 : 0) + (int16_t)activeIdx;
    if (key != _activeSlotKey) {
        if (_activeSlotKey >= 0) {
            netConsole.printf("[Thermal][SCHEDULE] Slot %02u:%02u -> level %s (%s)\n",
                slots[activeIdx].hour, slots[activeIdx].minute, getLevelName(_activeLevel), weekend ? "Weekend" : "Weekday");
        }
        _activeSlotKey = key;
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (_statuses[i].isManualHold) {
                _statuses[i].isManualHold = false;
                netConsole.printf("[Thermal] Zone %u (%s) Temporary Hold expired at slot change.\n",
                    (unsigned int)_configs[i].id, _configs[i].name);
            }
        }
    }

    // Keep static targets in sync (shown in status, used if the schedule is turned off)
    if (_scheduleConfig.enabled || _scheduleConfig.away) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (!_statuses[i].isManualHold) {
                _configs[i].targetTemp = getEffectiveTargetTemp(i);
            }
        }
    }
}

uint32_t ThermalZoneManager::getPreHeatLeadMinutes() const {
    // Screed needs hours: 2 h at 5 C outdoors, 3.5 h at -5 C (45 min .. 4 h); 90 min without weather data
    if (!timeAndWeather.isWeatherValid()) return 90;
    int lead = 120 + (int)((5.0f - timeAndWeather.getOutdoorTemperature()) * 9.0f);
    return (uint32_t)constrain(lead, 45, 240);
}

bool ThermalZoneManager::evaluateTpi(size_t index, float target, uint32_t now) {
    TpiState& t = _tpi[index];
    float temp = _statuses[index].currentTemp;
    float err = target - temp;
    bool cycleDone = t.cycleStartMs != 0 && now - t.cycleStartMs >= TPI_CYCLE_MS;

    // New cycle every hour, or immediately when the target moves (schedule level change, boost)
    if (t.cycleStartMs == 0 || cycleDone || fabsf(target - t.target) > 0.4f) {
        // Learn the base duty that holds the target; only near target so big setpoint jumps do not wind it up
        if (cycleDone && fabsf(err) < 1.0f) {
            t.integral = constrain(t.integral + TPI_KI * err, 0.0f, TPI_MAX_INTEGRAL);
            // Keep the learning across reboots; write only on a meaningful change (NVS wear)
            if (fabsf(t.integral - t.savedIntegral) >= 0.05f) {
                Preferences prefs;
                if (prefs.begin("zones", false)) {
                    prefs.putFloat(("z" + String(index) + "_tpi").c_str(), t.integral);
                    prefs.end();
                    t.savedIntegral = t.integral;
                }
            }
        }
        t.duty = constrain(err / TPI_BAND_C + t.integral, 0.0f, 1.0f);
        uint32_t onMs = (uint32_t)(t.duty * TPI_CYCLE_MS);
        if (onMs < TPI_MIN_ON_MS) onMs = 0;
        else if (TPI_CYCLE_MS - onMs < TPI_MIN_ON_MS) onMs = TPI_CYCLE_MS;
        t.cycleStartMs = now;
        t.onMs = onMs;
        t.target = target;
        netConsole.printf("[Thermal][TPI] Zone %u (%s): %.2f C -> target %.1f C, duty %u%% (learned %u%%), valve %u min/h\n",
            _configs[index].id, _configs[index].name, temp, target,
            (unsigned int)(t.duty * 100), (unsigned int)(t.integral * 100), (unsigned int)(onMs / 60000UL));
    }

    // Overshoot cut: stop for the rest of the cycle once above target + hysteresis (e.g. solar gain)
    uint32_t elapsed = now - t.cycleStartMs;
    if (temp >= target + _configs[index].hysteresis && elapsed < t.onMs) {
        t.onMs = elapsed;
    }
    return elapsed < t.onMs;
}

float ThermalZoneManager::getEffectiveTargetTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return DEFAULT_SETPOINT_TEMP;
    if (_statuses[index].isManualHold) return _configs[index].targetTemp;
    if (_scheduleConfig.away) return _scheduleConfig.levelTemps[LEVEL_AWAY][index];
    if (!_scheduleConfig.enabled || !timeAndWeather.isTimeSynced()) return _configs[index].targetTemp;
    return _scheduleConfig.levelTemps[_preHeatActive ? _nextLevel : _activeLevel][index];
}

void ThermalZoneManager::evaluateWindowDetection(size_t index, float currentTemp, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return;
    if (!_scheduleConfig.windowDetectionEnabled) {
        _windowStates[index].pauseActive = false;
        return;
    }

    float dropThreshold = (_configs[index].windowDropThreshold >= 0.5f) ? _configs[index].windowDropThreshold : 1.2f;

    if (!_windowStates[index].pauseActive) {
        if (_windowStates[index].baselineMs == 0 || (now - _windowStates[index].baselineMs > 300000UL)) {
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
            _windowStates[index].dropInsightLogged = false;
        } else if (currentTemp > _windowStates[index].baselineTemp) {
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
            _windowStates[index].dropInsightLogged = false;
        } else if ((_windowStates[index].baselineTemp - currentTemp) >= dropThreshold) {
            _windowStates[index].pauseActive = true;
            _windowStates[index].pauseStartMs = now;
            _windowStates[index].pauseMinTemp = currentTemp;
            time_t nowEpoch = time(nullptr);
            _windowStates[index].lastDetectionEpoch = (nowEpoch >= 1700000000) ? (uint32_t)nowEpoch : (now / 1000);
            netConsole.printf("[Thermal][WINDOW] Open window detected on Zone %u (%s): temp dropped %.2f \xC2\xB0""C in %us! (threshold: %.1f C). Suspending heating for 30m.\n",
                              _configs[index].id, _configs[index].name,
                              _windowStates[index].baselineTemp - currentTemp,
                              (now - _windowStates[index].baselineMs) / 1000,
                              dropThreshold);
        } else if ((_windowStates[index].baselineTemp - currentTemp) >= 0.35f && !_windowStates[index].dropInsightLogged) {
            _windowStates[index].dropInsightLogged = true;
            netConsole.printf("[Thermal][INSIGHT] Rapid cooling detected on Zone %u (%s): %.2f C -> %.2f C (-%.2f C in %us, possible aeration)\n",
                              _configs[index].id, _configs[index].name,
                              _windowStates[index].baselineTemp, currentTemp,
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
            _windowStates[index].lastDetectionDurationSec = (now - _windowStates[index].pauseStartMs) / 1000;
            _windowStates[index].pauseActive = false;
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
            netConsole.printf("[Thermal][WINDOW] Zone %u (%s) window pause cleared (%s, lasted %us). Resuming normal thermostat loop.\n",
                              _configs[index].id, _configs[index].name,
                              timeoutExpired ? "30 min timeout" : "temp rebounded +0.6 \xC2\xB0""C",
                              _windowStates[index].lastDetectionDurationSec);
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

bool ThermalZoneManager::clearWindowPause(size_t index) {
    if (index >= MAX_THERMAL_ZONES) return false;
    uint32_t now = millis();
    if (_windowStates[index].pauseActive) {
        _windowStates[index].lastDetectionDurationSec = (now - _windowStates[index].pauseStartMs) / 1000;
        _windowStates[index].pauseActive = false;
        _windowStates[index].baselineTemp = _statuses[index].currentTemp;
        _windowStates[index].baselineMs = now;
        if (_statuses[index].state == ZONE_WINDOW_PAUSE) {
            _statuses[index].state = ZONE_IDLE;
            _statuses[index].stateChangedMs = now;
        }
        netConsole.printf("[Thermal][WINDOW] Zone %u (%s) window pause MANUALLY CLEARED (lasted %us). Re-armed baseline at %.2f C.\n",
                          _configs[index].id, _configs[index].name,
                          _windowStates[index].lastDetectionDurationSec,
                          _windowStates[index].baselineTemp);
        return true;
    }
    return false;
}

void ThermalZoneManager::setWindowDropThreshold(size_t index, float threshold) {
    if (index >= MAX_THERMAL_ZONES) return;
    if (threshold < 0.8f) threshold = 0.8f;
    if (threshold > 2.0f) threshold = 2.0f;
    if (fabsf(_configs[index].windowDropThreshold - threshold) > 0.05f) {
        _configs[index].windowDropThreshold = threshold;
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) window drop threshold set to %.1f C\n",
                          _configs[index].id, _configs[index].name, threshold);
    }
}

float ThermalZoneManager::getWindowDropThreshold(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 1.2f;
    return (_configs[index].windowDropThreshold >= 0.8f && _configs[index].windowDropThreshold <= 2.0f) ? _configs[index].windowDropThreshold : 1.2f;
}

uint32_t ThermalZoneManager::getWindowLastDetectionEpoch(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    return _windowStates[index].lastDetectionEpoch;
}

uint32_t ThermalZoneManager::getWindowLastDetectionDurationSec(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    return _windowStates[index].lastDetectionDurationSec;
}

String ThermalZoneManager::getScheduleJson() const {
    String json;
    json.reserve(1536);
    json = "{";
    json += "\"status\":\"ok\",";
    json += "\"enabled\":" + String(_scheduleConfig.enabled ? "true" : "false") + ",";
    json += "\"away\":" + String(_scheduleConfig.away ? "true" : "false") + ",";
    json += "\"preHeatEnabled\":" + String(_scheduleConfig.preHeatEnabled ? "true" : "false") + ",";
    json += "\"windowDetectionEnabled\":" + String(_scheduleConfig.windowDetectionEnabled ? "true" : "false") + ",";
    json += "\"activeLevel\":\"" + String(getLevelName(_activeLevel)) + "\",";
    json += "\"nextLevel\":\"" + String(getLevelName(_nextLevel)) + "\",";
    json += "\"minsToNextSlot\":" + String(_minsToNextSlot) + ",";
    json += "\"preHeatActive\":" + String(_preHeatActive ? "true" : "false") + ",";
    json += "\"isWeekend\":" + String(timeAndWeather.isWeekend() ? "true" : "false") + ",";
    json += "\"weekday\":\"" + getSlotsString(false) + "\",";
    json += "\"weekend\":\"" + getSlotsString(true) + "\",";
    json += "\"openLoopBypass\":" + String(_openLoopBypass ? "true" : "false") + ",";
    json += "\"interlockDelaySec\":" + String(_interlockDelayMs / 1000) + ",";
    json += "\"boilerDelaySec\":" + String(_boilerDelayMs / 1000) + ",";
    json += "\"pumpOverrunSec\":" + String(_pumpOverrunMs / 1000) + ",";
    json += "\"hasTurReturSensors\":" + String(_hasManifoldTurReturSensors ? "true" : "false") + ",";
    json += "\"pumpControlEnabled\":" + String(_pumpControlEnabled ? "true" : "false") + ",";

    json += "\"zones\":[";
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + String(_configs[i].id) + ",";
        json += "\"name\":\"" + String(_configs[i].name) + "\",";
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            json += "\"" + String(getLevelName(l)) + "\":" + String(_scheduleConfig.levelTemps[l][i], 1) + ",";
        }
        json += "\"effectiveTarget\":" + String(getEffectiveTargetTemp(i), 1) + ",";
        json += "\"isManualHold\":" + String(_statuses[i].isManualHold ? "true" : "false") + ",";
        json += "\"windowPauseActive\":" + String(isWindowPauseActive(i) ? "true" : "false") + ",";
        json += "\"windowPauseRemainingSec\":" + String(getWindowPauseRemainingSec(i));
        json += "}";
    }
    json += "]}";
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

    // Handle debounced NVS saving (3s after last slider change)
    if (_nvsSavePending && now >= _nvsSavePendingMs) {
        _nvsSavePending = false;
        saveToPreferences();
    }

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

    // Periodic schedule evaluation (every 10s)
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
                currentTemp = ble->temperature + _configs[i].tempOffset;
                found = true;
            }
        }

        if (!found && strlen(_configs[i].sensorName) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByName(_configs[i].sensorName);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < timeoutMs)) {
                currentTemp = ble->temperature + _configs[i].tempOffset;
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

            // Time-proportional control (only if autoMode and not forced by frost guard)
            if (_autoMode && !_statuses[i].frostActive) {
                if (_windowStates[i].pauseActive) {
                    _statuses[i].demand = false;
                    _statuses[i].state = ZONE_WINDOW_PAUSE;
                } else {
                    _statuses[i].demand = evaluateTpi(i, getEffectiveTargetTemp(i), now);
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

    // 2. Actuator Relay State Machine with Warm-up Delay & Instant Abort
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
            if (_statuses[i].state == ZONE_WARMING_UP) {
                // Instant Abort: Demand dropped during 120s warmup before boiler ever fired!
                _statuses[i].state = ZONE_IDLE;
                _statuses[i].stateChangedMs = now;
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
                netConsole.printf("[Thermal] Actuator Zone %u (%s) warmup aborted: closed relay on pin %u.\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin);
            } else if (_statuses[i].state == ZONE_HEATING) {
                // If pump overrun is actively in progress with scheduled stop, keeper zone stays open
                if (_pumpActive && _pumpStopScheduledMs > 0 && (int8_t)i == _overrunKeeperZone) {
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
            } else if (_statuses[i].relayEngaged) {
                // HARDWARE SAFETY INVARIANT: Demand is false, not warming or heating, but relay is engaged!
                if (!(_pumpActive && _pumpStopScheduledMs > 0 && (int8_t)i == _overrunKeeperZone)) {
                    applyRelay(_configs[i].relayPin, false);
                    _statuses[i].relayEngaged = false;
                    _statuses[i].state = ZONE_IDLE;
                    netConsole.printf("[Thermal] Safety invariant: Zone %u (%s) closed orphaned relay on pin %u.\n",
                        _configs[i].id, _configs[i].name, _configs[i].relayPin);
                }
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
            bool flameWasActive = boiler.isLastCommandOn();
            _boilerDemand = false;
            _lastBoilerOffTime = now;

            if (flameWasActive) {
                if (boiler.canTransmit(false)) {
                    if (boiler.sendBoilerOff()) {
                        netConsole.println(F("[Thermal] BOILER STOP commanded via RF 433MHz!"));
                    }
                } else if (boiler.getPendingAction() != PENDING_OFF) {
                    boiler.setPendingAction(PENDING_OFF);
                    netConsole.println(F("[Thermal] BOILER STOP queued (Anti-short-cycling guard active)..."));
                }
            } else if (boiler.getPendingAction() == PENDING_ON) {
                boiler.cancelPendingAction();
                netConsole.println(F("[Thermal] Pending boiler start CANCELLED (demand dropped before ignition)."));
            }

            // Intelligent Pump Overrun: ONLY run post-circulation if flame was actually active!
            if (flameWasActive && _pumpControlEnabled && _pumpActive) {
                _pumpStopScheduledMs = now + _pumpOverrunMs;
                netConsole.printf("[Thermal] Intelligent Overrun: pump will run for %u s to dissipate boiler residual heat.\n", _pumpOverrunMs / 1000);
            } else {
                // Flame never lit: 0s overrun! Stop pump and close keeper zone immediately.
                if (_pumpControlEnabled && _pumpActive) {
                    applyRelay(PIN_RELAY_SPARE, false);
                    _pumpActive = false;
                    _pumpStopScheduledMs = 0;
                    netConsole.println(F("[Thermal] 0s Overrun: Flame was not active, pump stopped immediately."));
                }
                if (_overrunKeeperZone >= 0 && _overrunKeeperZone < MAX_THERMAL_ZONES) {
                    if (!_statuses[_overrunKeeperZone].demand) {
                        applyRelay(_configs[_overrunKeeperZone].relayPin, false);
                        _statuses[_overrunKeeperZone].relayEngaged = false;
                        _statuses[_overrunKeeperZone].state = ZONE_IDLE;
                    }
                    _overrunKeeperZone = -1;
                }
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
