#ifndef THERMAL_ZONE_MANAGER_H
#define THERMAL_ZONE_MANAGER_H

#include <Arduino.h>
#include "EventLog.h"
#include "Config.h"
#include "ParamStore.h"
#include "BoilerRadioTrigger.h"

#define MAX_THERMAL_ZONES 32  // zone slots reserved statically (FR-69); the zones in use are getZoneCount()

// Time-proportional (TPI) control: each hour the valve is open for duty * cycle.
// Floor screed reacts in hours, so dosing energy beats ON/OFF hysteresis (no overshoot).
#define TPI_CYCLE_MS      (60UL * 60UL * 1000UL)
#define TPI_MIN_ON_MS     (10UL * 60UL * 1000UL)  // Wax actuators need 3-5 min just to open
#define TPI_BAND_C        1.5f                     // 1.5 C below target -> 100 %
#define TPI_KI            0.05f                    // Learned duty change per hour per C of error
#define TPI_MAX_INTEGRAL  0.6f

struct TpiState {
    uint32_t cycleStartMs;
    uint32_t onMs;
    float target;
    float duty;      // Duty of the current cycle (0..1)
    float integral;  // Learned base duty that holds the target (heat loss)
    float savedIntegral;  // Last value written to NVS
};

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

enum HeatingType : uint8_t {
    HEATING_TYPE_FLOOR = 0,
    HEATING_TYPE_RADIATORS = 1
};

// Comfort levels: each zone has one temperature per level
enum ComfortLevel : uint8_t {
    LEVEL_DAY = 0,
    LEVEL_NIGHT,
    LEVEL_AWAY,
    LEVEL_COUNT
};

#define MAX_SCHEDULE_SLOTS 8
#define MAX_SCHEDULE_MOMENTS 4
#define MAX_SCHEDULE_DAYS 7

enum ScheduleMode : uint8_t {
    SCHED_MODE_5_2 = 0,
    SCHED_MODE_CONSTANT = 1,
    SCHED_MODE_UNIFIED = 2,
    SCHED_MODE_7DAY = 3
};

// FR-1.2, FR-29 (C-36): no target, level or boost above this, boost included
constexpr float TARGET_MAX_C = 28.0f;

// Encodes 5.0..30.0 C with 0.1 C resolution into 0..250
inline uint8_t encodeTempByte(float t) {
    if (t < 5.0f) t = 5.0f;
    if (t > 30.0f) t = 30.0f;
    return (uint8_t)lroundf((t - 5.0f) * 10.0f);
}
inline float decodeTempByte(uint8_t b) {
    return 5.0f + (float)b / 10.0f;
}

struct MomentConfig {
    uint8_t hour;
    uint8_t minute;
    uint8_t temps[MAX_THERMAL_ZONES]; // 0..250 maps to 5.0..30.0 C
};

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
    uint8_t scheduleMode; // 0 = 5+2, 1 = constant, 2 = unified, 3 = 7day
    float levelTemps[LEVEL_COUNT][MAX_THERMAL_ZONES];
    uint8_t weekdayCount;
    ScheduleSlot weekday[MAX_SCHEDULE_SLOTS];
    uint8_t weekendCount;
    ScheduleSlot weekend[MAX_SCHEDULE_SLOTS];
    MomentConfig days[MAX_SCHEDULE_DAYS][MAX_SCHEDULE_MOMENTS]; // 0=Sun, 1=Mon ... 6=Sat
    uint8_t awayTemps[MAX_THERMAL_ZONES];
    char momentNames[MAX_SCHEDULE_MOMENTS][16];
};

struct WindowDetectionState {
    float baselineTemp;
    uint32_t baselineMs;
    float bucketMax[15];     // peak per 60 s bucket: 15 min sliding baseline for adaptive detection (FR-40)
    uint8_t bucketHead;
    uint32_t bucketStartMs;
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
    bool enabled;
    float tempOffset;          // Sensor calibration offset added to the reading (-3..+3 C)
    bool solarGain;            // FR-70: zone participates in solar throttling (e.g. South/West exposure)
    uint8_t circuits;          // FR-63: underfloor heating loops/circuits (1..16, default 1)
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
    uint32_t energisedMs;  // millis() when the valve was energised: interlockDelay counts from here (FR-2)
    bool isManualHold;
    uint32_t holdStartMs;  // boostDuration counts from here (FR-39)
};

