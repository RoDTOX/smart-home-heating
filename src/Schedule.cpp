#include "Schedule.h"
#include <algorithm>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
bool eqIgnoreCase(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

void copyStr(char* dst, const char* src, size_t dstSize) {
    if (dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dstSize) len = dstSize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

int effectiveDay(uint8_t scheduleMode, int dayOfWeek) {
    if (scheduleMode == SCHED_MODE_UNIFIED) return 1;
    if (scheduleMode == SCHED_MODE_5_2) return (dayOfWeek == 0 || dayOfWeek == 6) ? 0 : 1;
    if (dayOfWeek < 0 || dayOfWeek >= MAX_SCHEDULE_DAYS) return 1;
    return dayOfWeek;
}
}  // namespace

Schedule::Schedule() {
    begin();
}

void Schedule::begin() {
    _cfg = getFactoryDefaultSchedule();
    _activeSlotKey = -1;
    _activeLevel = LEVEL_DAY;
    _nextLevel = LEVEL_NIGHT;
    _activeMomentIdx = 0;
    _nextMomentIdx = 1;
    _minsToNextSlot = 0;
    _preHeatActive = false;
}

void Schedule::setConfig(const ScheduleConfig& cfg) {
    _cfg = cfg;
}

const char* Schedule::getLevelName(uint8_t level) {
    switch (level) {
        case LEVEL_DAY:   return "day";
        case LEVEL_NIGHT: return "night";
        case LEVEL_AWAY:  return "away";
        default:          return "unknown";
    }
}

int Schedule::parseLevel(const char* name) {
    if (!name) return -1;
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
        if (eqIgnoreCase(name, getLevelName(l))) return l;
    }
    return -1;
}

