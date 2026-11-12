#include "ThermalZoneManager.h"
#include "TelemetryHistoryManager.h"
#include <LittleFS.h>
#include "NetworkConsole.h"
#include "OutputBank.h"
#include "EventLog.h"
#include "BoilerRadioTrigger.h"
#include "BleSensorScanner.h"
#include "DiagnosticEngine.h"
#include "TimeAndWeatherService.h"
#include "ZoneController.h"
#include "FaultInjector.h"
#include "HeatUpModel.h"
#include "SolarThrottle.h"
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
        _windowStates[i].bucketStartMs = 0;
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
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) clearZoneSlot(i);

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
        _zoneRuntimeTodaySec[i] = 0;
        _zoneRuntimeYesterdaySec[i] = 0;
        _zoneRuntimeMsAcc[i] = 0;
    }
    _lastZoneRuntimeMs = 0;
    _lastDay = 0;
}

static_assert(MAX_THERMAL_ZONES == PARAM_ZONES, "ParamStore zone records must match the zone count");
static_assert(MAX_THERMAL_ZONES <= SafetySupervisor::MAX_ZONES, "SafetySupervisor zone capacity must cover MAX_THERMAL_ZONES");
static_assert(sizeof(ScheduleConfig) <= 1984, "ScheduleConfig must fit in single NVS blob");

// A slot that is not a zone: no output (pin 0), switched off, so no loop over the slots can act on it
void ThermalZoneManager::clearZoneSlot(size_t i) {
    memset(&_configs[i], 0, sizeof(_configs[i]));
    _configs[i].id = i + 1;
    _configs[i].targetTemp = DEFAULT_SETPOINT_TEMP;
    _statuses[i] = ThermalZoneStatus();
    _statuses[i].state = ZONE_DISABLED;
    _tpi[i] = {0, 0, 0.0f, 0.0f, 0.0f, 0.0f};
    _windowStates[i] = WindowDetectionState();
    _windowStates[i].baselineTemp = -999.0f;
    _windowStates[i].bucketStartMs = 0;
    _windowStates[i].pauseMinTemp = 999.0f;
}

void ThermalZoneManager::loadConfig() {
    _safety.setInterlockDelayMs((uint32_t)paramStore.get(Param::InterlockDelay) * 1000UL);
    _safety.setPumpOverrunMs((uint32_t)paramStore.get(Param::Overrun) * 1000UL);
    _hysteresis = paramStore.getf(Param::Hysteresis);
    _sensorTimeoutSec = (uint32_t)paramStore.get(Param::SensorTimeout);
    _safety.setFreezeProtectionTemp(paramStore.getf(Param::FrostTemp));
    _windowThreshold = paramStore.getf(Param::WindowThreshold);

    _heatingType = (uint8_t)paramStore.get(Param::HeatingType);
    _openLoopBypass = paramStore.get(Param::OpenLoopBypass) != 0;
    outputBank.setNormallyClosed(paramStore.get(Param::ActuatorType) == 0);
    _pumpControlEnabled = paramStore.get(Param::PumpOutput) != 0;
    _autoMode = paramStore.state(StateKey::AutoMode) != 0;
    _manualStartMs = millis();
    _seasonHeating = paramStore.state(StateKey::SeasonHeating) != 0;
    _daysOn = (uint8_t)paramStore.state(StateKey::SeasonDaysOn);
    _daysOff = (uint8_t)paramStore.state(StateKey::SeasonDaysOff);
    {
        uint8_t sm = (uint8_t)paramStore.get(Param::SeasonMode);
        if (sm == 1) _seasonHeating = true;
        if (sm == 2) _seasonHeating = false;
    }
    _lastAntiSeizeEpoch = paramStore.state(StateKey::AntiSeizeEpoch);

    uint32_t stored = paramStore.state(StateKey::ZoneCount);
    _zoneCount = (uint8_t)(stored > MAX_THERMAL_ZONES ? 0 : stored);
    for (size_t i = 0; i < _zoneCount; i++) {
        const ZoneRecord z = paramStore.zone(i);
        strlcpy(_configs[i].name, z.name, sizeof(_configs[i].name));
        strlcpy(_configs[i].sensorMac, z.sensorMac, sizeof(_configs[i].sensorMac));
        strlcpy(_configs[i].sensorName, z.sensorName, sizeof(_configs[i].sensorName));
        _configs[i].relayPin = z.relayPin;
        _configs[i].enabled = z.enabled != 0;
        _configs[i].solarGain = z.solarGain != 0;
        _configs[i].circuits = (z.circuits >= 1 && z.circuits <= 16) ? z.circuits : ((i == 0) ? 3 : 2);
        _configs[i].humidityComp = z.humidityComp != 0;
        _configs[i].heatingType = z.heatingType <= 1 ? z.heatingType : 0;
        _configs[i].tempOffset = z.offsetC10 / 10.0f;
        _configs[i].targetTemp = z.targetC10 / 10.0f;
        _tpi[i].integral = _tpi[i].savedIntegral = paramStore.learned(i) / 1000.0f;
    }
    // A relay output serves one zone (FR-11.3): a record left half moved by a power cut is switched off, not trusted
    for (size_t i = 0; i < _zoneCount; i++) {
        for (size_t j = 0; j < i; j++) {
            if (_configs[i].relayPin == _configs[j].relayPin && _configs[i].enabled) {
                netConsole.printf("[Params] zone %u shares relay %u with zone %u: switched off\n", (unsigned)(i + 1),
                                  _configs[i].relayPin, (unsigned)(j + 1));
                _configs[i].enabled = false;
            }
        }
    }
}

// Hands every zone record and state value to ParamStore, which writes only what changed (NFR-2.9).
void ThermalZoneManager::persistConfig() {
    paramStore.saveState(StateKey::ZoneCount, _zoneCount);
    for (size_t i = 0; i < _zoneCount; i++) {
        ZoneRecord z;
        memset(&z, 0, sizeof(z));
        strlcpy(z.name, _configs[i].name, sizeof(z.name));
        strlcpy(z.sensorMac, _configs[i].sensorMac, sizeof(z.sensorMac));
        strlcpy(z.sensorName, _configs[i].sensorName, sizeof(z.sensorName));
        z.relayPin = _configs[i].relayPin;
        z.enabled = _configs[i].enabled ? 1 : 0;
        z.solarGain = _configs[i].solarGain ? 1 : 0;
        z.circuits = _configs[i].circuits >= 1 && _configs[i].circuits <= 16 ? _configs[i].circuits : 1;
        z.humidityComp = _configs[i].humidityComp ? 1 : 0;
        z.heatingType = _configs[i].heatingType <= 1 ? _configs[i].heatingType : 0;
        z.offsetC10 = (int16_t)lroundf(_configs[i].tempOffset * 10);
        z.targetC10 = (int16_t)lroundf(_configs[i].targetTemp * 10);
        paramStore.saveZone(i, z);
    }
    paramStore.saveState(StateKey::AutoMode, _autoMode ? 1 : 0);
    paramStore.saveState(StateKey::AntiSeizeEpoch, _lastAntiSeizeEpoch);
}

// Rule R-A8: debounced NVS writes run in loop(), not in the control task
void ThermalZoneManager::flushPendingNvs() {
    uint32_t now = millis();
    if (_nvsSavePending && now >= _nvsSavePendingMs) {
        _nvsSavePending = false;
        persistConfig();
    }
}

// The schedule as stored up to migration step 1: level temperatures for exactly three zones
struct ScheduleConfigV1 {
    bool enabled;
    bool preHeatEnabled;
    bool windowDetectionEnabled;
    bool away;
    float levelTemps[LEVEL_COUNT][3];
    uint8_t weekdayCount;
    ScheduleSlot weekday[MAX_SCHEDULE_SLOTS];
    uint8_t weekendCount;
    ScheduleSlot weekend[MAX_SCHEDULE_SLOTS];
};

void ThermalZoneManager::loadSchedule() {
    ScheduleConfig loaded;
    if (paramStore.loadBlob("cfg3", &loaded, sizeof(loaded))) {
        _scheduleConfig = loaded;
        if (_scheduleConfig.momentNames[0][0] == '\0') {
            strlcpy(_scheduleConfig.momentNames[0], "Trezire", sizeof(_scheduleConfig.momentNames[0]));
            strlcpy(_scheduleConfig.momentNames[1], "Plecat", sizeof(_scheduleConfig.momentNames[1]));
            strlcpy(_scheduleConfig.momentNames[2], "Sear\xC4\x83", sizeof(_scheduleConfig.momentNames[2]));
            strlcpy(_scheduleConfig.momentNames[3], "Noapte", sizeof(_scheduleConfig.momentNames[3]));
            saveSchedule();
        }
        Serial.println(F("[Thermal] Schedule v3 loaded from NVS."));
        return;
    }
    ScheduleConfigV1 old;
    if (paramStore.loadBlob("cfg2", &old, sizeof(old))) {
        loaded = getFactoryDefaultSchedule();
        loaded.enabled = old.enabled;
        loaded.preHeatEnabled = old.preHeatEnabled;
        loaded.windowDetectionEnabled = old.windowDetectionEnabled;
        loaded.away = old.away;
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            for (size_t i = 0; i < 3; i++) loaded.levelTemps[l][i] = old.levelTemps[l][i];
        }
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            loaded.awayTemps[i] = encodeTempByte(loaded.levelTemps[LEVEL_AWAY][i]);
        }
        loaded.weekdayCount = old.weekdayCount;
        loaded.weekendCount = old.weekendCount;
        memcpy(loaded.weekday, old.weekday, sizeof(loaded.weekday));
        memcpy(loaded.weekend, old.weekend, sizeof(loaded.weekend));
        _scheduleConfig = loaded;
        saveSchedule();
        Serial.println(F("[Thermal] Schedule migrated from cfg2 to cfg3."));
        return;
    }
    _scheduleConfig = getFactoryDefaultSchedule();
    saveSchedule();
}