class ThermalZoneManager {
public:
    ThermalZoneManager();

    void begin();
    void update(BoilerRadioTrigger& boiler);

    // Persistence (ParamStore, NVS writes only on change)
    void loadConfig();
    void persistConfig();
    void flushPendingNvs();

    // Getters
    size_t getZoneCount() const { return _zoneCount; }
    // FR-73 / FR-74: zones are added and deleted by the user; numbers stay 1..N
    enum class ZoneResult : uint8_t { Ok, NoFreeOutput, NoSuchZone, ValveEnergised };
    ZoneResult addZone(const char* name, size_t& index);  // name empty: "Zone n"
    ZoneResult deleteZone(size_t index);
    int firstFreeRelayPin() const;  // lowest free output in the order of the board's relays, -1 when none
    bool isRelayOutput(uint8_t pin) const { return ParamStore::validRelayPin(pin); }
    const ThermalZoneConfig& getConfig(size_t index) const;
    const ThermalZoneStatus& getStatus(size_t index) const;
    bool isBoilerDemanded() const { return _boilerDemand; }
    // FR-2: Boiler ON only after a valve has been energised for interlockDelay (MANUAL with openLoopBypass: always)
    bool boilerOnPermitted() const;
    bool isAutoMode() const { return _autoMode; }
    // Operating mode and season (architecture 7.5, 7.6): MANUAL while a manual session runs, else AUTO in season
    // HEATING and OFF (global OFF, FR-64) in season SUMMER
    enum class EffMode : uint8_t { Auto, Manual, Off };
    EffMode effectiveMode() const { return !_autoMode ? EffMode::Manual : (_seasonHeating ? EffMode::Auto : EffMode::Off); }
    uint32_t manualRemainingSec() const;   // FR-52, 0 when not in MANUAL
    bool isSeasonHeating() const { return _seasonHeating; }
    void setSeasonMode(uint8_t mode);      // 0 AUTO, 1 ON, 2 OFF (FR-12.2)
    void seasonNewDay(float prevDayMean);  // a new previous-day mean arrived (FR-12.2)
    bool setTargetTemperatureChecked(size_t index, float target);  // false: not a valid boost step (FR-1.2)
    uint32_t getInterlockDelayMs() const { return _interlockDelayMs; }
    uint32_t getPumpOverrunMs() const { return _pumpOverrunMs; }
    // Requirements §6 ranges are enforced by ParamStore: a setter returns false and changes nothing when out of range
    bool setInterlockDelaySec(uint32_t sec);
    bool setOverrunSec(uint32_t sec);

    // Hydraulic & Installation Settings
    bool isOpenLoopBypass() const { return _openLoopBypass; }
    void setOpenLoopBypass(bool en);
    uint8_t getPumpRelayPin() const { return PIN_RELAY_SPARE; }

    // Pump Control (Relay 4 on PIN_RELAY_SPARE / GPIO26)
    bool isPumpControlEnabled() const { return _pumpControlEnabled; }
    bool isPumpActive() const { return _pumpActive; }
    uint8_t eventContext() const;  // FR-62.1: boiler side state for every log entry
    bool setPumpControlEnabled(bool enabled);  // false: a zone uses the pump relay (FR-11.3), nothing changed
    // FR-11.3: index of the zone (other than exceptIndex) driving this relay output, -1 when none
    int zoneUsingRelay(uint8_t pin, size_t exceptIndex) const;

    // Actuator Type (NC vs NO)
    bool isActuatorNormallyClosed() const;  // held by OutputBank (FR-15.1)
    void setActuatorNormallyClosed(bool nc);

    // Frost Protection (<12C)
    float getFreezeProtectionTemp() const { return _freezeProtectionTemp; }
    bool setFreezeProtectionTemp(float temp);
    bool isFrostProtectionActive() const { return _frostProtectionActive; }

    // Summer Anti-Seize Exercise (Pumps & Valves)
    void triggerAntiSeizeExercise();
    bool isAntiSeizeRunning() const { return _antiSeizeRunning; }
    uint32_t getLastAntiSeizeEpoch() const { return _lastAntiSeizeEpoch; }

    // Sensor timeout `sensorTimeout` (FR-16.2, FR-18)
    uint32_t getSensorTimeoutSec() const { return _sensorTimeoutSec; }
    bool setSensorTimeoutSec(uint32_t sec);