ScheduleConfig Schedule::getFactoryDefaultSchedule() {
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
    copyStr(cfg.momentNames[0], "Trezire", sizeof(cfg.momentNames[0]));
    copyStr(cfg.momentNames[1], "Plecat", sizeof(cfg.momentNames[1]));
    copyStr(cfg.momentNames[2], "Sear\xC4\x83", sizeof(cfg.momentNames[2]));
    copyStr(cfg.momentNames[3], "Noapte", sizeof(cfg.momentNames[3]));
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
    static const uint8_t wkDays[2] = {0, 6};
    for (size_t k = 0; k < 2; k++) {
        uint8_t d = wkDays[k];
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

void Schedule::setScheduleMode(uint8_t mode) {
    if (mode > SCHED_MODE_7DAY) mode = SCHED_MODE_5_2;
    _cfg.scheduleMode = mode;
}

const char* Schedule::getScheduleModeName() const {
    switch (_cfg.scheduleMode) {
        case SCHED_MODE_CONSTANT: return "constant";
        case SCHED_MODE_UNIFIED:  return "unified";
        case SCHED_MODE_7DAY:     return "7day";
        default:                  return "5+2";
    }
}

bool Schedule::setScheduleModeByName(const char* name) {
    if (!name) return false;
    if (eqIgnoreCase(name, "constant")) _cfg.scheduleMode = SCHED_MODE_CONSTANT;
    else if (eqIgnoreCase(name, "unified")) _cfg.scheduleMode = SCHED_MODE_UNIFIED;
    else if (eqIgnoreCase(name, "7day") || eqIgnoreCase(name, "7-day")) _cfg.scheduleMode = SCHED_MODE_7DAY;
    else if (eqIgnoreCase(name, "5+2") || eqIgnoreCase(name, "5_2") || eqIgnoreCase(name, "5 2") || eqIgnoreCase(name, "5-2")) _cfg.scheduleMode = SCHED_MODE_5_2;
    else return false;
    return true;
}

bool Schedule::setLevelTemp(size_t index, size_t zoneCount, ComfortLevel level, float temp) {
    if (index >= zoneCount || index >= MAX_THERMAL_ZONES || level >= LEVEL_COUNT || temp < 5.0f || temp > TARGET_MAX_C) return false;
    _cfg.levelTemps[level][index] = temp;
    if (level == LEVEL_AWAY) {
        _cfg.awayTemps[index] = encodeTempByte(temp);
    }
    return true;
}

bool Schedule::setSlots(bool weekend, const char* spec) {
    if (!spec) return false;
    ScheduleSlot parsed[MAX_SCHEDULE_SLOTS];
    uint8_t n = 0;
    char buf[160];
    copyStr(buf, spec, sizeof(buf));
    for (char* tok = strtok(buf, ", "); tok; tok = strtok(nullptr, ", ")) {
        unsigned h = 0, m = 0;
        char lvl[8] = {0};
        if (n >= MAX_SCHEDULE_SLOTS || sscanf(tok, "%u:%u=%7s", &h, &m, lvl) != 3 || h > 23 || m > 59) return false;
        int level = parseLevel(lvl);
        if (level < 0) return false;
        parsed[n++] = {(uint8_t)h, (uint8_t)m, (uint8_t)level};
    }
    if (n == 0) return false;
    std::sort(parsed, parsed + n, [](const ScheduleSlot& a, const ScheduleSlot& b) {
        return a.hour * 60 + a.minute < b.hour * 60 + b.minute;
    });
    memcpy(weekend ? _cfg.weekend : _cfg.weekday, parsed, n * sizeof(ScheduleSlot));
    (weekend ? _cfg.weekendCount : _cfg.weekdayCount) = n;
    return true;
}

void Schedule::formatSlots(bool weekend, char* out, size_t outSize) const {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    uint8_t count = weekend ? _cfg.weekendCount : _cfg.weekdayCount;
    const ScheduleSlot* slots = weekend ? _cfg.weekend : _cfg.weekday;
    size_t pos = 0;
    for (uint8_t i = 0; i < count && pos + 1 < outSize; i++) {
        int n = snprintf(out + pos, outSize - pos, "%s%02u:%02u=%s",
                         i ? "," : "", slots[i].hour, slots[i].minute, getLevelName(slots[i].level));
        if (n < 0) break;
        size_t adv = (size_t)n;
        pos = (pos + adv < outSize) ? pos + adv : outSize - 1;
    }
}

bool Schedule::setMoment(uint8_t dayIndex, uint8_t momentIndex, uint8_t hour, uint8_t minute,
                         const float* temps, size_t zoneCount) {
    if (dayIndex >= MAX_SCHEDULE_DAYS || momentIndex >= MAX_SCHEDULE_MOMENTS || hour > 23 || minute > 59) return false;
    _cfg.days[dayIndex][momentIndex].hour = hour;
    _cfg.days[dayIndex][momentIndex].minute = minute;
    if (temps) {
        for (size_t z = 0; z < zoneCount && z < MAX_THERMAL_ZONES; z++) {
            if (temps[z] >= 5.0f && temps[z] <= TARGET_MAX_C) {
                _cfg.days[dayIndex][momentIndex].temps[z] = encodeTempByte(temps[z]);
            }
        }
    }
    return true;
}

bool Schedule::setMomentForGroup(const char* group, uint8_t momentIndex, uint8_t hour, uint8_t minute,
                                 const float* temps, size_t zoneCount) {
    if (!group || momentIndex >= MAX_SCHEDULE_MOMENTS || hour > 23 || minute > 59) return false;
    if (eqIgnoreCase(group, "weekday")) {
        for (uint8_t d = 1; d <= 5; d++) setMoment(d, momentIndex, hour, minute, temps, zoneCount);
        return true;
    }
    if (eqIgnoreCase(group, "weekend")) {
        setMoment(0, momentIndex, hour, minute, temps, zoneCount);
        setMoment(6, momentIndex, hour, minute, temps, zoneCount);
        return true;
    }
    if (eqIgnoreCase(group, "all")) {
        for (uint8_t d = 0; d < MAX_SCHEDULE_DAYS; d++) setMoment(d, momentIndex, hour, minute, temps, zoneCount);
        return true;
    }
    static const char* const DAY_NAMES[7] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    for (uint8_t d = 0; d < 7; d++) {
        if (eqIgnoreCase(group, DAY_NAMES[d])) {
            return setMoment(d, momentIndex, hour, minute, temps, zoneCount);
        }
    }
    char* end = nullptr;
    long d = strtol(group, &end, 10);
    if (end && *end == '\0' && group[0] != '\0' && d >= 0 && d < MAX_SCHEDULE_DAYS) {
        return setMoment((uint8_t)d, momentIndex, hour, minute, temps, zoneCount);
    }
    return false;
}

const char* Schedule::getMomentName(uint8_t index) const {
    if (index >= MAX_SCHEDULE_MOMENTS) return "";
    if (_cfg.momentNames[index][0] == '\0') {
        switch (index) {
            case 0: return "Trezire";
            case 1: return "Plecat";
            case 2: return "Sear\xC4\x83";
            case 3: return "Noapte";
        }
    }
    return _cfg.momentNames[index];
}

bool Schedule::setMomentName(uint8_t index, const char* name) {
    if (index >= MAX_SCHEDULE_MOMENTS || !name || name[0] == '\0') return false;
    copyStr(_cfg.momentNames[index], name, sizeof(_cfg.momentNames[index]));
    return true;
}

bool Schedule::setAway(bool away) {
    if (_cfg.away == away) return false;
    _cfg.away = away;
    return true;
}

bool Schedule::setAwayTemp(size_t index, size_t zoneCount, float temp) {
    if (index >= zoneCount || index >= MAX_THERMAL_ZONES || temp < 5.0f || temp > TARGET_MAX_C) return false;
    _cfg.awayTemps[index] = encodeTempByte(temp);
    _cfg.levelTemps[LEVEL_AWAY][index] = temp;
    return true;
}

float Schedule::getAwayTemp(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 18.0f;
    if (_cfg.awayTemps[index] > 0) return decodeTempByte(_cfg.awayTemps[index]);
    return _cfg.levelTemps[LEVEL_AWAY][index];
}

void Schedule::resetToDefault() {
    bool en = _cfg.enabled;
    _cfg = getFactoryDefaultSchedule();
    _cfg.enabled = en;
}

bool Schedule::setEnabled(bool enabled) {
    if (_cfg.enabled == enabled) return false;
    _cfg.enabled = enabled;
    return true;
}

bool Schedule::setPreHeatEnabled(bool enabled) {
    if (_cfg.preHeatEnabled == enabled) return false;
    _cfg.preHeatEnabled = enabled;
    return true;
}

bool Schedule::setWindowDetectionEnabled(bool enabled) {
    if (_cfg.windowDetectionEnabled == enabled) return false;
    _cfg.windowDetectionEnabled = enabled;
    return true;
}

ScheduleEvalResult Schedule::evaluate(const ScheduleEvalInput& in) {
    ScheduleEvalResult res{false, false, false, 0, 0};
    if (!in.timeSynced) return res;

    uint8_t count = in.weekend ? _cfg.weekendCount : _cfg.weekdayCount;
    if (count == 0) return res;
    const ScheduleSlot* slots = in.weekend ? _cfg.weekend : _cfg.weekday;
    uint32_t curMinutes = (uint32_t)in.hour * 60UL + (uint32_t)in.minute;

    size_t activeIdx = count - 1;
    for (size_t i = 0; i < count; i++) {
        if (curMinutes >= (uint32_t)slots[i].hour * 60UL + (uint32_t)slots[i].minute) activeIdx = i;
    }
    size_t nextIdx = (activeIdx + 1) % count;
    uint32_t nextMinutes = (uint32_t)slots[nextIdx].hour * 60UL + (uint32_t)slots[nextIdx].minute;
    _minsToNextSlot = (nextMinutes > curMinutes) ? nextMinutes - curMinutes : 1440UL - curMinutes + nextMinutes;
    _activeLevel = slots[activeIdx].level;
    _nextLevel = slots[nextIdx].level;

    int evalDay = effectiveDay(_cfg.scheduleMode, in.dayOfWeek);
    size_t actM = MAX_SCHEDULE_MOMENTS - 1;
    for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
        uint32_t mMins = (uint32_t)_cfg.days[evalDay][m].hour * 60UL + (uint32_t)_cfg.days[evalDay][m].minute;
        if (curMinutes >= mMins) actM = m;
    }
    _activeMomentIdx = (uint8_t)actM;
    _nextMomentIdx = (uint8_t)((actM + 1) % MAX_SCHEDULE_MOMENTS);

    // Pre-Heat (FR-48): start early if the next level is warmer in any zone
    _preHeatActive = false;
    if (_cfg.preHeatEnabled && _minsToNextSlot <= in.preHeatLeadMin) {
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
            if (fminf(_cfg.levelTemps[_nextLevel][i], TARGET_MAX_C) >
                fminf(_cfg.levelTemps[_activeLevel][i], TARGET_MAX_C) + 0.2f) {
                _preHeatActive = true;
                break;
            }
        }
    }

    int16_t key = (in.weekend ? 16 : 0) + (int16_t)activeIdx;
    res.evaluated = true;
    res.activeHour = slots[activeIdx].hour;
    res.activeMinute = slots[activeIdx].minute;
    if (key != _activeSlotKey) {
        res.slotChanged = true;
        res.hadPreviousSlot = (_activeSlotKey >= 0);
        _activeSlotKey = key;
    }
    return res;
}