void ThermalZoneManager::saveSchedule() {
    paramStore.saveBlob("cfg3", &_scheduleConfig, sizeof(_scheduleConfig));
    ScheduleConfigV1 old;
    memset(&old, 0, sizeof(old));
    old.enabled = _scheduleConfig.enabled;
    old.preHeatEnabled = _scheduleConfig.preHeatEnabled;
    old.windowDetectionEnabled = _scheduleConfig.windowDetectionEnabled;
    old.away = _scheduleConfig.away;
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
        for (size_t i = 0; i < 3; i++) old.levelTemps[l][i] = _scheduleConfig.levelTemps[l][i];
    }
    old.weekdayCount = _scheduleConfig.weekdayCount;
    old.weekendCount = _scheduleConfig.weekendCount;
    memcpy(old.weekday, _scheduleConfig.weekday, sizeof(old.weekday));
    memcpy(old.weekend, _scheduleConfig.weekend, sizeof(old.weekend));
    paramStore.saveBlob("cfg2", &old, sizeof(old));
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active, uint8_t reason) {
    // The pin level and the NC / NO polarity are the business of OutputBank (R-A1, FR-15.1)
    int z = zoneUsingRelay(pin, SIZE_MAX);
    if (z < 0) outputBank.applyDirect(pin, active, reason);  // pump, or an output no zone uses
    else outputBank.applyValve(pin, active, reason, _configs[z].id);
}

// FR-62.1: the boiler side at the moment of an entry; reads state only
uint8_t ThermalZoneManager::eventContext() const {
    uint8_t c = 0;
    if (boilerRadio.isLastCommandOn()) c |= EVCTX_BOILER_ON;
    if (_safety.isOverrunActive()) c |= EVCTX_OVERRUN;
    if (boilerRadio.getPendingAction() == PENDING_OFF) c |= EVCTX_PENDING_OFF;
    if (boilerRadio.getPendingAction() == PENDING_ON) c |= EVCTX_PENDING_ON;
    if (_autoMode) c |= EVCTX_AUTO;
    return c;
}

void ThermalZoneManager::begin() {
    eventLog.setContextFn([]() { return thermalZones.eventContext(); });
    boilerRadio.setOnPermit([]() { return thermalZones.boilerOnPermitted(); });  // FR-2, every ON source
    zoneController.begin();
    loadConfig();
    loadSchedule();
    evaluateSchedule(millis());
    loadRuntimeFromFlash();

    _bootWarmupActive = true;
    _bootWarmupStartMs = millis();

    // NFR-1.4: every relay output driven closed, also those no zone uses after a reassignment
    static const uint8_t RELAY_PINS[] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3, PIN_RELAY_SPARE};
    for (uint8_t pin : RELAY_PINS) {
        outputBank.claim(pin);
        applyRelay(pin, false, RS_BOOT);
    }
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].state = _configs[i].enabled ? ZONE_WARMUP : ZONE_DISABLED;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = millis();
    }
    _pumpActive = false;

    Serial.printf("[Thermal] Initialized: %u zones, Actuators: %s, Pump Relay 4: %s\n", (unsigned)_zoneCount,
        outputBank.normallyClosed() ? "NC (Normally Closed)" : "NO (Normally Open)",
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
        applyRelay(PIN_RELAY_SPARE, false, RS_CONFIG);
        _pumpActive = false;
    }
    paramStore.set(Param::PumpOutput, enabled ? 1 : 0);
    netConsole.printf("[Thermal] Circulation pump control via Relay 4 %s\n", enabled ? "ENABLED" : "DISABLED");
    return true;
}

bool ThermalZoneManager::isActuatorNormallyClosed() const { return outputBank.normallyClosed(); }

void ThermalZoneManager::setActuatorNormallyClosed(bool nc) {
    if (outputBank.normallyClosed() == nc) return;
    outputBank.setNormallyClosed(nc);
    paramStore.set(Param::ActuatorType, nc ? 0 : 1);
    // Refresh all zone relays with new logic polarity
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged, RS_CONFIG);  // a disabled zone may be energised (frost, FR-3 hold)
    }
    netConsole.printf("[Thermal] Actuator type set to: %s\n", nc ? "NC (Normally Closed)" : "NO (Normally Open)");
}

bool ThermalZoneManager::setFreezeProtectionTemp(float temp) {
    if (temp < paramStore.getf(Param::ValidMin) + 0.05f) return false;  // validMin stays below frostTemp (FR-53)
    if (!paramStore.setf(Param::FrostTemp, temp)) return false;
    _safety.setFreezeProtectionTemp(paramStore.getf(Param::FrostTemp));
    netConsole.printf("[Thermal] Frost protection threshold: %.1f C\n", _safety.getFreezeProtectionTemp());
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
    _safety.setInterlockDelayMs(sec * 1000UL);
    netConsole.printf("[Thermal] Actuator interlock: %u s\n", sec);
    return true;
}

bool ThermalZoneManager::setOverrunSec(uint32_t sec) {
    if (!paramStore.set(Param::Overrun, (int32_t)sec)) return false;
    _safety.setPumpOverrunMs(sec * 1000UL);
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
            applyRelay(_configs[i].relayPin, true, RS_EXERCISE);
        }
    }
    // Start circulation pump if enabled
    if (_pumpControlEnabled) {
        applyRelay(PIN_RELAY_SPARE, true, RS_EXERCISE);
        _pumpActive = true;
    }
}

void ThermalZoneManager::setTargetTemperature(size_t index, float target) {
    if (index < MAX_THERMAL_ZONES) {
        if (target < 15.0f) target = 15.0f;
        if (target > TARGET_MAX_C) target = TARGET_MAX_C;
        if (fabsf(_configs[index].targetTemp - target) > 0.05f) {
            _configs[index].targetTemp = target;
            // Debounce NVS flash write by 3000ms to protect flash and smooth slider changes
            _nvsSavePending = true;
            _nvsSavePendingMs = millis() + 3000;
        }

        // A set target is a hold: it ends after boostDuration, or at the next slot while the schedule runs (FR-39)
        _statuses[index].isManualHold = true;
        _statuses[index].holdStartMs = millis();
        if (_scheduleConfig.enabled && timeAndWeather.isTimeSynced()) {
            netConsole.printf("[Thermal] Zone %u (%s) TEMPORARY HOLD set to %.1f C (holds until next slot: %s in %u min)\n",
                _configs[index].id, _configs[index].name, target, getLevelName(_nextLevel), _minsToNextSlot);
        } else {
            netConsole.printf("[Thermal] Zone %u (%s) new target temp: %.1f C\n",
                _configs[index].id, _configs[index].name, target);
        }
    }
}

// FR-1.2: a boost is 0.5 - 2.0 C above the schedule level in 0.5 C steps; a target at or below it is a plain hold
bool ThermalZoneManager::setTargetTemperatureChecked(size_t index, float target) {
    if (index >= MAX_THERMAL_ZONES || target > TARGET_MAX_C + 0.05f) return false;  // C-36: refused, never cut
    float up = target - getScheduleTargetTemp(index);
    if (up > 0.05f) {
        float steps = up / 0.5f;
        if (up > 2.05f || fabsf(steps - roundf(steps)) > 0.1f) return false;
    }
    setTargetTemperature(index, target);
    return true;
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
            arbitrateNow();  // the valve closes unless FR-3 holds it (frost FR-16.1 keeps a disabled zone protected)
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
        outputBank.claim(pin);
        applyRelay(pin, false, RS_CONFIG);
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

void ThermalZoneManager::setSolarGain(size_t index, bool enabled) {
    if (index < MAX_THERMAL_ZONES && _configs[index].solarGain != enabled) {
        _configs[index].solarGain = enabled;
        persistConfig();
    }
}

void ThermalZoneManager::setCircuits(size_t index, uint8_t circuits) {
    if (index < MAX_THERMAL_ZONES && circuits >= 1 && circuits <= 16 && _configs[index].circuits != circuits) {
        _configs[index].circuits = circuits;
        persistConfig();
    }
}

void ThermalZoneManager::setHumidityComp(size_t index, bool enabled) {
    if (index < MAX_THERMAL_ZONES && _configs[index].humidityComp != enabled) {
        _configs[index].humidityComp = enabled;
        persistConfig();
    }
}

void ThermalZoneManager::setHeatingType(size_t index, uint8_t type) {
    if (index < MAX_THERMAL_ZONES && type <= 1 && _configs[index].heatingType != type) {
        _configs[index].heatingType = type;
        persistConfig();
    }
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
            outputBank.claim(relayPin);
            applyRelay(relayPin, false, RS_CONFIG);
            applyRelay(oldPin, false, RS_CONFIG);
        }
        if (targetTemp >= 15.0f && targetTemp <= TARGET_MAX_C) _configs[index].targetTemp = targetTemp;
        _configs[index].enabled = enabled;
        persistConfig();
        netConsole.printf("[Thermal] Zone %u mapped: '%s' | MAC: %s | Pin: %u | Target: %.1f C\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac, _configs[index].relayPin, _configs[index].targetTemp);
    }
}


float ThermalZoneManager::getLearnedDutyPct(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0.0f;
    return (float)lroundf(_tpi[index].integral * 1000) / 10.0f;
}