    // One global hysteresis (overshoot cut, FR-47) and window threshold (FR-40), requirements §6
    float getHysteresis() const { return _hysteresis; }
    bool setHysteresis(float hyst);
    float getWindowThreshold() const { return _windowThreshold; }
    bool setWindowThreshold(float threshold);

    // Boot Sensor Warmup (Grace period: max 300s / 5min)
    bool isBootWarmupActive() const { return _bootWarmupActive; }
    uint32_t getBootWarmupElapsedSec() const;
    uint32_t getBootWarmupRemainingSec() const;
    size_t getPairedZoneCount() const;
    size_t getAcquiredZoneCount() const;

    // Zone heating runtime counters (today / yesterday) - single source of truth
    uint32_t getZoneRuntimeTodaySec(size_t index) const;
    uint32_t getZoneRuntimeYesterdaySec(size_t index) const;
    void updateRuntimeDay(uint8_t day);
    void saveRuntimeToFlash();
    void loadRuntimeFromFlash();
    void syncRuntimeFromHistory();

    // Setters
    void setTargetTemperature(size_t index, float target);
    void setZoneEnabled(size_t index, bool enabled);
    void setZoneName(size_t index, const char* name);
    void setSensorName(size_t index, const char* sensorName);
    void setSensorMac(size_t index, const char* sensorMac);
    void setRelayPin(size_t index, uint8_t pin);
    bool setTempOffset(size_t index, float offset);
    void setSolarGain(size_t index, bool enabled);
    bool isSolarGain(size_t index) const { return index < MAX_THERMAL_ZONES ? _configs[index].solarGain : false; }
    void setCircuits(size_t index, uint8_t circuits);
    uint8_t getCircuits(size_t index) const { return index < MAX_THERMAL_ZONES ? _configs[index].circuits : 1; }

    // TPI control state (for diagnostics)
    float getDuty(size_t index) const { return index < MAX_THERMAL_ZONES ? _tpi[index].duty : 0.0f; }
    uint32_t getZoneOnMs(size_t index) const { return index < MAX_THERMAL_ZONES ? _tpi[index].onMs : 0; }
    float getLearnedDuty(size_t index) const { return index < MAX_THERMAL_ZONES ? _tpi[index].integral : 0.0f; }
    // Learned base duty in % with one decimal (the resolution of the stored per-mille value), 0..60 (FR-47)
    float getLearnedDutyPct(size_t index) const;
    void setLearnedDutyPct(size_t index, float pct);  // FR-65.3: stored on change only (NFR-2.9)
    uint32_t getPreHeatLeadMinutes() const;
    void mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, bool enabled);
    void setAutoMode(bool autoMode);

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

    void setScheduleMode(uint8_t mode);
    uint8_t getScheduleMode() const { return _scheduleConfig.scheduleMode; }
    const char* getScheduleModeName() const;
    bool setScheduleModeByName(const char* name);
    bool setMoment(uint8_t dayIndex, uint8_t momentIndex, uint8_t hour, uint8_t minute, const float* temps);
    bool setMomentForGroup(const char* group, uint8_t momentIndex, uint8_t hour, uint8_t minute, const float* temps);
    bool setAwayTemp(size_t index, float temp);
    float getAwayTemp(size_t index) const;
    const char* getMomentName(uint8_t index) const;
    bool setMomentName(uint8_t index, const char* name);

    // Open Window Detection State
    bool isWindowPauseActive(size_t index) const;
    bool isWindowDetected(size_t index) const;
    uint32_t getWindowPauseRemainingSec(size_t index) const;
    bool clearWindowPause(size_t index);
    uint32_t getWindowLastDetectionEpoch(size_t index) const;
    uint32_t getWindowLastDetectionDurationSec(size_t index) const;
    uint8_t getHeatingType() const { return _heatingType; }
    void setHeatingType(uint8_t type);

    // Effective Target (considers hold, away, schedule & pre-heat)
    float getEffectiveTargetTemp(size_t index) const;
    float getScheduleTargetTemp(size_t index) const;
    // FR-1.2: boost only while the held target is above the schedule level
    bool isBoost(size_t index) const {
        return index < MAX_THERMAL_ZONES && _statuses[index].isManualHold &&
               _configs[index].targetTemp > getScheduleTargetTemp(index) + 0.05f;
    }

    // Schedule JSON Serialization
    String getScheduleJson() const;

    // Schedule Persistence
    void loadSchedule();
    void saveSchedule();

    // Helpers
    // FR-8.4: displayed zone state code (FROST, DISABLED, FAULT, ACQUIRING, WINDOW, HEATING, IDLE)
    const char* getStateCode(size_t index) const;

