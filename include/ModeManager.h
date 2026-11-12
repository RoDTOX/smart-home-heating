#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdint.h>

// Operating mode and season (architecture §7.5, §7.6; FR-64):
// MANUAL while a manual session runs, else AUTO in season HEATING and OFF in season SUMMER.
enum class EffMode : uint8_t { Auto, Manual, Off };

struct SeasonChange {
    bool changed;
    bool heating;
    const char* reason;
};

struct ModeTickInput {
    uint32_t nowMs;
    uint32_t manualTimeoutSec;
    uint8_t seasonMode;
    bool anyZoneBelowNight;
};

struct ModeTickResult {
    bool manualTimedOut;
    SeasonChange seasonChanged;
};

// Pure operating-mode and season state machine (no Arduino.h, no millis(), no String).
class ModeManager {
public:
    void loadState(bool autoMode, uint32_t nowMs, bool seasonHeating,
                   uint8_t daysOn, uint8_t daysOff, uint8_t seasonMode);

    // Returns true when _autoMode actually changed.
    bool setAutoMode(bool autoMode, uint32_t nowMs);
    bool isAutoMode() const { return _autoMode; }

    EffMode effectiveMode() const {
        return !_autoMode ? EffMode::Manual : (_seasonHeating ? EffMode::Auto : EffMode::Off);
    }

    // FR-52: remaining seconds in MANUAL mode, or 0 when in AUTO.
    uint32_t manualRemainingSec(uint32_t nowMs, uint32_t manualTimeoutSec) const;

    bool isSeasonHeating() const { return _seasonHeating; }
    uint8_t daysOn() const { return _daysOn; }
    uint8_t daysOff() const { return _daysOff; }

    // 0 AUTO, 1 ON, 2 OFF (FR-12.2).
    SeasonChange setSeasonMode(uint8_t mode);

    // FR-12.2: ON after seasonOnDays days < heatLimit, OFF after seasonOffDays days > heatLimit + 2 C.
    SeasonChange seasonNewDay(float mean, uint8_t seasonMode, float heatLimit,
                              uint8_t seasonOnDays, uint8_t seasonOffDays);

    // FR-52 manual timeout + FR-12.2 12 h season safety net.
    ModeTickResult tick(const ModeTickInput& in);

    // FR-39: boost ends after boostDuration seconds regardless of wall-clock sync.
    static bool isBoostExpired(bool isManualHold, uint32_t holdStartMs,
                               uint32_t nowMs, uint32_t boostDurationSec);

private:
    SeasonChange seasonSet(bool heating, const char* why);

    bool _autoMode = true;
    bool _seasonHeating = true;
    uint8_t _daysOn = 0;
    uint8_t _daysOff = 0;
    uint32_t _manualStartMs = 0;
    uint32_t _coldSinceMs = 0;
};

#endif // MODE_MANAGER_H
