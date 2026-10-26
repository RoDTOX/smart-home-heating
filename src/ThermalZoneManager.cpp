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
    strncpy(_activePresetId, "default", sizeof(_activePresetId));

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _windowStates[i].baselineTemp = -999.0f;
        _windowStates[i].baselineMs = 0;
        _windowStates[i].pauseActive = false;
        _windowStates[i].pauseStartMs = 0;
        _windowStates[i].pauseMinTemp = 999.0f;
        _windowStates[i].dropInsightLogged = false;
        _windowStates[i].lastDetectionEpoch = 0;
        _windowStates[i].lastDetectionDurationSec = 0;
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
        _statuses[i].holdAnchor = BIO_ANCHOR_HOME;
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
        if (len == sizeof(loaded) && loaded.weekdayCount > 0 && loaded.weekdayCount <= MAX_SCHEDULE_BLOCKS) {
            _scheduleConfig = loaded;
            Serial.println(F("[Thermal] Smart Schedule loaded from NVS."));
        } else {
            _scheduleConfig = getFactoryDefaultSchedule();
            Serial.println(F("[Thermal] Schedule initialized to modern factory blocks."));
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
            // Debounce NVS flash write by 3000ms to protect flash and smooth slider changes
            _nvsSavePending = true;
            _nvsSavePendingMs = millis() + 3000;
        }

        // If Bio-Schedule is active, mark as Temporary Hold (holds until next anchor transition)
        // Does NOT overwrite the schedule preset!
        if (_scheduleConfig.enabled && timeAndWeather.isTimeSynced()) {
            _statuses[index].isManualHold = true;
            _statuses[index].holdAnchor = _activeAnchor;
            netConsole.printf("[Thermal] Zone %u (%s) TEMPORARY HOLD set to %.1f C (holds until next anchor: %s)\n",
                _configs[index].id, _configs[index].name, target, getAnchorName(_nextAnchor));
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
        bool weekend = timeAndWeather.isWeekend();
        const BioAnchorConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
        BioAnchorId anc = _preHeatActive ? _nextAnchor : _activeAnchor;
        if (anc >= BIO_ANCHORS_COUNT) anc = BIO_ANCHOR_HOME;
        _configs[index].targetTemp = profile[anc].targets[index];
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

bool ThermalZoneManager::setAnchorName(bool weekend, BioAnchorId anchor, const char* name) {
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    size_t idx = (size_t)anchor;
    ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    if (idx < count && name) {
        strncpy(profile[idx].name, name, sizeof(profile[idx].name) - 1);
        profile[idx].name[sizeof(profile[idx].name) - 1] = 0;
        saveSchedulePreferences();
        return true;
    }
    return false;
}

uint8_t ThermalZoneManager::getPresetDays(const char* presetId) const {
    if (!presetId || strlen(presetId) == 0) return 0x7F;
    Preferences prefs;
    if (!const_cast<Preferences&>(prefs).begin("presets", true)) return 0x7F;
    char keyDays[16];
    snprintf(keyDays, sizeof(keyDays), "%s_days", presetId);
    uint8_t d = prefs.getUChar(keyDays, 0x7F);
    prefs.end();
    return d;
}

bool ThermalZoneManager::setPresetDays(const char* presetId, uint8_t daysMask) {
    if (!presetId || strlen(presetId) == 0) return false;
    Preferences prefs;
    if (!prefs.begin("presets", false)) return false;
    char keyDays[16];
    snprintf(keyDays, sizeof(keyDays), "%s_days", presetId);
    prefs.putUChar(keyDays, daysMask);
    prefs.end();
    return true;
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
    memset(&cfg, 0, sizeof(cfg));
    cfg.enabled = true;
    cfg.preHeatEnabled = true;
    cfg.windowDetectionEnabled = true;

    // Weekday: 4 standard blocks
    cfg.weekdayCount = 4;
    strncpy(cfg.weekday[0].name, "Dimineata", sizeof(cfg.weekday[0].name) - 1);
    cfg.weekday[0].hour = 6;
    cfg.weekday[0].minute = 30;
    cfg.weekday[0].targets[0] = 22.0f;
    cfg.weekday[0].targets[1] = 22.0f;
    cfg.weekday[0].targets[2] = 22.0f;

    strncpy(cfg.weekday[1].name, "Zi / Serviciu", sizeof(cfg.weekday[1].name) - 1);
    cfg.weekday[1].hour = 8;
    cfg.weekday[1].minute = 30;
    cfg.weekday[1].targets[0] = 20.0f;
    cfg.weekday[1].targets[1] = 20.0f;
    cfg.weekday[1].targets[2] = 20.0f;

    strncpy(cfg.weekday[2].name, "Seara / Acasa", sizeof(cfg.weekday[2].name) - 1);
    cfg.weekday[2].hour = 17;
    cfg.weekday[2].minute = 0;
    cfg.weekday[2].targets[0] = 22.5f;
    cfg.weekday[2].targets[1] = 22.5f;
    cfg.weekday[2].targets[2] = 22.0f;

    strncpy(cfg.weekday[3].name, "Noapte", sizeof(cfg.weekday[3].name) - 1);
    cfg.weekday[3].hour = 22;
    cfg.weekday[3].minute = 30;
    cfg.weekday[3].targets[0] = 20.0f;
    cfg.weekday[3].targets[1] = 20.0f;
    cfg.weekday[3].targets[2] = 20.0f;

    // Weekend: 4 blocks
    cfg.weekendCount = 4;
    strncpy(cfg.weekend[0].name, "Dimineata", sizeof(cfg.weekend[0].name) - 1);
    cfg.weekend[0].hour = 8;
    cfg.weekend[0].minute = 0;
    cfg.weekend[0].targets[0] = 22.5f;
    cfg.weekend[0].targets[1] = 22.5f;
    cfg.weekend[0].targets[2] = 22.0f;

    strncpy(cfg.weekend[1].name, "Zi", sizeof(cfg.weekend[1].name) - 1);
    cfg.weekend[1].hour = 12;
    cfg.weekend[1].minute = 0;
    cfg.weekend[1].targets[0] = 22.5f;
    cfg.weekend[1].targets[1] = 22.5f;
    cfg.weekend[1].targets[2] = 22.0f;

    strncpy(cfg.weekend[2].name, "Seara", sizeof(cfg.weekend[2].name) - 1);
    cfg.weekend[2].hour = 18;
    cfg.weekend[2].minute = 0;
    cfg.weekend[2].targets[0] = 22.5f;
    cfg.weekend[2].targets[1] = 22.5f;
    cfg.weekend[2].targets[2] = 22.0f;

    strncpy(cfg.weekend[3].name, "Noapte", sizeof(cfg.weekend[3].name) - 1);
    cfg.weekend[3].hour = 23;
    cfg.weekend[3].minute = 0;
    cfg.weekend[3].targets[0] = 20.5f;
    cfg.weekend[3].targets[1] = 20.5f;
    cfg.weekend[3].targets[2] = 20.0f;

    return cfg;
}

BioScheduleConfig ThermalZoneManager::getFactoryEcoSchedule() {
    BioScheduleConfig cfg = getFactoryDefaultSchedule();
    for (uint8_t i = 0; i < cfg.weekdayCount; i++) {
        cfg.weekday[i].targets[0] = 18.0f;
        cfg.weekday[i].targets[1] = 18.5f;
        cfg.weekday[i].targets[2] = 18.0f;
    }
    for (uint8_t i = 0; i < cfg.weekendCount; i++) {
        cfg.weekend[i].targets[0] = 18.0f;
        cfg.weekend[i].targets[1] = 18.5f;
        cfg.weekend[i].targets[2] = 18.0f;
    }
    return cfg;
}

BioScheduleConfig ThermalZoneManager::getFactoryWfhSchedule() {
    BioScheduleConfig cfg = getFactoryDefaultSchedule();
    for (uint8_t i = 0; i < cfg.weekdayCount; i++) {
        if (cfg.weekday[i].hour >= 8 && cfg.weekday[i].hour < 22) {
            cfg.weekday[i].targets[0] = 22.5f;
            cfg.weekday[i].targets[1] = 22.5f;
            cfg.weekday[i].targets[2] = 22.0f;
        }
    }
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


bool ThermalZoneManager::addBlock(bool weekend, const char* name, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]) {
    if (hour > 23 || minute > 59) return false;
    uint8_t& count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count >= MAX_SCHEDULE_BLOCKS) return false;

    ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    size_t newIdx = count;
    strncpy(profile[newIdx].name, (name && strlen(name) > 0) ? name : "Interval", sizeof(profile[newIdx].name) - 1);
    profile[newIdx].name[sizeof(profile[newIdx].name) - 1] = 0;
    profile[newIdx].hour = hour;
    profile[newIdx].minute = minute;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        profile[newIdx].targets[i] = targets ? targets[i] : 21.0f;
    }
    count++;

    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Added Schedule Block '%s' (%s) at %02u:%02u (total: %u)\n",
                      profile[newIdx].name, weekend ? "Weekend" : "Weekday", hour, minute, count);
    return true;
}