void ThermalZoneManager::setLearnedDutyPct(size_t index, float pct) {
    if (index >= MAX_THERMAL_ZONES || !(pct >= 0.0f && pct <= PARAM_LEARNED_MAX / 10.0f)) return;
    int16_t perMille = (int16_t)lroundf(pct * 10);
    if (lroundf(_tpi[index].integral * 1000) == perMille) return;
    _tpi[index].integral = perMille / 1000.0f;
    paramStore.saveLearned(index, perMille);
    _tpi[index].savedIntegral = _tpi[index].integral;
}

// ---- Zones added and deleted by the user (FR-73, FR-74) ----------------------------------------------------

int ThermalZoneManager::firstFreeRelayPin() const {
    static const uint8_t ORDER[] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3, PIN_RELAY_SPARE};
    for (uint8_t pin : ORDER) {
        if (zoneUsingRelay(pin, SIZE_MAX) >= 0) continue;
        if (_pumpControlEnabled && pin == getPumpRelayPin()) continue;
        return pin;
    }
    return -1;
}

ThermalZoneManager::ZoneResult ThermalZoneManager::addZone(const char* name, size_t& index) {
    int pin = firstFreeRelayPin();
    if (pin < 0 || _zoneCount >= MAX_THERMAL_ZONES) return ZoneResult::NoFreeOutput;
    index = _zoneCount;
    clearZoneSlot(index);
    if (name && name[0] != '\0') strlcpy(_configs[index].name, name, sizeof(_configs[index].name));
    else snprintf(_configs[index].name, sizeof(_configs[index].name), "Zone %u", (unsigned)(index + 1));
    _configs[index].relayPin = (uint8_t)pin;
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) {  // factory levels (FR-29)
        const ScheduleConfig f = getFactoryDefaultSchedule();
        _scheduleConfig.levelTemps[l][index] = f.levelTemps[l][index];
    }
    _zoneCount++;
    outputBank.claim((uint8_t)pin);
    applyRelay((uint8_t)pin, false, RS_CONFIG);
    paramStore.saveLearned(index, 0);
    persistConfig();
    saveSchedule();
    netConsole.printf("[Thermal] Zone %u added on relay pin %d\n", (unsigned)(index + 1), pin);
    return ZoneResult::Ok;
}

ThermalZoneManager::ZoneResult ThermalZoneManager::deleteZone(size_t index) {
    if (index >= _zoneCount) return ZoneResult::NoSuchZone;
    if (_statuses[index].relayEngaged) return ZoneResult::ValveEnergised;  // FR-3, as FR-11.3
    applyRelay(_configs[index].relayPin, false, RS_CONFIG);  // closed per FR-15.1, while it still is a zone output
    const ScheduleConfig f = getFactoryDefaultSchedule();
    for (size_t j = index; j + 1 < _zoneCount; j++) {  // everything of a zone moves with it
        _configs[j] = _configs[j + 1];
        _configs[j].id = j + 1;
        _statuses[j] = _statuses[j + 1];
        _tpi[j] = _tpi[j + 1];
        _windowStates[j] = _windowStates[j + 1];
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) _scheduleConfig.levelTemps[l][j] = _scheduleConfig.levelTemps[l][j + 1];
        paramStore.saveLearned(j, paramStore.learned(j + 1));
    }
    _safety.onZoneRemoved(index);
    size_t last = _zoneCount - 1;
    clearZoneSlot(last);
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) _scheduleConfig.levelTemps[l][last] = f.levelTemps[l][last];
    paramStore.saveLearned(last, 0);
    _zoneCount--;
    diagEngine.removeZone((uint8_t)(index + 1));  // clears bit and shifts remaining zones (FR-74, PB-116)
    persistConfig();
    saveSchedule();
    netConsole.printf("[Thermal] Zone %u deleted\n", (unsigned)(index + 1));
    return ZoneResult::Ok;
}

void ThermalZoneManager::setOpenLoopBypass(bool en) {
    _openLoopBypass = en;
    paramStore.set(Param::OpenLoopBypass, en ? 1 : 0);
}

void ThermalZoneManager::setAutoMode(bool autoMode) {
    if (!autoMode && _autoMode) _manualStartMs = millis();  // FR-52: the session starts here (uptime based)
    if (autoMode != _autoMode) eventLog.add(EV_MODE, 0, RS_USER, 0, _autoMode ? 1 : 0, autoMode ? 1 : 0);
    _autoMode = autoMode;
    if (!autoMode) _boilerDemand = false;  // MANUAL: a running boiler is the user's, only frost (FR-16.1) starts or stops it
    persistConfig();
    netConsole.printf("[Thermal] Operation mode switched to: %s\n", _autoMode ? "AUTO" : "MANUAL");
}

uint32_t ThermalZoneManager::manualRemainingSec() const {
    if (_autoMode) return 0;
    uint32_t total = (uint32_t)paramStore.get(Param::ManualTimeout);
    uint32_t used = (millis() - _manualStartMs) / 1000UL;
    return used >= total ? 0 : total - used;
}

void ThermalZoneManager::seasonSet(bool heating, const char* why) {
    if (heating == _seasonHeating) return;
    _seasonHeating = heating;
    _daysOn = _daysOff = 0;
    paramStore.saveState(StateKey::SeasonHeating, heating ? 1 : 0);
    paramStore.saveState(StateKey::SeasonDaysOn, 0);
    paramStore.saveState(StateKey::SeasonDaysOff, 0);
    netConsole.printf("[Season] %s (%s)\n", heating ? "HEATING" : "SUMMER (global OFF)", why);
}

void ThermalZoneManager::setSeasonMode(uint8_t mode) {
    paramStore.set(Param::SeasonMode, mode);
    if (mode == 1) seasonSet(true, "season mode ON");
    else if (mode == 2) seasonSet(false, "season mode OFF");
    else { _daysOn = _daysOff = 0; }
    _coldSinceMs = 0;
}

// FR-12.2: ON after seasonOnDays days in a row below heatLimit, OFF after seasonOffDays days in a row above
// heatLimit + 2 C; season mode ON / OFF ignore the weather
void ThermalZoneManager::seasonNewDay(float mean) {
    if (paramStore.get(Param::SeasonMode) != 0) return;
    float limit = paramStore.getf(Param::HeatLimit);
    if (!_seasonHeating) {
        _daysOn = (mean < limit) ? _daysOn + 1 : 0;
        if (_daysOn >= paramStore.get(Param::SeasonOnDays)) seasonSet(true, "cold days");
    } else {
        _daysOff = (mean > limit + 2.0f) ? _daysOff + 1 : 0;
        if (_daysOff >= paramStore.get(Param::SeasonOffDays)) seasonSet(false, "warm days");
    }
    paramStore.saveState(StateKey::SeasonDaysOn, _daysOn);
    paramStore.saveState(StateKey::SeasonDaysOff, _daysOff);
}

// Once per tick: MANUAL timeout (FR-52), a new previous-day mean, the season safety net (FR-12.2)
void ThermalZoneManager::modeTick(uint32_t now) {
    if (!_autoMode && manualRemainingSec() == 0) {
        netConsole.println(F("[Thermal] MANUAL timeout (manualTimeout), back to AUTO."));
        setAutoMode(true);
    }
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {  // FR-39: a boost also ends after boostDuration, with or without time
        if (_statuses[i].isManualHold && now - _statuses[i].holdStartMs >= (uint32_t)paramStore.get(Param::BoostDuration) * 1000UL) {
            _statuses[i].isManualHold = false;
            netConsole.printf("[Thermal] Zone %u (%s) boost ended after boostDuration.\n",
                (unsigned int)_configs[i].id, _configs[i].name);
        }
    }
    float mean;
    if (timeAndWeather.takeNewDayMean(mean)) seasonNewDay(mean);
    if (!_seasonHeating && paramStore.get(Param::SeasonMode) == 0) {
        bool cold = false;
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++)
            if (_configs[i].enabled && _statuses[i].sensorValid && _statuses[i].currentTemp < fminf(_scheduleConfig.levelTemps[LEVEL_NIGHT][i], TARGET_MAX_C)) cold = true;
        if (!cold) _coldSinceMs = 0;
        else if (_coldSinceMs == 0) _coldSinceMs = now ? now : 1;
        else if (now - _coldSinceMs >= 12UL * 3600UL * 1000UL) seasonSet(true, "zone below night level for 12 h");
    } else {
        _coldSinceMs = 0;
    }
}

void ThermalZoneManager::setRelayManual(size_t index, bool on) {
    if (index >= MAX_THERMAL_ZONES || _autoMode) return;
    uint32_t now = millis();
    ThermalZoneStatus& st = _statuses[index];
    if (on) {
        if (!st.relayEngaged) {
            applyRelay(_configs[index].relayPin, true, RS_MANUAL);
            st.relayEngaged = true;
            _safety.syncValveState(index, true, now);
            st.stateChangedMs = now;
            eventLog.add(EV_INTERLOCK, 0, RS_MANUAL, _configs[index].id, _safety.getInterlockDelayMs() / 1000, 1);
        }
        st.demand = true;
        if (st.state != ZONE_FROST_GUARD && st.state != ZONE_HEATING) st.state = ZONE_WARMING_UP;
    } else {
        bool otherOpen = false;
        for (size_t j = 0; j < MAX_THERMAL_ZONES; j++) {
            if (j != index && _statuses[j].demand && valveOpen(j, now)) otherOpen = true;
        }
        if (!otherOpen && st.demand && valveOpen(index, now)) _safety.setOverrunKeeperZone((int8_t)index);  // FR-3: the last valve
        st.demand = false;
        arbitrateNow();  // closes the valve unless the boiler burns or the overrun runs (FR-3)
        if (st.relayEngaged) netConsole.printf("[Thermal][MANUAL] Zone %u valve held open: boiler ON or overrun (FR-3)\n", _configs[index].id);
    }
    netConsole.printf("[Thermal][MANUAL] Relay Zone %u switched to %s\n", _configs[index].id, on ? "ON" : "OFF");
}

