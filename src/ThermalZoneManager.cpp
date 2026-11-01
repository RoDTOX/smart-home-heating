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
      // Placeholders until begin() loads the requirements §6 values from ParamStore
      _sensorTimeoutSec(1200UL),
      _hysteresis(0.3f),
      _windowThreshold(1.2f),
      _bootWarmupActive(true),
      _bootWarmupStartMs(0),
      _interlockDelayMs(180000UL),
      _pumpOverrunMs(180000UL),
      _openLoopBypass(false),
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

    // No installation data in firmware (FR-11.1): names, sensors and targets come from ParamStore
    static const uint8_t pins[MAX_THERMAL_ZONES] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3};
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        memset(&_configs[i], 0, sizeof(_configs[i]));
        _configs[i].id = i + 1;
        _configs[i].relayPin = pins[i];
        _configs[i].targetTemp = DEFAULT_SETPOINT_TEMP;
    }

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

static_assert(MAX_THERMAL_ZONES == PARAM_ZONES, "ParamStore zone records must match the zone count");
static_assert(sizeof(ScheduleConfig) <= 256, "ParamStore::saveBlob compares at most 256 bytes");

void ThermalZoneManager::loadConfig() {
    _interlockDelayMs = (uint32_t)paramStore.get(Param::InterlockDelay) * 1000UL;
    _pumpOverrunMs = (uint32_t)paramStore.get(Param::Overrun) * 1000UL;
    _hysteresis = paramStore.getf(Param::Hysteresis);
    _sensorTimeoutSec = (uint32_t)paramStore.get(Param::SensorTimeout);
    _freezeProtectionTemp = paramStore.getf(Param::FrostTemp);
    _windowThreshold = paramStore.getf(Param::WindowThreshold);
    _openLoopBypass = paramStore.get(Param::OpenLoopBypass) != 0;
    _actuatorNormallyClosed = paramStore.get(Param::ActuatorType) == 0;
    _pumpControlEnabled = paramStore.get(Param::PumpOutput) != 0;
    _autoMode = paramStore.state(StateKey::AutoMode) != 0;
    _lastAntiSeizeEpoch = paramStore.state(StateKey::AntiSeizeEpoch);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        const ZoneRecord& z = paramStore.zone(i);
        strlcpy(_configs[i].name, z.name, sizeof(_configs[i].name));
        strlcpy(_configs[i].sensorMac, z.sensorMac, sizeof(_configs[i].sensorMac));
        strlcpy(_configs[i].sensorName, z.sensorName, sizeof(_configs[i].sensorName));
        _configs[i].relayPin = z.relayPin;
        _configs[i].enabled = z.enabled != 0;
        _configs[i].tempOffset = z.offsetC10 / 10.0f;
        _configs[i].targetTemp = z.targetC10 / 10.0f;
        _tpi[i].integral = _tpi[i].savedIntegral = paramStore.learned(i) / 1000.0f;
    }
}

// Hands every zone record and state value to ParamStore, which writes only what changed (NFR-2.9).
void ThermalZoneManager::persistConfig() {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        ZoneRecord z;
        memset(&z, 0, sizeof(z));
        strlcpy(z.name, _configs[i].name, sizeof(z.name));
        strlcpy(z.sensorMac, _configs[i].sensorMac, sizeof(z.sensorMac));
        strlcpy(z.sensorName, _configs[i].sensorName, sizeof(z.sensorName));
        z.relayPin = _configs[i].relayPin;
        z.enabled = _configs[i].enabled ? 1 : 0;
        z.offsetC10 = (int16_t)lroundf(_configs[i].tempOffset * 10);
        z.targetC10 = (int16_t)lroundf(_configs[i].targetTemp * 10);
        paramStore.saveZone(i, z);
    }
    paramStore.saveState(StateKey::AutoMode, _autoMode ? 1 : 0);
    paramStore.saveState(StateKey::AntiSeizeEpoch, _lastAntiSeizeEpoch);
}

