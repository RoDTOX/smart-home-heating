#ifndef PARAM_STORE_H
#define PARAM_STORE_H

#include <Arduino.h>

// ParamStore (docs/architecture.md §9): the only NVS user (rule R-A4).
// One table holds every configuration parameter of requirements §6 that the firmware uses today, with default,
// range and NVS key. Values are scaled integers, range-checked on every set and on load, and written to flash only
// when the stored integer changes (NFR-2.9). Zone records, state values and blobs follow the same write-on-change
// rule. Parameters of later migration steps are added with their feature.

enum class Param : uint8_t {
    InterlockDelay = 0,  // s       FR-2
    Overrun,             // s       FR-3
    BurnerMinRun,        // s       FR-4
    BurnerMinPause,      // s       FR-4
    Hysteresis,          // 0.01 C  FR-47
    SensorTimeout,       // s       FR-16.2, FR-18
    FrostTemp,           // 0.1 C   FR-16.1
    WindowThreshold,     // 0.1 C   FR-40
    HbInterval,          // s       FR-17.7, FR-55
    BoilerLinkHeartbeat, // 0 / 1   FR-55
    OpenLoopBypass,      // 0 / 1   FR-1.3
    ActuatorType,        // 0 = NC, 1 = NO   FR-15.1
    PumpOutput,          // 0 = none, 1 = relay 4   FR-14.1
    BoilerSetTemp,       // C       FR-35.1
    BoilerFlowObserved,  // C       FR-35.1
    GasFlameRate,        // 0.01 m3/h  FR-35.1
    BoilerPowerKw,       // kW      FR-35.1
    GasPrice,            // 0.01 Lei/m3 FR-35.1
    LoginAttempts,       // failed console logins in a row before the lockout   FR-75.4
    LoginLockout,        // s       FR-75.4
    TelnetLoginTimeout,  // s       FR-75.4
    ValidMin,            // 0.1 C   FR-53
    ValidMax,            // 0.1 C   FR-53
    MaxJump,             // 0.1 C   FR-53
    ManualTimeout,       // s       FR-52
    BoostDuration,       // s       FR-39
    HeatLimit,           // C       FR-12.2
    SeasonOnDays,        // days    FR-12.2
    SeasonOffDays,       // days    FR-12.2
    SeasonMode,          // 0 = AUTO, 1 = ON, 2 = OFF   FR-12.2
    HeatingType,         // 0 = floor, 1 = radiators   FR-40
    BoilerLinkType,      // 0 = rf433, 1 = wired, 2 = receiver   FR-55, FR-69
    FaultDuty,           // % (0..30%, default 20%) FR-51
    FaultFrostOutdoor,   // C (-10..10 C, default 5 C) FR-51, FR-64
    ExerciseIdleDays,    // days (7..14, default 14) FR-19
    SelfTestBurn,        // min (5..20, default 10) FR-60
    OverheatMargin,      // 0.1 C (1.0..5.0 C, default 2.0 C) FR-56
    OverheatTime,        // min (30..180 min, default 60 min) FR-56
    NoEffectDuty,        // % (30..100%, default 50%) FR-57
    NoEffectTime,        // h (1..6 h, default 3 h) FR-57
    NoEffectRise,        // 0.1 C (0.1..1.0 C, default 0.2 C) FR-57
    Count
};

// Persistent state (not configuration): written on change only.
enum class StateKey : uint8_t {
    AutoMode = 0,
    AntiSeizeEpoch,
    BoilerLastOn,
    RuntimeTotal,
    RuntimeToday,
    RuntimeYesterday,
    RuntimeThisMonth,
    RuntimeLastMonth,
    RuntimeDay,
    RuntimeMonth,
    ZoneCount,  // number of zones (FR-69, §6 `number of zones`)
    GasMeterBaseIndex,  // last typed reading, thousandths of m3 (FR-35.1)
    GasMeterBaseSec,    // boiler runtime totalSec when the last reading was stored (FR-35.1)
    GasMeterBaseEpoch,  // time of the last typed reading, 0 = none (FR-35.1)
    GasLogMigrated,     // 1 = the old calibration ring was dropped and the reading log started (FR-35.1)
    GasCompacted,       // oldest readings replaced because the log was full (FR-35.1)
    SeasonHeating,      // 1 = season state HEATING, 0 = SUMMER (FR-12.2)
    SeasonDaysOn,       // days in a row below heatLimit (FR-12.2)
    SeasonDaysOff,      // days in a row above heatLimit + 2 C (FR-12.2)
    SeasonDayKey,       // last day whose mean was counted (FR-12.2)
    Count
};