void ThermalZoneManager::setPumpManual(bool on) {
    // Relay 4 is the pump only with pump control on; otherwise it may be a zone output (FR-11.3)
    if (!_autoMode && _pumpControlEnabled) {
        if (on && !anyValveOpen(millis())) {  // FR-14.2, FR-14.5: not before a valve has been open for interlockDelay
            netConsole.println(F("[Thermal][MANUAL] Pump ON refused: no valve open for interlockDelay (FR-14.5)"));
            return;
        }
        _pumpActive = on;
        applyRelay(PIN_RELAY_SPARE, on, RS_MANUAL);
        netConsole.printf("[Thermal][MANUAL] Circulation Pump Relay 4 switched to %s\n", on ? "ON" : "OFF");
    }
}

void ThermalZoneManager::setBoilerManual(BoilerRadioTrigger& boiler, bool on) {
    if (!_autoMode) {
        if (on) {
            if (boiler.sendBoilerOn(false, RS_MANUAL)) {
                _lastBoilerOnTime = millis();
            }
        } else {
            if (boiler.sendBoilerOff(false, RS_MANUAL)) {
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
    cfg.scheduleMode = SCHED_MODE_5_2;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        cfg.levelTemps[LEVEL_DAY][i] = 23.0f;    // FR-29 factory levels
        cfg.levelTemps[LEVEL_NIGHT][i] = 21.0f;
        cfg.levelTemps[LEVEL_AWAY][i] = 18.0f;
        cfg.awayTemps[i] = encodeTempByte(18.0f);
    }
    strlcpy(cfg.momentNames[0], "Trezire", sizeof(cfg.momentNames[0]));
    strlcpy(cfg.momentNames[1], "Plecat", sizeof(cfg.momentNames[1]));
    strlcpy(cfg.momentNames[2], "Sear\xC4\x83", sizeof(cfg.momentNames[2]));
    strlcpy(cfg.momentNames[3], "Noapte", sizeof(cfg.momentNames[3]));
    cfg.weekdayCount = 2;
    cfg.weekday[0] = {6, 30, LEVEL_DAY};
    cfg.weekday[1] = {22, 30, LEVEL_NIGHT};
    cfg.weekendCount = 2;
    cfg.weekend[0] = {8, 0, LEVEL_DAY};
    cfg.weekend[1] = {23, 0, LEVEL_NIGHT};

    // Honeywell 4-moment daily matrix
    // Weekday: P1 06:30 (23.0), P2 08:30 (20.0), P3 17:00 (23.0), P4 22:30 (21.0)
    for (uint8_t d = 1; d <= 5; d++) {
        cfg.days[d][0].hour = 6;  cfg.days[d][0].minute = 30;
        cfg.days[d][1].hour = 8;  cfg.days[d][1].minute = 30;
        cfg.days[d][2].hour = 17; cfg.days[d][2].minute = 0;
        cfg.days[d][3].hour = 22; cfg.days[d][3].minute = 30;
        for (size_t z = 0; z < MAX_THERMAL_ZONES; z++) {
            cfg.days[d][0].temps[z] = encodeTempByte(23.0f);
            cfg.days[d][1].temps[z] = encodeTempByte(20.0f);
            cfg.days[d][2].temps[z] = encodeTempByte(23.0f);
            cfg.days[d][3].temps[z] = encodeTempByte(21.0f);
        }
    }
    // Weekend: P1 08:00 (23.0), P2 12:00 (23.0), P3 18:00 (23.0), P4 23:00 (21.0)
    for (uint8_t d : {0, 6}) {
        cfg.days[d][0].hour = 8;  cfg.days[d][0].minute = 0;
        cfg.days[d][1].hour = 12; cfg.days[d][1].minute = 0;
        cfg.days[d][2].hour = 18; cfg.days[d][2].minute = 0;
        cfg.days[d][3].hour = 23; cfg.days[d][3].minute = 0;
        for (size_t z = 0; z < MAX_THERMAL_ZONES; z++) {
            cfg.days[d][0].temps[z] = encodeTempByte(23.0f);
            cfg.days[d][1].temps[z] = encodeTempByte(23.0f);
            cfg.days[d][2].temps[z] = encodeTempByte(23.0f);
            cfg.days[d][3].temps[z] = encodeTempByte(21.0f);
        }
    }
    return cfg;
}

void ThermalZoneManager::setScheduleMode(uint8_t mode) {
    if (mode > SCHED_MODE_7DAY) mode = SCHED_MODE_5_2;
    _scheduleConfig.scheduleMode = mode;
    saveSchedule();
    evaluateSchedule(millis());
}

const char* ThermalZoneManager::getScheduleModeName() const {
    switch (_scheduleConfig.scheduleMode) {
        case SCHED_MODE_CONSTANT: return "constant";
        case SCHED_MODE_UNIFIED:  return "unified";
        case SCHED_MODE_7DAY:     return "7day";
        default:                  return "5+2";
    }
}

bool ThermalZoneManager::setScheduleModeByName(const char* name) {
    if (!name) return false;
    if (strcasecmp(name, "constant") == 0) _scheduleConfig.scheduleMode = SCHED_MODE_CONSTANT;
    else if (strcasecmp(name, "unified") == 0) _scheduleConfig.scheduleMode = SCHED_MODE_UNIFIED;
    else if (strcasecmp(name, "7day") == 0 || strcasecmp(name, "7-day") == 0) _scheduleConfig.scheduleMode = SCHED_MODE_7DAY;
    else if (strcasecmp(name, "5+2") == 0 || strcasecmp(name, "5_2") == 0 || strcasecmp(name, "5 2") == 0 || strcasecmp(name, "5-2") == 0) _scheduleConfig.scheduleMode = SCHED_MODE_5_2;
    else return false;
    saveSchedule();
    evaluateSchedule(millis());
    return true;
}

bool ThermalZoneManager::setMoment(uint8_t dayIndex, uint8_t momentIndex, uint8_t hour, uint8_t minute, const float* temps) {
    if (dayIndex >= MAX_SCHEDULE_DAYS || momentIndex >= MAX_SCHEDULE_MOMENTS || hour > 23 || minute > 59) return false;
    _scheduleConfig.days[dayIndex][momentIndex].hour = hour;
    _scheduleConfig.days[dayIndex][momentIndex].minute = minute;
    if (temps) {
        for (size_t z = 0; z < _zoneCount && z < MAX_THERMAL_ZONES; z++) {
            if (temps[z] >= 5.0f && temps[z] <= TARGET_MAX_C) {
                _scheduleConfig.days[dayIndex][momentIndex].temps[z] = encodeTempByte(temps[z]);
            }
        }
    }
    saveSchedule();
    evaluateSchedule(millis());
    return true;
}

bool ThermalZoneManager::setMomentForGroup(const char* group, uint8_t momentIndex, uint8_t hour, uint8_t minute, const float* temps) {
    if (!group || momentIndex >= MAX_SCHEDULE_MOMENTS || hour > 23 || minute > 59) return false;
    if (strcasecmp(group, "weekday") == 0) {
        for (uint8_t d = 1; d <= 5; d++) {
            setMoment(d, momentIndex, hour, minute, temps);
        }
    } else if (strcasecmp(group, "weekend") == 0) {
        setMoment(0, momentIndex, hour, minute, temps);
        setMoment(6, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "all") == 0) {
        for (uint8_t d = 0; d < MAX_SCHEDULE_DAYS; d++) {
            setMoment(d, momentIndex, hour, minute, temps);
        }
    } else if (strcasecmp(group, "sun") == 0 || strcmp(group, "0") == 0) {
        return setMoment(0, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "mon") == 0 || strcmp(group, "1") == 0) {
        return setMoment(1, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "tue") == 0 || strcmp(group, "2") == 0) {
        return setMoment(2, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "wed") == 0 || strcmp(group, "3") == 0) {
        return setMoment(3, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "thu") == 0 || strcmp(group, "4") == 0) {
        return setMoment(4, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "fri") == 0 || strcmp(group, "5") == 0) {
        return setMoment(5, momentIndex, hour, minute, temps);
    } else if (strcasecmp(group, "sat") == 0 || strcmp(group, "6") == 0) {
        return setMoment(6, momentIndex, hour, minute, temps);
    } else {
        int d = atoi(group);
        if (d >= 0 && d < MAX_SCHEDULE_DAYS) {
            return setMoment((uint8_t)d, momentIndex, hour, minute, temps);
        }
        return false;
    }
    return true;
}

bool ThermalZoneManager::setAwayTemp(size_t index, float temp) {
    if (index >= _zoneCount || temp < 5.0f || temp > TARGET_MAX_C) return false;
    _scheduleConfig.awayTemps[index] = encodeTempByte(temp);
    _scheduleConfig.levelTemps[LEVEL_AWAY][index] = temp;
    saveSchedule();
    evaluateSchedule(millis());
    return true;
}

float ThermalZoneManager::getAwayTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 18.0f;
    if (_scheduleConfig.awayTemps[index] > 0) return decodeTempByte(_scheduleConfig.awayTemps[index]);
    return _scheduleConfig.levelTemps[LEVEL_AWAY][index];
}

const char* ThermalZoneManager::getMomentName(uint8_t index) const {
    if (index >= MAX_SCHEDULE_MOMENTS) return "";
    if (_scheduleConfig.momentNames[index][0] == '\0') {
        switch (index) {
            case 0: return "Trezire";
            case 1: return "Plecat";
            case 2: return "Sear\xC4\x83";
            case 3: return "Noapte";
        }
    }
    return _scheduleConfig.momentNames[index];
}

bool ThermalZoneManager::setMomentName(uint8_t index, const char* name) {
    if (index >= MAX_SCHEDULE_MOMENTS || !name || strlen(name) == 0) return false;
    strlcpy(_scheduleConfig.momentNames[index], name, sizeof(_scheduleConfig.momentNames[index]));
    saveSchedule();
    return true;
}

bool ThermalZoneManager::setLevelTemp(size_t index, ComfortLevel level, float temp) {
    if (index >= _zoneCount || level >= LEVEL_COUNT || temp < 5.0f || temp > TARGET_MAX_C) return false;
    _scheduleConfig.levelTemps[level][index] = temp;
    if (level == LEVEL_AWAY) {
        _scheduleConfig.awayTemps[index] = encodeTempByte(temp);
    }
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

    // Honeywell moment tracking
    int dayOfWeek = timeAndWeather.getDayOfWeekIndex(); // 0=Sun..6=Sat
    int evalDay = dayOfWeek;
    if (_scheduleConfig.scheduleMode == SCHED_MODE_UNIFIED) evalDay = 1;
    else if (_scheduleConfig.scheduleMode == SCHED_MODE_5_2) evalDay = (dayOfWeek == 0 || dayOfWeek == 6) ? 0 : 1;

    size_t actM = MAX_SCHEDULE_MOMENTS - 1;
    for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
        uint32_t mMins = _scheduleConfig.days[evalDay][m].hour * 60 + _scheduleConfig.days[evalDay][m].minute;
        if (curMinutes >= mMins) actM = m;
    }
    _activeMomentIdx = (uint8_t)actM;
    _nextMomentIdx = (uint8_t)((actM + 1) % MAX_SCHEDULE_MOMENTS);

    // Pre-Heat: start early (weather-dependent lead) if the next level is warmer in any zone
    _preHeatActive = false;
    if (_scheduleConfig.preHeatEnabled && _minsToNextSlot <= getPreHeatLeadMinutes()) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (fminf(_scheduleConfig.levelTemps[_nextLevel][i], TARGET_MAX_C) > fminf(_scheduleConfig.levelTemps[_activeLevel][i], TARGET_MAX_C) + 0.2f) {
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
                float t = getEffectiveTargetTemp(i);
                if (fabsf(_configs[i].targetTemp - t) > 0.05f) {  // FR-61: the last schedule level survives a restart without time
                    _configs[i].targetTemp = t;
                    _nvsSavePending = true;
                    _nvsSavePendingMs = millis() + 3000;
                }
            }
        }
    }
}

