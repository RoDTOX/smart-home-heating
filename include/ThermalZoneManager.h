#ifndef THERMAL_ZONE_MANAGER_H
#define THERMAL_ZONE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "BoilerRadioTrigger.h"
#include "ThermodynamicEngine.h"

#define MAX_THERMAL_ZONES 3

// Multi-Zone FSM States [SWE-REQ-001]
enum ZoneState {
    ZONE_DISABLED = 0,
    ZONE_IDLE,          // Target reached, valve closed
    ZONE_WARMING_UP,    // Valve opening in progress (waiting for actuator interlock)
    ZONE_HEATING,       // Valve fully open, boiler and pump active
    ZONE_FAULT_TIMEOUT, // Sensor missing > timeout, zone shut down for safety
    ZONE_FROST_GUARD,   // Emergency frost protection active (<12C)
    ZONE_WARMUP,        // Boot grace period: acquiring sensor beacons (max 5 min)
    ZONE_WINDOW_PAUSE   // Smart pause: open window detected (>1.2C drop in <5 min or derivative trigger)
};

#define MAX_SCHEDULE_BLOCKS 8

struct ScheduleBlockConfig {
    char name[32];
    uint8_t hour;
    uint8_t minute;
    float targets[MAX_THERMAL_ZONES];
};

typedef ScheduleBlockConfig BioAnchorConfig;

enum BioAnchorId {
    BIO_ANCHOR_SLEEP = 0,
    BIO_ANCHOR_WAKE  = 1,
    BIO_ANCHOR_AWAY  = 2,
    BIO_ANCHOR_HOME  = 3,
    BIO_ANCHORS_COUNT = 4
};

struct BioScheduleConfig {
    bool enabled;
    bool preHeatEnabled;
    bool windowDetectionEnabled;
    uint8_t weekdayCount;
    ScheduleBlockConfig weekday[MAX_SCHEDULE_BLOCKS];
    uint8_t weekendCount;
    ScheduleBlockConfig weekend[MAX_SCHEDULE_BLOCKS];
};

struct WindowDetectionState {
    float baselineTemp;
    uint32_t baselineMs;
    bool pauseActive;
    uint32_t pauseStartMs;
    float pauseMinTemp;
    bool dropInsightLogged;
    uint32_t lastDetectionEpoch;
    uint32_t lastDetectionDurationSec;
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
    float windowDropThreshold; // Drop in <=5m to trigger pause (0.2 - 3.0 C, default 1.2 C)
    FloorType floorType;       // FLOOR_TILES (0) or FLOOR_PARQUET (1)
    float screedThicknessCm;   // 4.0 - 8.0 cm (default 5.0)
    InsulationLevel insulation;// Poor, Medium, Good, Passive
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
    bool isManualHold;
    BioAnchorId holdAnchor;
    uint32_t holdUntilMs;    // 0 = hold until next block, else timed boost expiry (millis)
    float holdRestoreTemp;   // static target restored when a boost expires with the schedule off
    float tDeadMin;          // Dynamic dead time (minutes)
    float heatingSlope;      // Rising rate (°C/h)
    float naturalLossRate;   // Natural cooling rate k_loss (°C/(h*K))
    float overshootDelta;    // Expected overshoot (°C)
    uint32_t preheatLeadMin; // Dynamic calculated preheat lead time (minutes)
    bool windowOpen;         // Rapid derivative open-window flag
    float windowDropRate;    // 1st derivative dT/dt (°C/min)
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

    // Hydraulic & Installation Settings
    bool isOpenLoopBypass() const { return _openLoopBypass; }
    void setOpenLoopBypass(bool en);
    uint32_t getBoilerDelayMs() const { return _boilerDelayMs; }
    void setBoilerDelayMs(uint32_t ms);
    bool hasManifoldTurReturSensors() const { return _hasManifoldTurReturSensors; }
    void setManifoldTurReturSensors(bool en);
    uint8_t getPumpRelayPin() const { return _pumpRelayPin; }
    void setPumpRelayPin(uint8_t pin);