private:
    float scheduleTargetRaw(size_t index) const;  // schedule level before the ceiling of C-36
    ThermalZoneConfig _configs[MAX_THERMAL_ZONES];
    ThermalZoneStatus _statuses[MAX_THERMAL_ZONES];

    // Schedule State
    ScheduleConfig _scheduleConfig;
    int16_t _activeSlotKey;  // weekend * 16 + slot index, -1 = none
    uint8_t _activeLevel;
    uint8_t _nextLevel;
    uint8_t _activeMomentIdx = 0;
    uint8_t _nextMomentIdx = 1;
    uint32_t _minsToNextSlot;
    bool _preHeatActive;
    WindowDetectionState _windowStates[MAX_THERMAL_ZONES];
    TpiState _tpi[MAX_THERMAL_ZONES];
    uint8_t _zoneCount = 0;
    uint32_t _zoneRuntimeTodaySec[MAX_THERMAL_ZONES];
    uint32_t _zoneRuntimeYesterdaySec[MAX_THERMAL_ZONES];
    uint32_t _zoneRuntimeMsAcc[MAX_THERMAL_ZONES];
    uint32_t _lastZoneRuntimeMs = 0;
    uint8_t _lastDay = 0;
    void clearZoneSlot(size_t index);
    uint32_t _lastScheduleEvalMs;

    bool _autoMode;
    bool _seasonHeating = true;
    uint8_t _daysOn = 0, _daysOff = 0;
    uint32_t _manualStartMs = 0;
    uint32_t _coldSinceMs = 0;       // season safety net: some zone below its night level since then (FR-12.2)
    void seasonSet(bool heating, const char* why);
    void modeTick(uint32_t now);
    bool _boilerDemand;
    bool _pumpControlEnabled;
    bool _pumpActive;
    bool _boilerWasOn;          // boiler state seen at the last tick: its fall starts the overrun (FR-3)
    bool _overrunActive;        // FR-3: the flame was lit and the OFF is sent, the keeper valve is held
    uint32_t _overrunUntilMs;
    int8_t _overrunKeeperZone;
    bool _holdLogged = false;                          // FR-62.1: the held close of the keeper valve is logged once
    bool _valveReady[MAX_THERMAL_ZONES] = {false};     // FR-62.1: interlock end seen for the energised valve
    float _freezeProtectionTemp;
    bool _frostProtectionActive;
    bool _antiSeizeRunning;
    uint32_t _antiSeizeStartMs;
    uint32_t _lastAntiSeizeEpoch;
    uint32_t _sensorTimeoutSec;
    float _hysteresis;
    float _windowThreshold;
    uint8_t _heatingType = 0;
    bool _bootWarmupActive;
    uint32_t _bootWarmupStartMs;

    // Hydraulic timings (requirements §6, loaded from ParamStore)
    uint32_t _interlockDelayMs;
    uint32_t _pumpOverrunMs;
    bool _openLoopBypass;

    uint32_t _lastBoilerOffTime;
    uint32_t _lastBoilerOnTime;
    uint32_t _lastEvaluationMs;
    volatile bool _nvsSavePending;
    volatile uint32_t _nvsSavePendingMs;

    void applyRelay(uint8_t pin, bool active, uint8_t reason = RS_NONE);
    bool valveOpen(size_t index, uint32_t now) const;  // energised for at least interlockDelay (FR-2)
    bool anyValveOpen(uint32_t now) const;
    void trackBoilerState(uint32_t now);                // starts and ends the overrun window (FR-3)
    void arbitrateValves(uint32_t now, bool& anyReady, bool& anyReadyFrost);  // FR-37, FR-3, FR-3.2
    void arbitrateNow();
    void evaluateFrost(size_t index, bool found, float currentTemp);  // FR-16.1, every zone, every mode
    void checkAntiSeizeSchedule(uint32_t now);
    void evaluateSchedule(uint32_t now);
    void evaluateWindowDetection(size_t index, float currentTemp, uint32_t now);
    bool evaluateTpi(size_t index, float target, uint32_t now);
};

extern ThermalZoneManager thermalZones;

#endif // THERMAL_ZONE_MANAGER_H