uint32_t ThermalZoneManager::getPreHeatLeadMinutes() const {
    float outT = timeAndWeather.getOutdoorTemperature();
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
    return heatUpModel.getPreHeatLeadMinutes(outT, wValid);
}

bool ThermalZoneManager::evaluateTpi(size_t index, float target, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return false;
    float temp = _statuses[index].currentTemp;
    float outT = timeAndWeather.getOutdoorTemperature();
    bool weatherValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
    bool demand = zoneController.evaluateTpi(index, temp, target, now, outT, weatherValid, _hysteresis);
    float duty = zoneController.getDuty(index);
    // FR-70: SolarThrottle factor
    float solarFactor = solarThrottle.getThrottleFactor(index);
    duty *= solarFactor;
    _tpi[index].duty = duty;
    _tpi[index].onMs = (uint32_t)(duty * 1800000.0f); // TPI cycle is 30 min (1800 s)
    _tpi[index].target = target;
    return demand;
}

float ThermalZoneManager::getHumidityCompReduction(size_t index) const {
    // FR-71: Relative humidity compensation lowers target by up to 0.5 C at RH >= 60%
    if (index >= MAX_THERMAL_ZONES || !_configs[index].humidityComp) return 0.0f;
    const BleSensorData* sens = bleScanner.findSensorByMac(_configs[index].sensorMac);
    if (!sens || !_statuses[index].sensorValid || _statuses[index].sensorFault) return 0.0f;
    float rh = sens->humidity;
    if (rh < 60.0f) return 0.0f;
    float red = (rh - 60.0f) * 0.025f;
    return constrain(red, 0.0f, 0.5f);
}

float ThermalZoneManager::getEffectiveTargetTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return DEFAULT_SETPOINT_TEMP;
    float baseTarget = _statuses[index].isManualHold ? fminf(_configs[index].targetTemp, TARGET_MAX_C) : getScheduleTargetTemp(index);
    float reduction = getHumidityCompReduction(index);
    return fmaxf(baseTarget - reduction, 5.0f);
}

// FR-1.2: the level the schedule (or away) asks now, ignoring a hold
float ThermalZoneManager::getScheduleTargetTemp(size_t index) const {
    return fminf(scheduleTargetRaw(index), TARGET_MAX_C);  // a stored level above the ceiling is used as the ceiling (C-36)
}

float ThermalZoneManager::scheduleTargetRaw(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return DEFAULT_SETPOINT_TEMP;
    if (_scheduleConfig.away) {
        if (_scheduleConfig.awayTemps[index] > 0) return decodeTempByte(_scheduleConfig.awayTemps[index]);
        return _scheduleConfig.levelTemps[LEVEL_AWAY][index];
    }
    if (!_scheduleConfig.enabled || !timeAndWeather.isTimeSynced()) return _configs[index].targetTemp;

    int dayOfWeek = timeAndWeather.getDayOfWeekIndex();
    int evalDay = dayOfWeek;
    if (_scheduleConfig.scheduleMode == SCHED_MODE_UNIFIED) evalDay = 1;
    else if (_scheduleConfig.scheduleMode == SCHED_MODE_5_2) evalDay = (dayOfWeek == 0 || dayOfWeek == 6) ? 0 : 1;

    if (_scheduleConfig.scheduleMode == SCHED_MODE_CONSTANT) {
        if (_scheduleConfig.days[evalDay][0].temps[index] > 0) {
            return decodeTempByte(_scheduleConfig.days[evalDay][0].temps[index]);
        }
        return _scheduleConfig.levelTemps[LEVEL_DAY][index];
    }

    size_t targetM = _preHeatActive ? _nextMomentIdx : _activeMomentIdx;
    if (_scheduleConfig.days[evalDay][targetM].temps[index] > 0) {
        return decodeTempByte(_scheduleConfig.days[evalDay][targetM].temps[index]);
    }
    return _scheduleConfig.levelTemps[_preHeatActive ? _nextLevel : _activeLevel][index];
}

void ThermalZoneManager::evaluateWindowDetection(size_t index, float currentTemp, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return;
    if (!_scheduleConfig.windowDetectionEnabled) {
        _windowStates[index].pauseActive = false;
        _windowStates[index].bucketStartMs = 0;
        for (uint8_t k = 0; k < 15; k++) _windowStates[index].bucketMax[k] = -999.0f;
        if (_statuses[index].state == ZONE_WINDOW_PAUSE) {
            _statuses[index].state = ZONE_IDLE;
            _statuses[index].stateChangedMs = now;
        }
        return;
    }

    float dropThreshold = _windowThreshold;

    if (!_windowStates[index].pauseActive) {
        WindowDetectionState &w = _windowStates[index];
        // Baseline = highest reading of the last 15 min (adaptive) in fifteen 60 s buckets.
        if (w.bucketStartMs == 0 || now - w.bucketStartMs >= 900000UL) {
            for (uint8_t k = 0; k < 15; k++) w.bucketMax[k] = -999.0f;
            w.bucketHead = 0;
            w.bucketStartMs = now;
            w.dropInsightLogged = false;
        } else {
            while (now - w.bucketStartMs >= 60000UL) {
                w.bucketHead = (w.bucketHead + 1) % 15;
                w.bucketMax[w.bucketHead] = -999.0f;
                w.bucketStartMs += 60000UL;
            }
        }
        if (currentTemp > w.bucketMax[w.bucketHead]) w.bucketMax[w.bucketHead] = currentTemp;

        // 1. Fast 5-minute peak (last 5 buckets)
        float peak5m = -999.0f;
        uint8_t peakAge5m = 0;
        for (uint8_t k = 0; k < 5; k++) {
            uint8_t idx = (w.bucketHead + 15 - k) % 15;
            if (w.bucketMax[idx] > peak5m) {
                peak5m = w.bucketMax[idx];
                peakAge5m = k;
            }
        }

        // 2. Adaptive 15-minute peak (all 15 buckets)
        float peak15m = -999.0f;
        uint8_t peakAge15m = 0;
        for (uint8_t k = 0; k < 15; k++) {
            uint8_t idx = (w.bucketHead + 15 - k) % 15;
            if (w.bucketMax[idx] > peak15m) {
                peak15m = w.bucketMax[idx];
                peakAge15m = k;
            }
        }

        if (peak15m > w.baselineTemp || w.baselineMs == 0) w.dropInsightLogged = false;
        w.baselineTemp = peak15m;
        w.baselineMs = now - (uint32_t)peakAge15m * 60000UL;

        float drop5m = peak5m - currentTemp;
        float drop15m = peak15m - currentTemp;

        float outdoor = timeAndWeather.getOutdoorTemperature();
        bool outdoorCold = (outdoor > -50.0f) ? (currentTemp - outdoor >= 5.0f) : true;

        bool fastDrop = (drop5m >= dropThreshold);
        bool adaptiveDrop = (_heatingType == HEATING_TYPE_FLOOR && drop15m >= 0.40f && outdoorCold);

        if (fastDrop || adaptiveDrop) {
            w.pauseActive = true;
            w.pauseStartMs = now;
            w.pauseMinTemp = currentTemp;
            time_t nowEpoch = time(nullptr);
            w.lastDetectionEpoch = (nowEpoch >= 1700000000) ? (uint32_t)nowEpoch : (now / 1000);
            netConsole.printf("[Thermal][WINDOW] Open window detected on Zone %u (%s): %s (%.2f C -> %.2f C, drop %.2f C in %us)...\n",
                              _configs[index].id, _configs[index].name,
                              adaptiveDrop ? "adaptive 15m trend" : "fast 5m drop",
                              fastDrop ? peak5m : peak15m, currentTemp,
                              fastDrop ? drop5m : drop15m,
                              fastDrop ? (peakAge5m * 60) : (peakAge15m * 60));
        } else if (drop15m >= 0.30f && !w.dropInsightLogged) {
            w.dropInsightLogged = true;
            netConsole.printf("[Thermal][INSIGHT] Rapid cooling detected on Zone %u (%s): %.2f C -> %.2f C (-%.2f C in %us, possible aeration)\n",
                              _configs[index].id, _configs[index].name,
                              peak15m, currentTemp, drop15m, (now - w.baselineMs) / 1000);
        }
    } else {
        if (currentTemp < _windowStates[index].pauseMinTemp) {
            _windowStates[index].pauseMinTemp = currentTemp;
        }
        bool timeoutExpired = (now - _windowStates[index].pauseStartMs >= 1800000UL); // 30 min
        float reboundTarget = (_heatingType == HEATING_TYPE_FLOOR) ? 0.30f : 0.60f;
        bool rebounded = (currentTemp >= _windowStates[index].pauseMinTemp + reboundTarget);
        if (timeoutExpired || rebounded) {
            _windowStates[index].lastDetectionDurationSec = (now - _windowStates[index].pauseStartMs) / 1000;
            _windowStates[index].pauseActive = false;
            _windowStates[index].baselineTemp = currentTemp;
            _windowStates[index].baselineMs = now;
            _windowStates[index].bucketStartMs = 0;
            if (_statuses[index].state == ZONE_WINDOW_PAUSE) {
                _statuses[index].state = ZONE_IDLE;
                _statuses[index].stateChangedMs = now;
            }
            netConsole.printf("[Thermal][WINDOW] Zone %u (%s) window pause cleared (%s, lasted %us). Resuming normal thermostat loop.\n",
                              _configs[index].id, _configs[index].name,
                              timeoutExpired ? "30 min timeout" : "temp rebounded +0.6 \xC2\xB0""C",
                              _windowStates[index].lastDetectionDurationSec);
        }
    }
}