bool ThermalZoneManager::updateBlock(bool weekend, size_t index, const char* name, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]) {
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (index >= count || hour > 23 || minute > 59) return false;

    ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    if (name && strlen(name) > 0) {
        strncpy(profile[index].name, name, sizeof(profile[index].name) - 1);
        profile[index].name[sizeof(profile[index].name) - 1] = 0;
    }
    profile[index].hour = hour;
    profile[index].minute = minute;
    if (targets) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (targets[i] >= 5.0f && targets[i] <= 35.0f) {
                profile[index].targets[i] = targets[i];
            }
        }
    }

    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Updated Schedule Block #%u '%s' (%s): %02u:%02u [%.1f, %.1f, %.1f]\n",
                      (unsigned int)index, profile[index].name, weekend ? "Weekend" : "Weekday",
                      hour, minute, profile[index].targets[0], profile[index].targets[1], profile[index].targets[2]);
    return true;
}

bool ThermalZoneManager::deleteBlock(bool weekend, size_t index) {
    uint8_t& count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (index >= count || count <= 1) return false;

    ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    for (size_t i = index; i < (size_t)count - 1; i++) {
        profile[i] = profile[i + 1];
    }
    count--;

    saveSchedulePreferences();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Deleted Schedule Block #%u (%s) (remaining: %u)\n",
                      (unsigned int)index, weekend ? "Weekend" : "Weekday", count);
    return true;
}