void ThermalZoneManager::loadSchedule() {
    ScheduleConfig loaded;
    if (!paramStore.loadBlob("cfg2", &loaded, sizeof(loaded))) {
        return;  // factory schedule (constructor)
    }
    bool valid = loaded.weekdayCount >= 1 && loaded.weekdayCount <= MAX_SCHEDULE_SLOTS &&
                 loaded.weekendCount >= 1 && loaded.weekendCount <= MAX_SCHEDULE_SLOTS;
    for (uint8_t l = 0; valid && l < LEVEL_COUNT; l++) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            float t = loaded.levelTemps[l][i];
            if (!(t >= 5.0f && t <= 30.0f)) valid = false;  // FR-29 range
        }
    }
    for (uint8_t n = 0; valid && n < MAX_SCHEDULE_SLOTS; n++) {
        const ScheduleSlot* lists[2] = {&loaded.weekday[n], &loaded.weekend[n]};
        uint8_t counts[2] = {loaded.weekdayCount, loaded.weekendCount};
        for (uint8_t w = 0; w < 2; w++) {
            if (n < counts[w] && (lists[w]->hour > 23 || lists[w]->minute > 59 || lists[w]->level >= LEVEL_COUNT)) {
                valid = false;
            }
        }
    }
    if (valid) {
        _scheduleConfig = loaded;
        Serial.println(F("[Thermal] Schedule loaded from NVS."));
    } else {
        netConsole.println(F("[Thermal] Stored schedule invalid, factory schedule used."));
    }
}

void ThermalZoneManager::saveSchedule() {
    paramStore.saveBlob("cfg2", &_scheduleConfig, sizeof(_scheduleConfig));
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active) {
    if (zoneUsingRelay(pin, SIZE_MAX) < 0) {
        // Pump, or a relay no zone uses: directly active HIGH (runs when energized), off = de-energized
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
    loadConfig();
    loadSchedule();
    evaluateSchedule(millis());

    _bootWarmupActive = true;
    _bootWarmupStartMs = millis();

    // NFR-1.4: every relay output driven closed, also those no zone uses after a reassignment
    static const uint8_t RELAY_PINS[] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3, PIN_RELAY_SPARE};
    for (uint8_t pin : RELAY_PINS) {
        pinMode(pin, OUTPUT);
        applyRelay(pin, false);
    }
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].state = _configs[i].enabled ? ZONE_WARMUP : ZONE_DISABLED;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = millis();
    }
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

int ThermalZoneManager::zoneUsingRelay(uint8_t pin, size_t exceptIndex) const {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (i != exceptIndex && _configs[i].relayPin == pin) return (int)i;
    }
    return -1;
}

bool ThermalZoneManager::setPumpControlEnabled(bool enabled) {
    if (_pumpControlEnabled == enabled) return true;
    // FR-11.3: the pump relay must not be a zone output
    if (enabled && zoneUsingRelay(getPumpRelayPin(), SIZE_MAX) >= 0) return false;
    _pumpControlEnabled = enabled;
    if (!enabled && _pumpActive) {
        applyRelay(PIN_RELAY_SPARE, false);
        _pumpActive = false;
    }
    paramStore.set(Param::PumpOutput, enabled ? 1 : 0);
    netConsole.printf("[Thermal] Circulation pump control via Relay 4 %s\n", enabled ? "ENABLED" : "DISABLED");
    return true;
}

void ThermalZoneManager::setActuatorNormallyClosed(bool nc) {
    if (_actuatorNormallyClosed == nc) return;
    _actuatorNormallyClosed = nc;
    paramStore.set(Param::ActuatorType, nc ? 0 : 1);
    // Refresh all zone relays with new logic polarity
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled) {
            applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged);
        }
    }
    netConsole.printf("[Thermal] Actuator type set to: %s\n", nc ? "NC (Normally Closed)" : "NO (Normally Open)");
}

bool ThermalZoneManager::setFreezeProtectionTemp(float temp) {
    if (!paramStore.setf(Param::FrostTemp, temp)) return false;
    _freezeProtectionTemp = paramStore.getf(Param::FrostTemp);
    netConsole.printf("[Thermal] Frost protection threshold: %.1f C\n", _freezeProtectionTemp);
    return true;
}

bool ThermalZoneManager::setSensorTimeoutSec(uint32_t sec) {
    if (!paramStore.set(Param::SensorTimeout, (int32_t)sec)) return false;
    _sensorTimeoutSec = sec;
    netConsole.printf("[Thermal] Sensor timeout: %u s\n", _sensorTimeoutSec);
    return true;
}