bool ThermalZoneManager::isWindowPauseActive(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return false;
    return _windowStates[index].pauseActive && (_heatingType == HEATING_TYPE_RADIATORS);
}

bool ThermalZoneManager::isWindowDetected(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return false;
    return _windowStates[index].pauseActive;
}

void ThermalZoneManager::setHeatingType(uint8_t type) {
    if (type <= 1) {
        _heatingType = type;
        paramStore.set(Param::HeatingType, type);
    }
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
        _windowStates[index].bucketStartMs = 0;
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

static void appendBoolField(String& out, const char* key, bool val) {
    out += '"';
    out += key;
    out += "\":";
    out += val ? "true," : "false,";
}

static void appendIdTempField(String& out, uint8_t id, float temp) {
    char buf[12];
    snprintf(buf, sizeof(buf), "\"%u\":", (unsigned)id);
    out += buf;
    out += String(temp, 1);
}

static void appendDayMomentsJson(String& out, const ScheduleConfig& sched,
                                 const ThermalZoneConfig* configs, size_t zoneCount,
                                 size_t dayIdx) {
    out += '[';
    for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
        if (m > 0) out += ',';
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"moment\":%u,\"time\":\"%02u:%02u\",\"temps\":{",
                 (unsigned)(m + 1), sched.days[dayIdx][m].hour, sched.days[dayIdx][m].minute);
        out += buf;
        for (size_t z = 0; z < zoneCount; z++) {
            if (z > 0) out += ',';
            float t = sched.days[dayIdx][m].temps[z] > 0
                ? decodeTempByte(sched.days[dayIdx][m].temps[z])
                : sched.levelTemps[m == 3 ? LEVEL_NIGHT : LEVEL_DAY][z];
            appendIdTempField(out, configs[z].id, t);
        }
        out += "}}";
    }
    out += ']';
}

String ThermalZoneManager::getScheduleJson() const {
    String json;
    json.reserve(2560);
    json = "{\"status\":\"ok\",";
    appendBoolField(json, "enabled", _scheduleConfig.enabled);
    appendBoolField(json, "away", _scheduleConfig.away);
    json += "\"scheduleMode\":\"";
    json += getScheduleModeName();
    json += "\",\"activeMoment\":";
    json += (unsigned)(_activeMomentIdx + 1);
    json += ",\"nextMoment\":";
    json += (unsigned)(_nextMomentIdx + 1);
    json += ',';
    appendBoolField(json, "preHeatEnabled", _scheduleConfig.preHeatEnabled);
    appendBoolField(json, "windowDetectionEnabled", _scheduleConfig.windowDetectionEnabled);
    json += "\"activeLevel\":\"";
    json += getLevelName(_activeLevel);
    json += "\",\"nextLevel\":\"";
    json += getLevelName(_nextLevel);
    json += "\",\"minsToNextSlot\":";
    json += _minsToNextSlot;
    json += ',';
    appendBoolField(json, "preHeatActive", _preHeatActive);
    appendBoolField(json, "isWeekend", timeAndWeather.isWeekend());
    json += "\"dayOfWeek\":";
    json += timeAndWeather.getDayOfWeekIndex();
    json += ",\"weekday\":\"";
    json += getSlotsString(false);
    json += "\",\"weekend\":\"";
    json += getSlotsString(true);
    json += "\",";
    appendBoolField(json, "openLoopBypass", _openLoopBypass);
    json += "\"interlockDelaySec\":";
    json += _safety.getInterlockDelayMs() / 1000;
    json += ",\"pumpOverrunSec\":";
    json += _safety.getPumpOverrunMs() / 1000;
    json += ',';
    appendBoolField(json, "pumpControlEnabled", _pumpControlEnabled);
    json += "\"momentNames\":[";
    for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
        if (m > 0) json += ',';
        json += '"';
        json += getMomentName((uint8_t)m);
        json += '"';
    }
    json += "],\"weekday_moments\":";
    appendDayMomentsJson(json, _scheduleConfig, _configs, _zoneCount, 1);
    json += ",\"weekend_moments\":";
    appendDayMomentsJson(json, _scheduleConfig, _configs, _zoneCount, 0);
    json += ",\"awayTemps\":{";
    for (size_t z = 0; z < _zoneCount; z++) {
        if (z > 0) json += ',';
        float t = _scheduleConfig.awayTemps[z] > 0
            ? decodeTempByte(_scheduleConfig.awayTemps[z])
            : _scheduleConfig.levelTemps[LEVEL_AWAY][z];
        appendIdTempField(json, _configs[z].id, t);
    }
    json += "},\"days\":[";
    for (size_t d = 0; d < 7; d++) {
        if (d > 0) json += ',';
        appendDayMomentsJson(json, _scheduleConfig, _configs, _zoneCount, d);
    }
    json += "],\"zones\":[";
    for (size_t i = 0; i < _zoneCount; i++) {
        if (i > 0) json += ',';
        json += "{\"id\":";
        json += _configs[i].id;
        json += ",\"name\":\"";
        json += _configs[i].name;
        json += "\",";
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            float lvVal = _scheduleConfig.levelTemps[l][i];
            if (l == LEVEL_AWAY && _scheduleConfig.awayTemps[i] > 0) lvVal = decodeTempByte(_scheduleConfig.awayTemps[i]);
            json += '"';
            json += getLevelName(l);
            json += "\":";
            json += String(lvVal, 1);
            json += ',';
        }
        json += "\"effectiveTarget\":";
        json += String(getEffectiveTargetTemp(i), 1);
        json += ',';
        appendBoolField(json, "isManualHold", _statuses[i].isManualHold);
        appendBoolField(json, "windowDetected", isWindowDetected(i));
        appendBoolField(json, "windowPauseActive", isWindowPauseActive(i));
        json += "\"windowPauseRemainingSec\":";
        json += getWindowPauseRemainingSec(i);
        json += '}';
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
        _nvsSavePending = true;
        _nvsSavePendingMs = now;
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

bool ThermalZoneManager::boilerOnPermitted() const {
    return _safety.boilerOnPermitted(millis(), _autoMode, _openLoopBypass);  // FR-2 & FR-1.3
}

// FR-3: the overrun starts with the OFF of a lit flame (a start cancelled before the ON never lit it, FR-3.2) and
// lasts `overrun`; the compare is wrap-safe (PB-7)
void ThermalZoneManager::trackBoilerState(uint32_t now) {
    SafetySupervisor::OverrunTransition tr = _safety.trackBoilerState(now, boilerRadio.isLastCommandOn());
    if (tr == SafetySupervisor::OverrunTransition::Started) {
        eventLog.add(EV_OVERRUN, 0, RS_NONE, 0, _safety.getPumpOverrunMs() / 1000, 1);
        netConsole.printf("[Thermal] Overrun started: the last valve stays open for %u s (FR-3).\n",
                          _safety.getPumpOverrunMs() / 1000);
    } else if (tr == SafetySupervisor::OverrunTransition::Finished) {
        eventLog.add(EV_OVERRUN, 0, RS_NONE, 0, 0, 2);
        netConsole.println(F("[Thermal] Overrun finished (FR-3)."));
    }
}