    // Pump Control (Relay 4 on PIN_RELAY_SPARE / GPIO26) [SWE-REQ-005]
    bool isPumpControlEnabled() const { return _pumpControlEnabled; }
    bool isPumpActive() const { return _pumpActive; }
    void setPumpControlEnabled(bool enabled);

    // Actuator Type (NC vs NO) [SWE-REQ-002]
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

    // Sensor Ingestion Failsafe Timeout (default 600s / 10min) [SWE-REQ-013]
    uint32_t getSensorTimeoutSec() const { return _sensorTimeoutSec; }
    void setSensorTimeoutSec(uint32_t sec);

    // Boot Sensor Warmup (Grace period: max 300s / 5min) [SWE-REQ-012]
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
    void setInterlockDelayMs(uint32_t ms); // [SWE-REQ-004]
    void setPumpOverrunMs(uint32_t ms); // [SWE-REQ-005]
    void setMinBoilerOffMs(uint32_t ms);

    // Manual Hold (Temporary Override until next anchor)
    bool isManualHoldActive(size_t index) const {
        if (index >= MAX_THERMAL_ZONES) return false;
        return _statuses[index].isManualHold;
    }
    void clearManualHold(size_t index);
    bool boostZone(size_t index, float delta, uint32_t minutes);
    uint32_t getHoldRemainingSec(size_t index) const;

    // Away / Vacation (eco preset, previous preset restored on return or at awayUntil epoch)
    bool setAway(uint32_t untilEpoch);
    void endAway(bool restorePrevious);
    bool isAwayActive() const { return _awayActive; }
    uint32_t getAwayUntil() const { return _awayUntil; }

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