bool ThermalZoneManager::setHysteresis(float hyst) {
    if (!paramStore.setf(Param::Hysteresis, hyst)) return false;
    _hysteresis = paramStore.getf(Param::Hysteresis);
    netConsole.printf("[Thermal] Hysteresis (overshoot cut): %.2f C\n", _hysteresis);
    return true;
}

bool ThermalZoneManager::setWindowThreshold(float threshold) {
    if (!paramStore.setf(Param::WindowThreshold, threshold)) return false;
    _windowThreshold = paramStore.getf(Param::WindowThreshold);
    netConsole.printf("[Thermal] Window drop threshold (all zones): %.1f C\n", _windowThreshold);
    return true;
}

bool ThermalZoneManager::setInterlockDelaySec(uint32_t sec) {
    if (!paramStore.set(Param::InterlockDelay, (int32_t)sec)) return false;
    _interlockDelayMs = sec * 1000UL;
    netConsole.printf("[Thermal] Actuator interlock: %u s\n", sec);
    return true;
}

bool ThermalZoneManager::setOverrunSec(uint32_t sec) {
    if (!paramStore.set(Param::Overrun, (int32_t)sec)) return false;
    _pumpOverrunMs = sec * 1000UL;
    netConsole.printf("[Thermal] Overrun: %u s\n", sec);
    return true;
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
        persistConfig();
        netConsole.printf("[Thermal] Zone %u (%s) %s\n",
            _configs[index].id, _configs[index].name, enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setZoneName(size_t index, const char* name) {
    if (index < MAX_THERMAL_ZONES && name) {
        strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        _configs[index].name[sizeof(_configs[index].name) - 1] = '\0';
        persistConfig();
    }
}

void ThermalZoneManager::setSensorName(size_t index, const char* sensorName) {
    if (index < MAX_THERMAL_ZONES && sensorName) {
        strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        _configs[index].sensorName[sizeof(_configs[index].sensorName) - 1] = '\0';
        persistConfig();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor name '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorName);
    }
}

void ThermalZoneManager::setSensorMac(size_t index, const char* sensorMac) {
    if (index < MAX_THERMAL_ZONES && sensorMac) {
        strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        _configs[index].sensorMac[sizeof(_configs[index].sensorMac) - 1] = '\0';
        persistConfig();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor MAC '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac);
    }
}

void ThermalZoneManager::setRelayPin(size_t index, uint8_t pin) {
    if (index < MAX_THERMAL_ZONES && ParamStore::validRelayPin(pin)) {
        _configs[index].relayPin = pin;
        pinMode(pin, OUTPUT);
        applyRelay(pin, false);
        persistConfig();
    }
}

bool ThermalZoneManager::setTempOffset(size_t index, float offset) {
    if (index >= MAX_THERMAL_ZONES || offset < -3.0f || offset > 3.0f) return false;
    if (fabsf(_configs[index].tempOffset - offset) > 0.01f) {
        _configs[index].tempOffset = offset;
        persistConfig();
    }
    netConsole.printf("[Thermal] Zone %u (%s) sensor offset %+.1f C\n", _configs[index].id, _configs[index].name, offset);
    return true;
}

void ThermalZoneManager::mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, bool enabled) {
    if (index < MAX_THERMAL_ZONES) {
        if (name && strlen(name) > 0) strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        if (sensorMac) strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        if (sensorName) strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        if (ParamStore::validRelayPin(relayPin) && relayPin != _configs[index].relayPin) {
            // The caller refuses a move while the valve is energised; both outputs are driven closed here
            uint8_t oldPin = _configs[index].relayPin;
            _configs[index].relayPin = relayPin;
            pinMode(relayPin, OUTPUT);
            applyRelay(relayPin, false);
            applyRelay(oldPin, false);
        }
        if (targetTemp >= 15.0f && targetTemp <= 30.0f) _configs[index].targetTemp = targetTemp;
        _configs[index].enabled = enabled;
        persistConfig();
        netConsole.printf("[Thermal] Zone %u mapped: '%s' | MAC: %s | Pin: %u | Target: %.1f C\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac, _configs[index].relayPin, _configs[index].targetTemp);
    }
}


void ThermalZoneManager::setOpenLoopBypass(bool en) {
    _openLoopBypass = en;
    paramStore.set(Param::OpenLoopBypass, en ? 1 : 0);
}

void ThermalZoneManager::setAutoMode(bool autoMode) {
    _autoMode = autoMode;
    persistConfig();
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
    // Relay 4 is the pump only with pump control on; otherwise it may be a zone output (FR-11.3)
    if (!_autoMode && _pumpControlEnabled) {
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

// FR-8.4: the first state that applies from the top; architecture §7 "Displayed zone state"
const char* ThermalZoneManager::getStateCode(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return "DISABLED";
    const ThermalZoneStatus& st = _statuses[index];
    if (st.frostActive || st.state == ZONE_FROST_GUARD) return "FROST";
    if (!_configs[index].enabled || st.state == ZONE_DISABLED) return "DISABLED";
    if (st.sensorFault || st.state == ZONE_FAULT_TIMEOUT) return "FAULT";
    if (st.state == ZONE_WARMUP) return "ACQUIRING";
    if (st.state == ZONE_WINDOW_PAUSE || isWindowPauseActive(index)) return "WINDOW";
    if (st.relayEngaged || st.state == ZONE_WARMING_UP || st.state == ZONE_HEATING) return "HEATING";
    return "IDLE";
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
        cfg.levelTemps[LEVEL_DAY][i] = 23.0f;    // FR-29 factory levels
        cfg.levelTemps[LEVEL_NIGHT][i] = 21.0f;
        cfg.levelTemps[LEVEL_AWAY][i] = 18.0f;
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
    saveSchedule();
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
    saveSchedule();
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
    saveSchedule();
    evaluateSchedule(millis());
    netConsole.printf("[Thermal] Away mode %s\n", away ? "ON (all zones at away level)" : "OFF (back to schedule)");
}

void ThermalZoneManager::resetScheduleToDefault() {
    bool en = _scheduleConfig.enabled;
    _scheduleConfig = getFactoryDefaultSchedule();
    _scheduleConfig.enabled = en;
    saveSchedule();
    evaluateSchedule(millis());
    netConsole.println(F("[Thermal] Schedule reset to factory default."));
}

void ThermalZoneManager::setScheduleEnabled(bool enabled) {
    if (_scheduleConfig.enabled != enabled) {
        _scheduleConfig.enabled = enabled;
        saveSchedule();
        evaluateSchedule(millis());
        netConsole.printf("[Thermal] Schedule %s\n", enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setPreHeatEnabled(bool enabled) {
    if (_scheduleConfig.preHeatEnabled != enabled) {
        _scheduleConfig.preHeatEnabled = enabled;
        saveSchedule();
        evaluateSchedule(millis());
        netConsole.printf("[Thermal] Predictive Pre-Heat %s\n", enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setWindowDetectionEnabled(bool enabled) {
    if (_scheduleConfig.windowDetectionEnabled != enabled) {
        _scheduleConfig.windowDetectionEnabled = enabled;
        saveSchedule();
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
                paramStore.saveLearned(index, (int16_t)lroundf(t.integral * 1000));
                t.savedIntegral = t.integral;
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
    if (temp >= target + _hysteresis && elapsed < t.onMs) {
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

    float dropThreshold = _windowThreshold;

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
    json += "\"pumpOverrunSec\":" + String(_pumpOverrunMs / 1000) + ",";
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
        persistConfig();
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
        persistConfig();
    }

    // Handle ongoing Anti-Seize Exercise (20s duration)
    if (_antiSeizeRunning) {
        if (now - _antiSeizeStartMs >= 20000UL) {
            _antiSeizeRunning = false;
            time_t nowEpoch = time(nullptr);
            if (nowEpoch >= 1700000000) {
                _lastAntiSeizeEpoch = (uint32_t)nowEpoch;
            }
            persistConfig();

            // Turn off pump
            if (_pumpControlEnabled) {
                applyRelay(PIN_RELAY_SPARE, false);
                _pumpActive = false;
            }
            // Restore every relay to its zone's valve state; a zone switched off during the exercise closes
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                applyRelay(_configs[i].relayPin, _configs[i].enabled && _statuses[i].relayEngaged);
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