const char* ThermalZoneManager::getActiveBlockName() const {
    bool weekend = timeAndWeather.isWeekend();
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count == 0) return "Standard";
    const ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    size_t idx = _activeBlockIdx < count ? _activeBlockIdx : 0;
    return profile[idx].name;
}

const char* ThermalZoneManager::getNextBlockName() const {
    bool weekend = timeAndWeather.isWeekend();
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count == 0) return "Standard";
    const ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    size_t idx = _nextBlockIdx < count ? _nextBlockIdx : 0;
    return profile[idx].name;
}

bool ThermalZoneManager::updateAnchor(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]) {
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    size_t idx = (size_t)anchor;
    ScheduleBlockConfig* profile = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    const char* currentName = (idx < count && strlen(profile[idx].name) > 0) ? profile[idx].name : getAnchorName(anchor);
    if (idx >= count && count < MAX_SCHEDULE_BLOCKS) {
        return addBlock(weekend, currentName, hour, minute, targets);
    }
    return updateBlock(weekend, idx, currentName, hour, minute, targets);
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
        json += "{\"id\":\"" + String(factories[i].id) + "\",\"name\":\"" + name + "\",\"type\":\"factory\",\"saved\":" + (saved ? "true" : "false") + ",\"daysMask\":" + String(getPresetDays(factories[i].id)) + "}";
    }

    const char* slots[] = {"custom1", "custom2", "custom3", "custom4"};
    for (int i = 0; i < 4; i++) {
        char keyCfg[16], keyName[16];
        snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", slots[i]);
        snprintf(keyName, sizeof(keyName), "%s_name", slots[i]);
        bool saved = prefsOpen && prefs.isKey(keyCfg);
        String name = (saved && prefs.isKey(keyName)) ? prefs.getString(keyName, slots[i]) : (String("Slot Liber (") + slots[i] + ")");
        json += ",{\"id\":\"" + String(slots[i]) + "\",\"name\":\"" + name + "\",\"type\":\"user\",\"saved\":" + (saved ? "true" : "false") + ",\"daysMask\":" + String(getPresetDays(slots[i])) + "}";
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
    char oldPreset[32];
    strncpy(oldPreset, _activePresetId, sizeof(oldPreset) - 1);
    oldPreset[sizeof(oldPreset) - 1] = '\0';

    _scheduleConfig = targetCfg;
    _scheduleConfig.enabled = currentEn;
    strncpy(_activePresetId, presetId, sizeof(_activePresetId) - 1);
    _activePresetId[sizeof(_activePresetId) - 1] = '\0';
    saveSchedulePreferences();
    evaluateSchedule(millis());

    // Synchronize individual zone targetTemps to match the new preset's active block targets
    bool weekend = timeAndWeather.isTimeSynced() ? timeAndWeather.isWeekend() : false;
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    const ScheduleBlockConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    size_t blkIdx = (_activeBlockIdx < count) ? _activeBlockIdx : 0;
    bool zChanged = false;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        float newT = prof[blkIdx].targets[i];
        if (fabsf(_configs[i].targetTemp - newT) > 0.05f) {
            _configs[i].targetTemp = newT;
            zChanged = true;
        }
    }
    if (zChanged) {
        saveToPreferences();
    }

    netConsole.printf("[Schedule][PRESET] Preset switched: '%s' -> '%s'\n", oldPreset, presetId);
    return true;
}

