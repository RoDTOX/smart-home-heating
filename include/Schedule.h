#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MAX_THERMAL_ZONES
#define MAX_THERMAL_ZONES 32  // zone slots reserved statically (FR-69)
#endif

// Comfort levels: each zone has one temperature per level (FR-29)
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

// From hour:minute on, all zones follow `level` (FR-30)
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

struct ScheduleEvalInput {
    bool timeSynced;
    bool weekend;
    uint8_t hour;
    uint8_t minute;
    int dayOfWeek;           // 0=Sun..6=Sat
    uint32_t preHeatLeadMin; // FR-48: weather-dependent lead from HeatUpModel
};

struct ScheduleEvalResult {
    bool evaluated;
    bool slotChanged;
    bool hadPreviousSlot;
    uint8_t activeHour;
    uint8_t activeMinute;
};

/**
 * Pure schedule state & evaluation (FR-29, FR-30, FR-31, FR-48, FR-61).
 * No Arduino.h, no millis(), no heap allocation; caller passes clock & pre-heat lead.
 */
class Schedule {
public:
    Schedule();

    void begin();
    void setConfig(const ScheduleConfig& cfg);
    const ScheduleConfig& getConfig() const { return _cfg; }
    ScheduleConfig& mutableConfig() { return _cfg; }

    static ScheduleConfig getFactoryDefaultSchedule();
    static const char* getLevelName(uint8_t level);
    static int parseLevel(const char* name);  // -1 if invalid

    void setScheduleMode(uint8_t mode);
    uint8_t getScheduleMode() const { return _cfg.scheduleMode; }
    const char* getScheduleModeName() const;
    bool setScheduleModeByName(const char* name);

    bool setLevelTemp(size_t index, size_t zoneCount, ComfortLevel level, float temp);
    bool setSlots(bool weekend, const char* spec);
    void formatSlots(bool weekend, char* out, size_t outSize) const;

    bool setMoment(uint8_t dayIndex, uint8_t momentIndex, uint8_t hour, uint8_t minute,
                   const float* temps, size_t zoneCount);
    bool setMomentForGroup(const char* group, uint8_t momentIndex, uint8_t hour, uint8_t minute,
                           const float* temps, size_t zoneCount);
    const char* getMomentName(uint8_t index) const;
    bool setMomentName(uint8_t index, const char* name);

    bool setAway(bool away);
    bool setAwayTemp(size_t index, size_t zoneCount, float temp);
    float getAwayTemp(size_t index) const;
    void resetToDefault();
    bool setEnabled(bool enabled);
    bool setPreHeatEnabled(bool enabled);
    bool setWindowDetectionEnabled(bool enabled);

    bool isEnabled() const { return _cfg.enabled; }
    bool isPreHeatEnabled() const { return _cfg.preHeatEnabled; }
    bool isWindowDetectionEnabled() const { return _cfg.windowDetectionEnabled; }
    bool isAway() const { return _cfg.away; }

    uint8_t getActiveLevel() const { return _activeLevel; }
    uint8_t getNextLevel() const { return _nextLevel; }
    uint8_t getActiveMomentIdx() const { return _activeMomentIdx; }
    uint8_t getNextMomentIdx() const { return _nextMomentIdx; }
    uint32_t getMinutesToNextSlot() const { return _minsToNextSlot; }
    bool isPreHeatActive() const { return _preHeatActive; }

    ScheduleEvalResult evaluate(const ScheduleEvalInput& in);

    float targetRaw(size_t index, bool timeSynced, int dayOfWeek, float staticTarget) const;
    float getTargetTemp(size_t index, bool timeSynced, int dayOfWeek, float staticTarget) const;

    void resetZoneLevels(size_t index);
    void onZoneRemoved(size_t index, size_t zoneCount);

private:
    ScheduleConfig _cfg;
    int16_t _activeSlotKey;
    uint8_t _activeLevel;
    uint8_t _nextLevel;
    uint8_t _activeMomentIdx;
    uint8_t _nextMomentIdx;
    uint32_t _minsToNextSlot;
    bool _preHeatActive;
};

#endif