enum ParamFlags : uint8_t {
    P_NONE = 0,
    P_AUTH = 1,      // write needs authentication once FR-58 exists (migration step 9)
    P_LEARNED = 2,   // learned value, NFR-5.3
};

struct ParamDef {
    const char* name;  // requirements §6 name
    const char* key;   // NVS key (namespace "cfg")
    int32_t def;
    int32_t min;
    int32_t max;
    uint8_t scale;     // stored integer = value * scale
    uint8_t flags;
};

#define PARAM_ZONES 32  // zone slots reserved statically (FR-69, NFR-2.6)
#define PARAM_LEARNED_MAX 600  // learned TPI base duty, per mille (0..60 %, FR-47)

// Installation data of one zone (FR-11.3, FR-11.4). Empty strings until commissioning (FR-11.1).
struct ZoneRecord {
    char name[32];
    char sensorMac[18];
    char sensorName[32];
    uint8_t relayPin;
    uint8_t enabled;
    int16_t offsetC10;   // sensor offset, 0.1 C (FR-49, -30..30)
    int16_t targetC10;   // manual target, 0.1 C
};

class ParamStore {
public:
    void begin();

    // Configuration parameters
    static const ParamDef& def(Param p);
    int32_t get(Param p) const { return _values[(uint8_t)p]; }
    float getf(Param p) const { return (float)get(p) / def(p).scale; }
    bool inRange(Param p, int32_t raw) const;
    bool set(Param p, int32_t raw);      // false: out of range, nothing changed
    bool setf(Param p, float value);     // value in units, rounded to the scale
    static int32_t toRaw(Param p, float value);

    // Zone records
    ZoneRecord zone(size_t i) const;  // read from NVS (boot and migration only): 32 cached records would cost 2.9 KB of heap
    void saveZone(size_t i, const ZoneRecord& z);

    // Learned TPI base duty per zone, per mille (NFR-5.3 bounds)
    int16_t learned(size_t i) const { return i < PARAM_ZONES ? _learned[i] : 0; }
    void saveLearned(size_t i, int16_t perMille);
    uint8_t learnedResets() const { return _learnedResets; }  // records discarded at load (DTC-301)

    // State
    uint32_t state(StateKey k) const { return _state[(uint8_t)k]; }
    void saveState(StateKey k, uint32_t v);

    // Location "lat,lon" (FR-9.2)
    const char* location() const { return _location; }
    bool setLocation(const char* coords);  // false: not "lat,lon" within -90..90 / -180..180
    static bool validLocation(const char* coords);
    static bool validRelayPin(uint8_t pin);

    // Opaque blob owned by a caller module (schedule); the caller validates the content.
    bool loadBlob(const char* key, void* buf, size_t len);
    void saveBlob(const char* key, const void* buf, size_t len);
    void eraseBlob(const char* key);

    uint8_t schemaVersion() const { return _schema; }

private:
    int32_t _values[(uint8_t)Param::Count];
    int16_t _learned[PARAM_ZONES];
    uint32_t _state[(uint8_t)StateKey::Count];
    char _location[32];
    uint8_t _schema = 0;
    uint8_t _learnedResets = 0;

    void loadAll();
    void migrateLegacy();
    bool writeParam(Param p);
    bool writeZone(size_t i, const ZoneRecord& z);
    bool writeLearned(size_t i);
    bool writeState(StateKey k);
    bool writeLocation();
    static void defaultZone(size_t i, ZoneRecord& z);
    static bool validZone(const ZoneRecord& z);
};

extern ParamStore paramStore;

#endif // PARAM_STORE_H