bool ThermalZoneManager::savePreset(const char* presetId, const char* name, uint8_t daysMask) {
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
    char keyDays[16];
    snprintf(keyDays, sizeof(keyDays), "%s_days", presetId);
    prefs.putUChar(keyDays, daysMask);
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
    if (!timeAndWeather.isTimeSynced()) return;

    // Day of week auto-switching based on preset daysMask
    int wday = timeAndWeather.getDayOfWeekIndex();
    uint8_t currentDayBit = (wday == 0) ? (1 << 6) : (1 << (wday - 1));
    uint8_t activePresetDays = getPresetDays(_activePresetId);
    if ((activePresetDays & currentDayBit) == 0) {
        const char* candidatePresets[] = {"default", "wfh", "eco", "custom1", "custom2", "custom3", "custom4"};
        for (size_t p = 0; p < sizeof(candidatePresets)/sizeof(candidatePresets[0]); p++) {
            if (getPresetDays(candidatePresets[p]) & currentDayBit) {
                applyPreset(candidatePresets[p]);
                break;
            }
        }
    }

    bool weekend = timeAndWeather.isWeekend();
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count == 0) count = 1;
    const ScheduleBlockConfig* blocks = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;

    int curHour = timeAndWeather.getHour();
    int curMin = timeAndWeather.getMinute();
    uint32_t curMinutes = curHour * 60 + curMin;

    // Find active block (latest block where blockMinutes <= curMinutes)
    size_t activeIdx = 0;
    bool found = false;
    for (int i = (int)count - 1; i >= 0; i--) {
        uint32_t blkMinutes = blocks[i].hour * 60 + blocks[i].minute;
        if (curMinutes >= blkMinutes) {
            activeIdx = (size_t)i;
            found = true;
            break;
        }
    }
    if (!found) {
        activeIdx = count - 1;
    }

    size_t nextIdx = (activeIdx + 1) % count;
    uint32_t nextMinutes = blocks[nextIdx].hour * 60 + blocks[nextIdx].minute;
    if (nextMinutes <= curMinutes) {
        _minsToNextAnchor = (1440 - curMinutes) + nextMinutes;
    } else {
        _minsToNextAnchor = nextMinutes - curMinutes;
    }

    size_t prevBlock = _activeBlockIdx;
    _activeBlockIdx = activeIdx;
    _nextBlockIdx = nextIdx;
    _activeAnchor = (BioAnchorId)(activeIdx < BIO_ANCHORS_COUNT ? activeIdx : 3);
    _nextAnchor = (BioAnchorId)(nextIdx < BIO_ANCHORS_COUNT ? nextIdx : 0);

    // Pre-Heat calculation (45 mins anticipative underfloor heating)
    _preHeatActive = false;
    if (_scheduleConfig.preHeatEnabled && _minsToNextAnchor <= 45) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (blocks[nextIdx].targets[i] > blocks[activeIdx].targets[i] + 0.2f) {
                _preHeatActive = true;
                break;
            }
        }
    }

    // Block transition: clear manual holds
    if (prevBlock != _activeBlockIdx) {
        netConsole.printf("[Thermal][SCHEDULE] Block transition: %s -> %s (Profile: %s, Time: %02d:%02d)\n",
            blocks[prevBlock].name, blocks[_activeBlockIdx].name,
            weekend ? "Weekend" : "Weekday", curHour, curMin);

        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (_statuses[i].isManualHold) {
                _statuses[i].isManualHold = false;
                netConsole.printf("[Thermal] Zone %u (%s) Temporary Hold expired naturally at block change.\n",
                    (unsigned int)_configs[i].id, _configs[i].name);
            }
        }
    }

    // Synchronize static targets
    if (_scheduleConfig.enabled) {
        const ScheduleBlockConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
        size_t blkToUse = _preHeatActive ? _nextBlockIdx : _activeBlockIdx;
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (!_statuses[i].isManualHold) {
                float target = prof[blkToUse].targets[i];
                if (fabsf(_configs[i].targetTemp - target) > 0.05f) {
                    _configs[i].targetTemp = target;
                }
            }
        }
    }
}