    // Dynamic Time Block Management (Tado / Honeywell Style)
    uint8_t getBlockCount(bool weekend) const { return weekend ? _scheduleConfig.weekendCount : _scheduleConfig.weekdayCount; }
    const ScheduleBlockConfig* getBlocks(bool weekend) const { return weekend ? _scheduleConfig.weekend : _scheduleConfig.weekday; }
    bool addBlock(bool weekend, const char* name, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]);
    bool updateBlock(bool weekend, size_t index, const char* name, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]);
    bool deleteBlock(bool weekend, size_t index);
    size_t getActiveBlockIndex() const { return _activeBlockIdx; }
    size_t getNextBlockIndex() const { return _nextBlockIdx; }
    const char* getActiveBlockName() const;
    const char* getNextBlockName() const;

    // Bio-Schedule Customization (Backward compatibility)
    bool updateAnchor(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute, const float targets[MAX_THERMAL_ZONES]);
    bool setAnchorTime(bool weekend, BioAnchorId anchor, uint8_t hour, uint8_t minute);
    bool setAnchorTargets(bool weekend, BioAnchorId anchor, const float targets[MAX_THERMAL_ZONES]);
    bool setAnchorTarget(bool weekend, BioAnchorId anchor, size_t zoneIndex, float target);
    bool setAnchorName(bool weekend, BioAnchorId anchor, const char* name);

    // Presets Engine
    String getPresetsJson() const;
    bool applyPreset(const char* presetId);
    bool savePreset(const char* presetId, const char* name, uint8_t daysMask = 0x7F);
    bool resetPreset(const char* presetId);
    void resetScheduleToDefault();
    const char* getActivePresetId() const { return _activePresetId; }
    uint8_t getPresetDays(const char* presetId) const;
    bool setPresetDays(const char* presetId, uint8_t daysMask);

    static BioScheduleConfig getFactoryDefaultSchedule();
    static BioScheduleConfig getFactoryEcoSchedule();
    static BioScheduleConfig getFactoryWfhSchedule();
    static BioAnchorId parseAnchorId(const char* name);

    BioAnchorId getActiveAnchor() const { return _activeAnchor; }
    BioAnchorId getNextAnchor() const { return _nextAnchor; }
    uint32_t getMinutesToNextAnchor() const { return _minsToNextAnchor; }
    const char* getAnchorName(BioAnchorId id) const;
    const char* getActiveAnchorName() const { return getActiveBlockName(); }
    bool isPreHeatActive() const { return _preHeatActive; }

    // Open Window Detection State
    bool isWindowPauseActive(size_t index) const;
    uint32_t getWindowPauseRemainingSec(size_t index) const;
    bool clearWindowPause(size_t index);
    void setWindowDropThreshold(size_t index, float threshold);
    float getWindowDropThreshold(size_t index) const;
    uint32_t getWindowLastDetectionEpoch(size_t index) const;
    uint32_t getWindowLastDetectionDurationSec(size_t index) const;

    // Effective Target (considers BioSchedule & Pre-Heat)
    float getEffectiveTargetTemp(size_t index) const;

    // Schedule JSON Serialization
    String getScheduleJson() const;

    // Schedule Persistence
    void loadSchedulePreferences();
    void saveSchedulePreferences();

    // Helpers
    const char* getStateStr(ZoneState state) const;

    // Thermodynamic Engine & Calibration (R1 & R2)
    ThermodynamicEngine& getThermodynamicEngine() { return _thermoEngine; }
    const ThermodynamicEngine& getThermodynamicEngine() const { return _thermoEngine; }
    bool setCalibration(size_t index, FloorType floor, float screedThicknessCm, InsulationLevel insulation);
    bool setCalibration(size_t index, const char* floorStr, float screedThicknessCm, const char* insulationStr);
    FloorType getFloorType(size_t index) const;
    float getScreedThicknessCm(size_t index) const;
    InsulationLevel getInsulationLevel(size_t index) const;
    const CalibratedMetrics& getCalibratedMetrics(size_t index) const;
    uint32_t getPreheatLeadMin(size_t index) const;
    bool isWindowOpen(size_t index) const;
    float getWindowDropRate(size_t index) const;
    float getWindowSecondDerivative(size_t index) const;

private:
    ThermalZoneConfig _configs[MAX_THERMAL_ZONES];
    ThermalZoneStatus _statuses[MAX_THERMAL_ZONES];

    // Bio-Schedule & Climate State
    BioScheduleConfig _scheduleConfig;
    char _activePresetId[16];
    bool _awayActive;
    uint32_t _awayUntil;
    char _awayPrevPreset[16];
    size_t _activeBlockIdx;
    size_t _nextBlockIdx;
    BioAnchorId _activeAnchor;
    BioAnchorId _nextAnchor;
    uint32_t _minsToNextAnchor;
    bool _preHeatActive;
    WindowDetectionState _windowStates[MAX_THERMAL_ZONES];
    ThermodynamicEngine _thermoEngine;
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

    // Hydraulic Timings & Hardware Wiring
    uint32_t _interlockDelayMs;
    uint32_t _pumpOverrunMs;
    uint32_t _minBoilerOffMs;
    uint32_t _boilerDelayMs;
    bool _openLoopBypass;
    bool _hasManifoldTurReturSensors;
    uint8_t _pumpRelayPin;

    uint32_t _lastBoilerOffTime;
    uint32_t _lastBoilerOnTime;
    uint32_t _lastEvaluationMs;
    bool _nvsSavePending;
    uint32_t _nvsSavePendingMs;

    void applyRelay(uint8_t pin, bool active);
    void updatePumpState(bool anyZoneReadyForBoiler, uint32_t now);
    void checkAntiSeizeSchedule(uint32_t now);
    void evaluateSchedule(uint32_t now);
    void evaluateWindowDetection(size_t index, float currentTemp, uint32_t now);
};

extern ThermalZoneManager thermalZones;

#endif // THERMAL_ZONE_MANAGER_H
