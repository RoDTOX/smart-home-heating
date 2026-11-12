#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * SafetySupervisor (L3 Safety Layer, Step 4 / I-4, architecture.md §6)
 * Pure safety arbiter: valve interlock (FR-2), overrun & keeper (FR-3, FR-3.2),
 * frost guard (FR-16.1), and open-loop bypass permit (FR-1.3).
 * No Arduino.h, no millis(), no heap allocation; caller passes time and zone inputs.
 */
class SafetySupervisor {
public:
    static constexpr size_t MAX_ZONES = 32;
    static_assert(MAX_ZONES <= 32, "demandMask and frostMask are 32-bit bitmasks");

    enum class OverrunTransition : uint8_t {
        None = 0,
        Started,
        Finished
    };

    enum ValveActionFlag : uint8_t {
        VA_NONE            = 0,
        VA_OPEN_CHANGED    = 1 << 0,
        VA_OPEN_TARGET     = 1 << 1,
        VA_OVERRUN_END     = 1 << 2,
        VA_HOLD_STARTED    = 1 << 3,
        VA_INTERLOCK_DONE  = 1 << 4
    };

    struct ValveAction {
        uint8_t flags;
        uint16_t interlockElapsedSec;
        bool has(uint8_t f) const { return (flags & f) != 0; }
    };

    struct FrostEvalResult {
        bool frostActive = false;
        bool becameActive = false;
        bool becameInactive = false;
    };

    SafetySupervisor();

    void begin(uint32_t interlockDelayMs = 180000UL,
               uint32_t pumpOverrunMs = 180000UL,
               float freezeProtectionTemp = 5.0f);

    // Configuration setters (parameters stay bounded by ParamStore / callers)
    void setInterlockDelayMs(uint32_t ms) { _interlockDelayMs = ms; }
    uint32_t getInterlockDelayMs() const { return _interlockDelayMs; }
    void setPumpOverrunMs(uint32_t ms) { _pumpOverrunMs = ms; }
    uint32_t getPumpOverrunMs() const { return _pumpOverrunMs; }
    void setFreezeProtectionTemp(float tempC) { _freezeProtectionTemp = tempC; }
    float getFreezeProtectionTemp() const { return _freezeProtectionTemp; }

    // Valve / interlock / overrun queries
    bool valveOpen(size_t index, uint32_t now) const {
        return index < MAX_ZONES && _zones[index].relayEngaged &&
               (uint32_t)(now - _zones[index].energisedMs) >= _interlockDelayMs;
    }
    bool anyValveOpen(uint32_t now) const;
    bool isValveEnergised(size_t index) const {
        return index < MAX_ZONES && _zones[index].relayEngaged;
    }
    bool isAnyValveEnergised() const {
        for (size_t i = 0; i < MAX_ZONES; i++) {
            if (_zones[i].relayEngaged) return true;
        }
        return false;
    }
    uint32_t getValveEnergisedMs(size_t index) const {
        return (index < MAX_ZONES) ? _zones[index].energisedMs : 0;
    }
    bool isOverrunActive() const { return _overrunActive; }
    int8_t getOverrunKeeperZone() const { return _overrunKeeperZone; }

    // FR-2 & FR-1.3: boiler ON permitted when >= 1 valve open for interlockDelay (or MANUAL openLoopBypass)
    bool boilerOnPermitted(uint32_t now, bool autoMode, bool openLoopBypass) const {
        return (!autoMode && openLoopBypass) || anyValveOpen(now);
    }

    // FR-3 & FR-3.2: overrun starts on fall of a lit flame; cancelled start never triggers overrun
    OverrunTransition trackBoilerState(uint32_t now, bool boilerLastCommandOn);

    // FR-37, FR-3, FR-2, FR-16.1: pure valve arbitration across all zones
    void arbitrateValves(uint32_t now,
                         bool boilerLastCommandOn,
                         uint32_t demandMask,
                         uint32_t frostMask,
                         size_t zoneCount,
                         bool& anyReady,
                         bool& anyReadyFrost,
                         ValveAction outActions[]);

    void arbitrateValves(uint32_t now,
                         bool boilerLastCommandOn,
                         const bool demand[],
                         const bool frostActive[],
                         size_t zoneCount,
                         bool& anyReady,
                         bool& anyReadyFrost,
                         ValveAction outActions[]) {
        uint32_t dm = 0, fm = 0;
        size_t n = zoneCount > MAX_ZONES ? MAX_ZONES : zoneCount;
        for (size_t i = 0; i < n; ++i) {
            if (demand[i]) dm |= (1UL << i);
            if (frostActive[i]) fm |= (1UL << i);
        }
        arbitrateValves(now, boilerLastCommandOn, dm, fm, n, anyReady, anyReadyFrost, outActions);
    }

    // FR-16.1: frost evaluation for a single zone (enabled or disabled, any mode)
    FrostEvalResult evaluateFrost(bool wasFrostActive, bool found, float currentTemp) const;

    // Synchronize state when a manual action, config reset, or zone removal alters relays directly
    void syncValveState(size_t index, bool engaged, uint32_t now);
    void setOverrunKeeperZone(int8_t zoneIndex) { _overrunKeeperZone = zoneIndex; }
    void onZoneRemoved(size_t index);

private:
    bool _boilerWasOn;
    bool _overrunActive;
    uint32_t _overrunUntilMs;
    int8_t _overrunKeeperZone;
    bool _holdLogged;

    uint32_t _interlockDelayMs;
    uint32_t _pumpOverrunMs;
    float _freezeProtectionTemp;

    struct ZoneSafetyState {
        uint32_t energisedMs;
        bool relayEngaged;
        bool valveReady;
    };
    ZoneSafetyState _zones[MAX_ZONES];
};