float Schedule::targetRaw(size_t index, bool timeSynced, int dayOfWeek, float staticTarget) const {
    if (index >= MAX_THERMAL_ZONES) return staticTarget;
    if (_cfg.away) {
        if (_cfg.awayTemps[index] > 0) return decodeTempByte(_cfg.awayTemps[index]);
        return _cfg.levelTemps[LEVEL_AWAY][index];
    }
    if (!_cfg.enabled || !timeSynced) return staticTarget;

    int evalDay = effectiveDay(_cfg.scheduleMode, dayOfWeek);
    if (_cfg.scheduleMode == SCHED_MODE_CONSTANT) {
        if (_cfg.days[evalDay][0].temps[index] > 0) {
            return decodeTempByte(_cfg.days[evalDay][0].temps[index]);
        }
        return _cfg.levelTemps[LEVEL_DAY][index];
    }

    size_t targetM = _preHeatActive ? _nextMomentIdx : _activeMomentIdx;
    if (_cfg.days[evalDay][targetM].temps[index] > 0) {
        return decodeTempByte(_cfg.days[evalDay][targetM].temps[index]);
    }
    return _cfg.levelTemps[_preHeatActive ? _nextLevel : _activeLevel][index];
}

float Schedule::getTargetTemp(size_t index, bool timeSynced, int dayOfWeek, float staticTarget) const {
    return fminf(targetRaw(index, timeSynced, dayOfWeek, staticTarget), TARGET_MAX_C);
}

void Schedule::resetZoneLevels(size_t index) {
    if (index >= MAX_THERMAL_ZONES) return;
    const ScheduleConfig f = getFactoryDefaultSchedule();
    for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
        _cfg.levelTemps[l][index] = f.levelTemps[l][index];
    }
}

void Schedule::onZoneRemoved(size_t index, size_t zoneCount) {
    if (index >= zoneCount || zoneCount == 0) return;
    for (size_t j = index; j + 1 < zoneCount; j++) {
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            _cfg.levelTemps[l][j] = _cfg.levelTemps[l][j + 1];
        }
    }
    resetZoneLevels(zoneCount - 1);
}
