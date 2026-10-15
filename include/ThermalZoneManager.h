#ifndef THERMAL_ZONE_MANAGER_H
#define THERMAL_ZONE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "BoilerRadioTrigger.h"

#define MAX_THERMAL_ZONES 3

enum ZoneState {
    ZONE_DISABLED = 0,
    ZONE_IDLE,          // Target reached, valve closed
    ZONE_WARMING_UP,    // Valve opening in progress (waiting for actuator interlock)
    ZONE_HEATING,       // Valve fully open, boiler and pump active
    ZONE_FAULT_TIMEOUT, // Sensor missing > timeout, zone shut down for safety
    ZONE_FROST_GUARD,   // Emergency frost protection active (<12C)
    ZONE_WARMUP,        // Boot grace period: acquiring sensor beacons (max 5 min)
    ZONE_WINDOW_PAUSE   // Smart pause: open window detected (>1.2C drop in <5 min)
};

enum BioAnchorId {
    BIO_ANCHOR_SLEEP = 0,
    BIO_ANCHOR_WAKE  = 1,
    BIO_ANCHOR_AWAY  = 2,
    BIO_ANCHOR_HOME  = 3,
    BIO_ANCHORS_COUNT = 4
};

struct BioAnchorConfig {
    uint8_t hour;
    uint8_t minute;
    float targets[MAX_THERMAL_ZONES];
};

struct BioScheduleConfig {
    bool enabled;
    bool preHeatEnabled;
    bool windowDetectionEnabled;
    BioAnchorConfig weekday[BIO_ANCHORS_COUNT];
    BioAnchorConfig weekend[BIO_ANCHORS_COUNT];
};

struct WindowDetectionState {
    float baselineTemp;
    uint32_t baselineMs;
    bool pauseActive;
    uint32_t pauseStartMs;
    float pauseMinTemp;
};

struct ThermalZoneConfig {
    uint8_t id;
    char name[32];
    char sensorMac[18];   // Direct MAC mapping (e.g. "A4:C1:38:A4:C1:DD")
    char sensorName[32];  // Friendly name or alias
    uint8_t relayPin;
    float targetTemp;
    float hysteresis;
    bool enabled;
};

struct ThermalZoneStatus {
    ZoneState state;
    float currentTemp;
    bool sensorValid;
    bool sensorFault;
    bool frostActive;
    bool demand;
    bool relayEngaged;
    uint32_t stateChangedMs;
};

class ThermalZoneManager {
public:
    ThermalZoneManager();

    void begin();
    void update(BoilerRadioTrigger& boiler);

    // Persistence
    void loadFromPreferences();
    void saveToPreferences();

    // Getters
    size_t getZoneCount() const { return MAX_THERMAL_ZONES; }
    const ThermalZoneConfig& getConfig(size_t index) const;
    const ThermalZoneStatus& getStatus(size_t index) const;
    bool isBoilerDemanded() const { return _boilerDemand; }
    bool isAutoMode() const { return _autoMode; }
    uint32_t getInterlockDelayMs() const { return _interlockDelayMs; }
    uint32_t getPumpOverrunMs() const { return _pumpOverrunMs; }
    uint32_t getMinBoilerOffMs() const { return _minBoilerOffMs; }

    // Pump Control (Relay 4 on PIN_RELAY_SPARE / GPIO26)
    bool isPumpControlEnabled() const { return _pumpControlEnabled; }
    bool isPumpActive() const { return _pumpActive; }
    void setPumpControlEnabled(bool enabled);

    // Actuator Type (NC vs NO)
    bool isActuatorNormallyClosed() const { return _actuatorNormallyClosed; }
    void setActuatorNormallyClosed(bool nc);

    // Frost Protection (<12C)
    float getFreezeProtectionTemp() const { return _freezeProtectionTemp; }
    void setFreezeProtectionTemp(float temp);
    bool isFrostProtectionActive() const { return _frostProtectionActive; }

    // Summer Anti-Seize Exercise (Pumps & Valves)
    void triggerAntiSeizeExercise();
    bool isAntiSeizeRunning() const { return _antiSeizeRunning; }
    uint32_t getLastAntiSeizeEpoch() const { return _lastAntiSeizeEpoch; }

    // Sensor Ingestion Failsafe Timeout (default 600s / 10min)
    uint32_t getSensorTimeoutSec() const { return _sensorTimeoutSec; }
    void setSensorTimeoutSec(uint32_t sec);

    // Boot Sensor Warmup (Grace period: max 300s / 5min)
    bool isBootWarmupActive() const { return _bootWarmupActive; }
    uint32_t getBootWarmupElapsedSec() const;
    uint32_t getBootWarmupRemainingSec() const;
    size_t getPairedZoneCount() const;
    size_t getAcquiredZoneCount() const;