// FR-37, FR-3: every valve follows its zone's demand. The last valve that was open stays open while the boiler is
// ON or PENDING_OFF and during the overrun; it closes at the next tick after that. A valve counts for the boiler
// once it has been energised for interlockDelay (FR-2), frost zones included (FR-16.1).
void ThermalZoneManager::arbitrateValves(uint32_t now, bool& anyReady, bool& anyReadyFrost) {
    uint32_t demandMask = 0;
    uint32_t frostMask = 0;
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        uint32_t bit = 1UL << i;
        if (_statuses[i].demand) demandMask |= bit;
        if (_statuses[i].frostActive) frostMask |= bit;
    }

    SafetySupervisor::ValveAction actions[MAX_THERMAL_ZONES];
    _safety.arbitrateValves(now, boilerRadio.isLastCommandOn(), demandMask, frostMask,
                            MAX_THERMAL_ZONES, anyReady, anyReadyFrost, actions);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        ThermalZoneStatus& st = _statuses[i];
        const SafetySupervisor::ValveAction& act = actions[i];

        if (act.has(SafetySupervisor::VA_OPEN_CHANGED)) {
            bool open = act.has(SafetySupervisor::VA_OPEN_TARGET);
            if (open) {
                applyRelay(_configs[i].relayPin, true, st.frostActive ? RS_FROST : (_autoMode ? RS_DEMAND : RS_MANUAL));
                st.relayEngaged = true;
                st.stateChangedMs = now;
                eventLog.add(EV_INTERLOCK, 0, RS_NONE, _configs[i].id, _safety.getInterlockDelayMs() / 1000, 1);
                if (st.state != ZONE_FROST_GUARD) st.state = ZONE_WARMING_UP;
                netConsole.printf("[Thermal] Actuator Zone %u (%s) opening on pin %u. Waiting %u s...\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin, _safety.getInterlockDelayMs() / 1000);
            } else {
                bool wasHeld = act.has(SafetySupervisor::VA_OVERRUN_END);
                applyRelay(_configs[i].relayPin, false,
                           wasHeld ? RS_OVERRUN_END
                                   : (st.state == ZONE_FAULT_TIMEOUT ? RS_SENSOR_LOSS : (_autoMode ? RS_NO_DEMAND : RS_MANUAL)));
                st.relayEngaged = false;
                st.stateChangedMs = now;
                if (st.state == ZONE_HEATING || st.state == ZONE_WARMING_UP || st.state == ZONE_FROST_GUARD) st.state = ZONE_IDLE;
                netConsole.printf("[Thermal] Zone %u (%s) closed relay on pin %u.\n", _configs[i].id, _configs[i].name, _configs[i].relayPin);
            }
        } else if (st.demand && (st.state == ZONE_WARMING_UP || st.state == ZONE_IDLE) && _safety.valveOpen(i, now)) {
            st.state = ZONE_HEATING;
            st.stateChangedMs = now;
            netConsole.printf("[Thermal] Actuator Zone %u (%s) fully OPEN. Circuit ready!\n", _configs[i].id, _configs[i].name);
        }

        if (act.has(SafetySupervisor::VA_HOLD_STARTED)) {
            eventLog.add(EV_OVERRUN, 0, RS_NO_DEMAND, _configs[i].id, 0, 3);
        }
        if (act.has(SafetySupervisor::VA_INTERLOCK_DONE)) {
            eventLog.add(EV_INTERLOCK, 0, RS_NONE, _configs[i].id, act.interlockElapsedSec, 2);
        }
    }
}

void ThermalZoneManager::arbitrateNow() {
    uint32_t now = millis();
    bool ready, readyFrost;
    trackBoilerState(now);
    arbitrateValves(now, ready, readyFrost);
}

// FR-16.1: any zone, enabled or disabled, with an accepted reading below frostTemp heats in every mode until
// frostTemp + 1.5 C; the interlock (FR-2) and the burner guard (FR-4) still apply, because it is only a demand
void ThermalZoneManager::evaluateFrost(size_t index, bool found, float currentTemp) {
    ThermalZoneStatus& st = _statuses[index];
    SafetySupervisor::FrostEvalResult res = _safety.evaluateFrost(st.frostActive, found, currentTemp);
    st.frostActive = res.frostActive;
    if (st.frostActive) {
        _frostProtectionActive = true;
        st.demand = true;
        st.state = ZONE_FROST_GUARD;
        if (res.becameActive) {
            eventLog.add(EV_FROST, 0, RS_FROST, _configs[index].id, 0, 1);
            netConsole.printf("[Thermal][FROST GUARD] Heating triggered on Zone %u (%s): %.1f C < %.1f C\n",
                _configs[index].id, _configs[index].name, currentTemp, _safety.getFreezeProtectionTemp());
        }
    } else if (res.becameInactive) {
        eventLog.add(EV_FROST, 0, RS_FROST, _configs[index].id, 1, 0);
        st.demand = false;  // AUTO recomputes it right after; MANUAL returns to the user's request (closed)
        if (st.state == ZONE_FROST_GUARD) st.state = st.relayEngaged ? ZONE_HEATING : ZONE_IDLE;
    }
}