float ThermalZoneManager::getEffectiveTargetTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return DEFAULT_SETPOINT_TEMP;
    if (_statuses[index].isManualHold) {
        return _configs[index].targetTemp;
    }
    if (!_scheduleConfig.enabled || !timeAndWeather.isTimeSynced()) {
        return _configs[index].targetTemp;
    }
    bool weekend = timeAndWeather.isWeekend();
    uint8_t count = weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount;
    if (count == 0) return _configs[index].targetTemp;
    const ScheduleBlockConfig* prof = weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday;
    size_t blkToUse = _preHeatActive ? (_nextBlockIdx % count) : (_activeBlockIdx % count);
    return prof[blkToUse].targets[index];
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
    json.reserve(2560);
    json = "{";
    json += "\"status\":\"ok\",";
    json += "\"enabled\":" + String(_scheduleConfig.enabled ? "true" : "false") + ",";
    json += "\"preHeatEnabled\":" + String(_scheduleConfig.preHeatEnabled ? "true" : "false") + ",";
    json += "\"windowDetectionEnabled\":" + String(_scheduleConfig.windowDetectionEnabled ? "true" : "false") + ",";
    json += "\"activePreset\":\"" + String(_activePresetId) + "\",";
    json += "\"activeBlockIndex\":" + String(_activeBlockIdx) + ",";
    json += "\"activeBlockName\":\"" + String(getActiveBlockName()) + "\",";
    json += "\"nextBlockIndex\":" + String(_nextBlockIdx) + ",";
    json += "\"nextBlockName\":\"" + String(getNextBlockName()) + "\",";
    json += "\"minsToNextBlock\":" + String(_minsToNextAnchor) + ",";
    json += "\"activeAnchor\":\"" + String(getActiveBlockName()) + "\",";
    json += "\"nextAnchor\":\"" + String(getNextBlockName()) + "\",";
    json += "\"minsToNextAnchor\":" + String(_minsToNextAnchor) + ",";
    json += "\"preHeatActive\":" + String(_preHeatActive ? "true" : "false") + ",";
    json += "\"isWeekend\":" + String(timeAndWeather.isWeekend() ? "true" : "false") + ",";
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
        json += "\"effectiveTarget\":" + String(getEffectiveTargetTemp(i), 1) + ",";
        json += "\"staticTarget\":" + String(_configs[i].targetTemp, 1) + ",";
        json += "\"isManualHold\":" + String(_statuses[i].isManualHold ? "true" : "false") + ",";
        json += "\"windowPauseActive\":" + String(isWindowPauseActive(i) ? "true" : "false") + ",";
        json += "\"windowPauseRemainingSec\":" + String(getWindowPauseRemainingSec(i));
        json += "}";
    }
    json += "],";

    auto serializeProfile = [&](const ScheduleBlockConfig* blocks, uint8_t count, const char* profName) {
        String s = "\"" + String(profName) + "\":{";
        s += "\"count\":" + String(count) + ",";
        s += "\"blocks\":[";
        for (uint8_t i = 0; i < count; i++) {
            if (i > 0) s += ",";
            s += "{";
            s += "\"index\":" + String(i) + ",";
            s += "\"name\":\"" + String(blocks[i].name) + "\",";
            s += "\"hour\":" + String(blocks[i].hour) + ",";
            s += "\"minute\":" + String(blocks[i].minute) + ",";
            s += "\"targets\":[" + String(blocks[i].targets[0], 1) + "," + String(blocks[i].targets[1], 1) + "," + String(blocks[i].targets[2], 1) + "]";
            s += "}";
        }
        s += "]";

        const char* legacyKeys[] = {"sleep", "wake", "away", "home"};
        for (uint8_t i = 0; i < 4 && i < count; i++) {
            s += ",\"" + String(legacyKeys[i]) + "\":{";
            s += "\"name\":\"" + String(blocks[i].name) + "\",";
            s += "\"hour\":" + String(blocks[i].hour) + ",";
            s += "\"minute\":" + String(blocks[i].minute) + ",";
            s += "\"targets\":[" + String(blocks[i].targets[0], 1) + "," + String(blocks[i].targets[1], 1) + "," + String(blocks[i].targets[2], 1) + "]";
            s += "}";
        }
        s += "}";
        return s;
    };

    json += serializeProfile(_scheduleConfig.weekday, _scheduleConfig.weekdayCount, "weekday") + ",";
    json += serializeProfile(_scheduleConfig.weekend, _scheduleConfig.weekendCount, "weekend");
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
                            netConsole.printf("[Thermal] Zone %u (%s) heat demand: current %.2f C <= start threshold %.2f C (target: %.1f C, hyst: +/-%.2f C%s)\n",
                                _configs[i].id, _configs[i].name,
                                _statuses[i].currentTemp,
                                target - _configs[i].hysteresis,
                                target,
                                _configs[i].hysteresis,
                                _scheduleConfig.enabled ? (_preHeatActive ? ", PRE-HEAT" : ", BIO") : "");
                        }
                    } else {
                        if (_statuses[i].currentTemp >= (target + _configs[i].hysteresis)) {
                            _statuses[i].demand = false;
                            // Note: State transition to ZONE_IDLE and relay cut-off are safely executed in Step 2 below
                            netConsole.printf("[Thermal] Zone %u (%s) target reached: current %.2f C >= stop threshold %.2f C (target: %.1f C, hyst: +/-%.2f C%s%s)\n",
                                _configs[i].id, _configs[i].name,
                                _statuses[i].currentTemp,
                                target + _configs[i].hysteresis,
                                target,
                                _configs[i].hysteresis,
                                _statuses[i].isManualHold ? ", HOLD" : "",
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