    // Setters
    void setTargetTemperature(size_t index, float target);
    void setHysteresis(size_t index, float hyst);
    void setZoneEnabled(size_t index, bool enabled);
    void setZoneName(size_t index, const char* name);
    void setSensorName(size_t index, const char* sensorName);
    void setSensorMac(size_t index, const char* sensorMac);
    void setRelayPin(size_t index, uint8_t pin);
    void mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, float hysteresis, bool enabled);
    void setAutoMode(bool autoMode);
    void setInterlockDelayMs(uint32_t ms) { _interlockDelayMs = ms; }
    void setPumpOverrunMs(uint32_t ms) { _pumpOverrunMs = ms; }
    void setMinBoilerOffMs(uint32_t ms) { _minBoilerOffMs = ms; }

    // Manual Control
    void setRelayManual(size_t index, bool on);
    void setPumpManual(bool on);
    void setBoilerManual(BoilerRadioTrigger& boiler, bool on);

    // Smart Bio-Schedule & Natural Anchors
    const BioScheduleConfig& getScheduleConfig() const { return _scheduleConfig; }
    void setScheduleConfig(const BioScheduleConfig& cfg);
    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const { return _scheduleConfig.enabled; }
    void setPreHeatEnabled(bool enabled);
    bool isPreHeatEnabled() const { return _scheduleConfig.preHeatEnabled; }
    void setWindowDetectionEnabled(bool enabled);
    bool isWindowDetectionEnabled() const { return _scheduleConfig.windowDetectionEnabled; }

    // Bio-Schedule Customization
    bool updateAnchor(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]);
    bool setAnchorTime(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute);
    bool setAnchorTargets(bool weekend, BioAnchorId anchor, const float targets[MAX_THERMAL_ZONES]);
    bool setAnchorTarget(bool weekend, BioAnchorId anchor, size_t zoneIndex, float target);

    // Presets Engine
    String getPresetsJson() const;
    bool applyPreset(const char* presetId);
    bool savePreset(const char* presetId, const char* name);
    bool resetPreset(const char* presetId);
    void resetScheduleToDefault();
    const char* getActivePresetId() const { return _activePresetId; }

    static BioScheduleConfig getFactoryDefaultSchedule();
    static BioScheduleConfig getFactoryEcoSchedule();
    static BioScheduleConfig getFactoryWfhSchedule();
    static BioAnchorId parseAnchorId(const char* name);

    BioAnchorId getActiveAnchor() const { return _activeAnchor; }
    BioAnchorId getNextAnchor() const { return _nextAnchor; }
    uint32_t getMinutesToNextAnchor() const { return _minsToNextAnchor; }
    const char* getAnchorName(BioAnchorId id) const;
    const char* getActiveAnchorName() const { return getAnchorName(_activeAnchor); }
    bool isPreHeatActive() const { return _preHeatActive; }

    // Open Window Detection State
    bool isWindowPauseActive(size_t index) const;
    uint32_t getWindowPauseRemainingSec(size_t index) const;

    // Effective Target (considers BioSchedule & Pre-Heat)
    float getEffectiveTargetTemp(size_t index) const;

    // Schedule JSON Serialization
    String getScheduleJson() const;

    // Schedule Persistence
    void loadSchedulePreferences();
    void saveSchedulePreferences();

    // Helpers
    const char* getStateStr(ZoneState state) const;

private:
    ThermalZoneConfig _configs[MAX_THERMAL_ZONES];
    ThermalZoneStatus _statuses[MAX_THERMAL_ZONES];

    // Bio-Schedule & Climate State
    BioScheduleConfig _scheduleConfig;
    char _activePresetId[16];
    BioAnchorId _activeAnchor;
    BioAnchorId _nextAnchor;
    uint32_t _minsToNextAnchor;
    bool _preHeatActive;
    WindowDetectionState _windowStates[MAX_THERMAL_ZONES];
    uint32_t _lastScheduleEvalMs;

    bool _autoMode;
    bool _boilerDemand;
    bool _pumpControlEnabled;
    bool _pumpActive;
    uint32_t _pumpStopScheduledMs;
    int8_t _overrunKeeperZone;
    bool _actuatorNormallyClosed;
    float _freezeProtectionTemp;
    bool _frostProtectionActive;
    bool _antiSeizeRunning;
    uint32_t _antiSeizeStartMs;
    uint32_t _lastAntiSeizeEpoch;
    uint32_t _sensorTimeoutSec;
    bool _bootWarmupActive;
    uint32_t _bootWarmupStartMs;

    uint32_t _interlockDelayMs;
    uint32_t _pumpOverrunMs;
    uint32_t _minBoilerOffMs;
    uint32_t _lastBoilerOffTime;
    uint32_t _lastBoilerOnTime;
    uint32_t _lastEvaluationMs;

    void applyRelay(uint8_t pin, bool active);
    void updatePumpState(bool anyZoneReadyForBoiler, uint32_t now);
    void checkAntiSeizeSchedule(uint32_t now);
    void evaluateSchedule(uint32_t now);
    void evaluateWindowDetection(size_t index, float currentTemp, uint32_t now);
};

extern ThermalZoneManager thermalZones;

#endif // THERMAL_ZONE_MANAGER_H