void ThermalZoneManager::update(BoilerRadioTrigger& boiler) {
    uint32_t now = millis();
    if (now - _lastEvaluationMs < 950) {
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
            _nvsSavePending = true;
            _nvsSavePendingMs = now;

            // Turn off pump
            if (_pumpControlEnabled) {
                applyRelay(PIN_RELAY_SPARE, false, RS_EXERCISE);
                _pumpActive = false;
            }
            // Restore every relay to its zone's valve state; a zone switched off during the exercise closes
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged, RS_EXERCISE);
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

    modeTick(now);

    // 1. Evaluate heat demand per zone
    _frostProtectionActive = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        float currentTemp = -999.0f;
        bool found = false;
        uint32_t timeoutMs = _sensorTimeoutSec * 1000UL;

        // Direct Native BLE Sensor Resolution
        if (faultInjector.isSensorLost(_configs[i].id)) {
            found = false;
        } else {
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
            diagEngine.clearDtc(Dtc::ERR_SENSOR_TIMEOUT, _configs[i].id);  // a disabled zone has no sensor fault
            evaluateFrost(i, found, currentTemp);  // FR-16.1: a disabled zone is protected too
            continue;                              // its valve follows demand in arbitrateValves
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
                eventLog.add(EV_SENSOR, 0, RS_NONE, _configs[i].id, 1, 0);
                diagEngine.clearDtc(Dtc::ERR_SENSOR_TIMEOUT, _configs[i].id);  // first accepted reading (FR-16.2)
                netConsole.printf("[Thermal] Zone %u (%s) sensor recovered: %.1f \xC2\xB0""C\n",
                                  _configs[i].id, _configs[i].name, currentTemp);
            }

            evaluateFrost(i, found, currentTemp);  // FR-16.1

            // Time-proportional control (only if autoMode and not forced by frost guard)
            if (_autoMode && !_statuses[i].frostActive) {
                if (_windowStates[i].pauseActive && _heatingType == HEATING_TYPE_RADIATORS) {
                    _statuses[i].demand = false;
                    _statuses[i].state = ZONE_WINDOW_PAUSE;
                } else {
                    if (_statuses[i].state == ZONE_WINDOW_PAUSE) { _statuses[i].state = ZONE_IDLE; _statuses[i].stateChangedMs = now; } // pause over: leave the stale state (FR-8.4)
                    _statuses[i].demand = _seasonHeating && evaluateTpi(i, getEffectiveTargetTemp(i), now);  // global OFF: no heat (FR-64)
                }
            }
        } else {
            evaluateFrost(i, false, 0.0f);  // no accepted reading: no frost duty (FR-16.1), FR-51 covers the loss
            if (_bootWarmupActive && !faultInjector.isSensorLost(_configs[i].id) && _statuses[i].state == ZONE_WARMUP) {
                _statuses[i].sensorValid = false;
                _statuses[i].sensorFault = false;
                _statuses[i].state = ZONE_WARMUP;
            } else {
                // Sensor missing > timeout: FR-51 Fallback Duty (fixes PB-23, PB-61)
                bool newFault = _statuses[i].state != ZONE_FAULT_TIMEOUT;
                _statuses[i].sensorValid = false;
                _statuses[i].sensorFault = true;
                _statuses[i].state = ZONE_FAULT_TIMEOUT;

                if (newFault) eventLog.add(EV_SENSOR, 0, RS_SENSOR_LOSS, _configs[i].id, 0, 1);
                if (newFault) {
                    char dtcMsg[64];
                    snprintf(dtcMsg, sizeof(dtcMsg), "Zone %u sensor timeout >%us - fallback heating active", _configs[i].id, _sensorTimeoutSec);
                    diagEngine.raiseDtc(Dtc::ERR_SENSOR_TIMEOUT, dtcMsg, SEV_ERROR, _configs[i].id);
                }

                // FR-51: In AUTO mode, zone in FAULT keeps heating with learned duty or faultDuty
                if (_autoMode && !_statuses[i].frostActive) {
                    float outT = timeAndWeather.getOutdoorTemperature();
                    bool weatherValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
                    float faultDutyPct = (float)paramStore.get(Param::FaultDuty);
                    float faultFrostOutdoor = (float)paramStore.get(Param::FaultFrostOutdoor);
                    _statuses[i].demand = zoneController.evaluateFaultDuty(i, now, outT, weatherValid,
                                                                         _seasonHeating, faultDutyPct, faultFrostOutdoor);
                    _tpi[i].duty = zoneController.getDuty(i);
                    _tpi[i].onMs = zoneController.getOnMs(i);
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
            eventLog.add(EV_WARMUP, 0, RS_NONE, 0, acquired, paired);
            netConsole.printf("[INIT]   * Sensor Discovery: All %u room sensors acquired in %us! System fully operational.\n",
                              (unsigned int)paired, (unsigned int)((now - _bootWarmupStartMs) / 1000));
        } else if (now - _bootWarmupStartMs >= 300000UL) {
            _bootWarmupActive = false;
            eventLog.add(EV_WARMUP, 0, RS_NONE, 0, acquired, paired);
            netConsole.printf("[INIT]   * Sensor Discovery: Warmup cutoff (300s): %u/%u sensors acquired. Activating system for operational zones.\n",
                              (unsigned int)acquired, (unsigned int)paired);
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                if (_configs[i].enabled && !_statuses[i].sensorValid) {
                    _statuses[i].state = ZONE_FAULT_TIMEOUT;
                    _statuses[i].sensorFault = true;
                    eventLog.add(EV_SENSOR, 0, RS_SENSOR_LOSS, _configs[i].id, 0, 1);
                    char dtcMsg[64];
                    snprintf(dtcMsg, sizeof(dtcMsg), "Zone %u sensor missing at boot warmup cutoff", _configs[i].id);
                    diagEngine.raiseDtc(Dtc::ERR_SENSOR_TIMEOUT, dtcMsg, SEV_ERROR, _configs[i].id);
                }
            }
        }
        // Acquired operational zones can begin normal heating immediately;
        // zones still waiting for BLE sensors stay safe in ZONE_WARMUP (valves closed).
    }

    // 2. Valves (FR-37, FR-3, FR-16.1): the overrun window first, then every valve follows its demand
    trackBoilerState(now);
    bool anyZoneReadyForBoiler = false;
    bool anyFrostZoneReady = false;
    arbitrateValves(now, anyZoneReadyForBoiler, anyFrostZoneReady);

    // Self-check (FR-3, FR-4): the boiler never burns with every valve closed; a violation ends the run at once
    if (boiler.isLastCommandOn() && !(!_autoMode && _openLoopBypass)) {
        bool anyEnergised = false;
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) anyEnergised = anyEnergised || _statuses[i].relayEngaged;
        if (!anyEnergised) {
            netConsole.println(F("[Thermal] SAFETY SELF-CHECK: boiler ON with every valve closed (FR-3), forced OFF."));
            boiler.sendBoilerOff(true, RS_SAFETY);
        }
    }

    // 3. Pump guard (FR-14.2, FR-14.3, FR-14.5): ON only while a valve is open, the held overrun valve included
    if (_pumpControlEnabled) {
        bool pumpWanted = anyValveOpen(now);
        if (pumpWanted != _pumpActive && (_autoMode || !pumpWanted || anyFrostZoneReady || _safety.isOverrunActive())) {
            applyRelay(PIN_RELAY_SPARE, pumpWanted, RS_PUMP_GUARD);
            _pumpActive = pumpWanted;
            netConsole.printf("[Thermal] Circulation Pump (Relay 4) %s.\n", pumpWanted ? "STARTED" : "STOPPED");
        }
    }

    // 4. Boiler (FR-2, FR-4): AUTO follows the zones; MANUAL only frost (FR-16.1) starts or stops it
    bool wantOn = _autoMode ? anyZoneReadyForBoiler : anyFrostZoneReady;
    if (wantOn) {
        _boilerDemand = true;
        if (boiler.getPendingAction() == PENDING_OFF) boiler.cancelPendingAction(RS_DEMAND);  // desired ON again (FR-32)
        if (!boiler.isLastCommandOn()) {
            uint8_t guardWhy = RS_NONE;
            if (boiler.canTransmit(true, nullptr, nullptr, &guardWhy)) {
                if (boiler.sendBoilerOn(false, _autoMode ? RS_DEMAND : RS_FROST)) {
                    _lastBoilerOnTime = now;
                    netConsole.println(F("[Thermal] BOILER START commanded via RF 433MHz!"));
                }
            } else if (boiler.getPendingAction() != PENDING_ON) {
                boiler.setPendingAction(PENDING_ON, guardWhy);
                netConsole.println(F("[Thermal] BOILER START queued (Anti-short-cycling guard active)..."));
            }
        }
    } else if (_autoMode ? (_boilerDemand || boiler.isLastCommandOn()) : _boilerDemand) {
        bool flameWasActive = boiler.isLastCommandOn();
        _boilerDemand = false;
        _lastBoilerOffTime = now;
        if (flameWasActive) {
            // the valve of the last zone stays open while the OFF waits (PENDING_OFF) and for the overrun (FR-3)
            uint8_t guardWhy = RS_NONE;
            if (boiler.canTransmit(false, nullptr, nullptr, &guardWhy)) {
                if (boiler.sendBoilerOff(false, RS_NO_DEMAND)) {
                    netConsole.println(F("[Thermal] BOILER STOP commanded via RF 433MHz!"));
                }
            } else if (boiler.getPendingAction() != PENDING_OFF) {
                boiler.setPendingAction(PENDING_OFF, guardWhy);
                netConsole.println(F("[Thermal] BOILER STOP queued (Anti-short-cycling guard active)..."));
            }
        } else if (boiler.getPendingAction() == PENDING_ON) {
            boiler.cancelPendingAction(RS_DEMAND_LOST);
            netConsole.println(F("[Thermal] Pending boiler start CANCELLED (demand dropped before ignition, FR-3.2)."));
        }
    }

    // Accumulate active heating runtime per zone
    uint32_t nowMs = millis();
    static uint32_t lastFlashSaveMs = 0;
    if (_lastZoneRuntimeMs == 0) {
        _lastZoneRuntimeMs = nowMs;
    } else {
        uint32_t deltaMs = nowMs - _lastZoneRuntimeMs;
        _lastZoneRuntimeMs = nowMs;
        for (size_t i = 0; i < _zoneCount && i < MAX_THERMAL_ZONES; i++) {
            if (_statuses[i].relayEngaged) {
                _zoneRuntimeMsAcc[i] += deltaMs;
                if (_zoneRuntimeMsAcc[i] >= 1000) {
                    uint32_t addSec = _zoneRuntimeMsAcc[i] / 1000;
                    _zoneRuntimeTodaySec[i] += addSec;
                    _zoneRuntimeMsAcc[i] %= 1000;
                }
            }
        }
        if (nowMs - lastFlashSaveMs >= 60000UL) {
            saveRuntimeToFlash();
            lastFlashSaveMs = nowMs;
        }
    }
}

void ThermalZoneManager::updateRuntimeDay(uint8_t day) {
    if (day == 0) return;
    if (_lastDay == 0) {
        _lastDay = day;
        return;
    }
    if (day != _lastDay) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            _zoneRuntimeYesterdaySec[i] = _zoneRuntimeTodaySec[i];
            _zoneRuntimeTodaySec[i] = 0;
            _zoneRuntimeMsAcc[i] = 0;
        }
        _lastDay = day;
        saveRuntimeToFlash();
        netConsole.printf("[Thermal][Runtime] Rollover to new day %u.\n", day);
    }
}


uint32_t ThermalZoneManager::getZoneRuntimeTodaySec(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    uint32_t hist = (index < 3) ? telemetryHistory.getZoneRuntimeTodayFromHistory(index) : 0;
    return (_zoneRuntimeTodaySec[index] > hist) ? _zoneRuntimeTodaySec[index] : hist;
}

uint32_t ThermalZoneManager::getZoneRuntimeYesterdaySec(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    uint32_t hist = (index < 3) ? telemetryHistory.getZoneRuntimeYesterdayFromHistory(index) : 0;
    return (_zoneRuntimeYesterdaySec[index] > hist) ? _zoneRuntimeYesterdaySec[index] : hist;
}

void ThermalZoneManager::syncRuntimeFromHistory() {
    for (size_t i = 0; i < MAX_THERMAL_ZONES && i < 3; i++) {
        uint32_t histToday = telemetryHistory.getZoneRuntimeTodayFromHistory(i);
        uint32_t histYest  = telemetryHistory.getZoneRuntimeYesterdayFromHistory(i);
        if (_zoneRuntimeTodaySec[i] < histToday) {
            _zoneRuntimeTodaySec[i] = histToday;
        }
        if (_zoneRuntimeYesterdaySec[i] < histYest) {
            _zoneRuntimeYesterdaySec[i] = histYest;
        }
    }
}

void ThermalZoneManager::saveRuntimeToFlash() {
    File f = LittleFS.open("/zone_runtime.bin", "w");
    if (f) {
        f.write((const uint8_t*)&_lastDay, sizeof(_lastDay));
        f.write((const uint8_t*)_zoneRuntimeTodaySec, sizeof(_zoneRuntimeTodaySec));
        f.write((const uint8_t*)_zoneRuntimeYesterdaySec, sizeof(_zoneRuntimeYesterdaySec));
        f.close();
    }
}

void ThermalZoneManager::loadRuntimeFromFlash() {
    if (LittleFS.exists("/zone_runtime.bin")) {
        File f = LittleFS.open("/zone_runtime.bin", "r");
        if (f) {
            uint8_t savedDay = 0;
            f.read((uint8_t*)&savedDay, sizeof(savedDay));
            f.read((uint8_t*)_zoneRuntimeTodaySec, sizeof(_zoneRuntimeTodaySec));
            f.read((uint8_t*)_zoneRuntimeYesterdaySec, sizeof(_zoneRuntimeYesterdaySec));
            f.close();
            _lastDay = savedDay;
        }
    }
    syncRuntimeFromHistory();
}
