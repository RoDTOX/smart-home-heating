#include "ModeManager.h"

void ModeManager::loadState(bool autoMode, uint32_t nowMs, bool seasonHeating,
                            uint8_t daysOn, uint8_t daysOff, uint8_t seasonMode) {
    _autoMode = autoMode;
    _manualStartMs = nowMs;
    _seasonHeating = seasonHeating;
    _daysOn = daysOn;
    _daysOff = daysOff;
    _coldSinceMs = 0;
    if (seasonMode == 1) _seasonHeating = true;
    if (seasonMode == 2) _seasonHeating = false;
}

bool ModeManager::setAutoMode(bool autoMode, uint32_t nowMs) {
    if (!autoMode && _autoMode) _manualStartMs = nowMs;  // FR-52: session starts on AUTO -> MANUAL transition
    bool changed = (autoMode != _autoMode);
    _autoMode = autoMode;
    return changed;
}

uint32_t ModeManager::manualRemainingSec(uint32_t nowMs, uint32_t manualTimeoutSec) const {
    if (_autoMode) return 0;
    uint32_t used = (nowMs - _manualStartMs) / 1000UL;
    return used >= manualTimeoutSec ? 0 : manualTimeoutSec - used;
}

SeasonChange ModeManager::seasonSet(bool heating, const char* why) {
    if (heating == _seasonHeating) return {false, _seasonHeating, nullptr};
    _seasonHeating = heating;
    _daysOn = 0;
    _daysOff = 0;
    return {true, heating, why};
}

SeasonChange ModeManager::setSeasonMode(uint8_t mode) {
    _coldSinceMs = 0;
    if (mode == 1) return seasonSet(true, "season mode ON");
    if (mode == 2) return seasonSet(false, "season mode OFF");
    _daysOn = 0;
    _daysOff = 0;
    return {false, _seasonHeating, nullptr};
}

SeasonChange ModeManager::seasonNewDay(float mean, uint8_t seasonMode, float heatLimit,
                                       uint8_t seasonOnDays, uint8_t seasonOffDays) {
    if (seasonMode != 0) return {false, _seasonHeating, nullptr};
    SeasonChange sc = {false, _seasonHeating, nullptr};
    if (!_seasonHeating) {
        _daysOn = (mean < heatLimit) ? (uint8_t)(_daysOn + 1) : 0;
        if (_daysOn >= seasonOnDays) sc = seasonSet(true, "cold days");
    } else {
        _daysOff = (mean > heatLimit + 2.0f) ? (uint8_t)(_daysOff + 1) : 0;
        if (_daysOff >= seasonOffDays) sc = seasonSet(false, "warm days");
    }
    return sc;
}

ModeTickResult ModeManager::tick(const ModeTickInput& in) {
    ModeTickResult res = {false, {false, _seasonHeating, nullptr}};
    if (!_autoMode && manualRemainingSec(in.nowMs, in.manualTimeoutSec) == 0) {
        _autoMode = true;
        res.manualTimedOut = true;
    }
    if (!_seasonHeating && in.seasonMode == 0) {
        if (!in.anyZoneBelowNight) {
            _coldSinceMs = 0;
        } else if (_coldSinceMs == 0) {
            _coldSinceMs = in.nowMs ? in.nowMs : 1;
        } else if (in.nowMs - _coldSinceMs >= 12UL * 3600UL * 1000UL) {
            res.seasonChanged = seasonSet(true, "zone below night level for 12 h");
        }
    } else {
        _coldSinceMs = 0;
    }
    return res;
}

bool ModeManager::isBoostExpired(bool isManualHold, uint32_t holdStartMs,
                                 uint32_t nowMs, uint32_t boostDurationSec) {
    return isManualHold && (nowMs - holdStartMs >= boostDurationSec * 1000UL);
}
