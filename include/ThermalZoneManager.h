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

// Comfort levels: each zone has one temperature per level
enum ComfortLevel : uint8_t {
    LEVEL_DAY = 0,
    LEVEL_NIGHT,
    LEVEL_AWAY,
    LEVEL_COUNT
};

#define MAX_SCHEDULE_SLOTS 8

// From hour:minute on, all zones follow `level`
struct ScheduleSlot {
    uint8_t hour;
    uint8_t minute;
    uint8_t level;
};

struct ScheduleConfig {
    bool enabled;
    bool preHeatEnabled;
    bool windowDetectionEnabled;
    bool away;  // Global away override (ignores slots)
    float levelTemps[LEVEL_COUNT][MAX_THERMAL_ZONES];
    uint8_t weekdayCount;
    ScheduleSlot weekday[MAX_SCHEDULE_SLOTS];
    uint8_t weekendCount;
    ScheduleSlot weekend[MAX_SCHEDULE_SLOTS];
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
    float windowDropThreshold; // Drop in <=5m to trigger pause (0.8 - 2.0 C, default 1.2 C)
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
    void setInterlockDelayMs(uint32_t ms);
    void setPumpOverrunMs(uint32_t ms);
    void setMinBoilerOffMs(uint32_t ms);

    // Manual Hold (Temporary Override until next schedule slot)
    bool isManualHoldActive(size_t index) const {
        if (index >= MAX_THERMAL_ZONES) return false;
        return _statuses[index].isManualHold;
    }
    void clearManualHold(size_t index);

    // Manual Control
    void setRelayManual(size_t index, bool on);
    void setPumpManual(bool on);
    void setBoilerManual(BoilerRadioTrigger& boiler, bool on);

    // Schedule: same slots for all zones, each slot selects a comfort level
    const ScheduleConfig& getScheduleConfig() const { return _scheduleConfig; }
    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const { return _scheduleConfig.enabled; }
    void setPreHeatEnabled(bool enabled);
    bool isPreHeatEnabled() const { return _scheduleConfig.preHeatEnabled; }
    void setWindowDetectionEnabled(bool enabled);
    bool isWindowDetectionEnabled() const { return _scheduleConfig.windowDetectionEnabled; }
    void setAway(bool away);
    bool isAway() const { return _scheduleConfig.away; }
    bool setLevelTemp(size_t index, ComfortLevel level, float temp);
    bool setSlots(bool weekend, const char* spec);  // e.g. "06:30=day,22:30=night"
    String getSlotsString(bool weekend) const;
    void resetScheduleToDefault();
    static ScheduleConfig getFactoryDefaultSchedule();
    static const char* getLevelName(uint8_t level);
    static int parseLevel(const char* name);  // -1 if invalid

    uint8_t getActiveLevel() const { return _activeLevel; }
    uint8_t getNextLevel() const { return _nextLevel; }
    uint32_t getMinutesToNextSlot() const { return _minsToNextSlot; }
    bool isPreHeatActive() const { return _preHeatActive; }

    // Open Window Detection State
    bool isWindowPauseActive(size_t index) const;
    uint32_t getWindowPauseRemainingSec(size_t index) const;
    bool clearWindowPause(size_t index);
    void setWindowDropThreshold(size_t index, float threshold);
    float getWindowDropThreshold(size_t index) const;
    uint32_t getWindowLastDetectionEpoch(size_t index) const;
    uint32_t getWindowLastDetectionDurationSec(size_t index) const;

    // Effective Target (considers hold, away, schedule & pre-heat)
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

    // Schedule State
    ScheduleConfig _scheduleConfig;
    int16_t _activeSlotKey;  // weekend * 16 + slot index, -1 = none
    uint8_t _activeLevel;
    uint8_t _nextLevel;
    uint32_t _minsToNextSlot;
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
