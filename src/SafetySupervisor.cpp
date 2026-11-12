#include "SafetySupervisor.h"
#include <string.h>

SafetySupervisor::SafetySupervisor() {
    begin();
}

void SafetySupervisor::begin(uint32_t interlockDelayMs, uint32_t pumpOverrunMs, float freezeProtectionTemp) {
    _interlockDelayMs = interlockDelayMs;
    _pumpOverrunMs = pumpOverrunMs;
    _freezeProtectionTemp = freezeProtectionTemp;
    _boilerWasOn = false;
    _overrunActive = false;
    _overrunUntilMs = 0;
    _overrunKeeperZone = -1;
    _holdLogged = false;
    memset(_zones, 0, sizeof(_zones));
}

bool SafetySupervisor::anyValveOpen(uint32_t now) const {
    for (size_t i = 0; i < MAX_ZONES; i++) {
        if (_zones[i].relayEngaged && (uint32_t)(now - _zones[i].energisedMs) >= _interlockDelayMs) return true;
    }
    return false;
}

// FR-3: overrun starts with the OFF of a lit flame (FR-3.2: cancelled start before ON never starts overrun)
SafetySupervisor::OverrunTransition SafetySupervisor::trackBoilerState(uint32_t now, bool boilerLastCommandOn) {
    OverrunTransition tr = OverrunTransition::None;
    if (_boilerWasOn && !boilerLastCommandOn) {
        _overrunActive = true;
        _overrunUntilMs = now + _pumpOverrunMs;
        tr = OverrunTransition::Started;
    } else if (boilerLastCommandOn) {
        _overrunActive = false;
    } else if (_overrunActive && (int32_t)(now - _overrunUntilMs) >= 0) {
        _overrunActive = false;
        tr = OverrunTransition::Finished;
    }
    _boilerWasOn = boilerLastCommandOn;
    return tr;
}

void SafetySupervisor::arbitrateValves(uint32_t now,
                                       bool boilerLastCommandOn,
                                       uint32_t demandMask,
                                       uint32_t frostMask,
                                       size_t zoneCount,
                                       bool& anyReady,
                                       bool& anyReadyFrost,
                                       ValveAction outActions[]) {
    if (zoneCount > MAX_ZONES) zoneCount = MAX_ZONES;
    anyReady = false;
    anyReadyFrost = false;
    memset(outActions, 0, sizeof(ValveAction) * zoneCount);

    for (size_t i = 0; i < zoneCount; i++) {
        uint32_t bit = 1UL << i;
        if ((demandMask & bit) && _zones[i].relayEngaged && (uint32_t)(now - _zones[i].energisedMs) >= _interlockDelayMs) {
            anyReady = true;
            if (frostMask & bit) anyReadyFrost = true;
            _overrunKeeperZone = (int8_t)i;
        }
    }

    bool holdWanted = !anyReady && _overrunKeeperZone >= 0 && (boilerLastCommandOn || _overrunActive);

    for (size_t i = 0; i < zoneCount; i++) {
        ZoneSafetyState& zs = _zones[i];
        if (demandMask & (1UL << i)) {
            if ((int8_t)i == _overrunKeeperZone) _holdLogged = false;
            if (!zs.relayEngaged) {
                zs.relayEngaged = true;
                zs.energisedMs = now;
                outActions[i].flags |= (VA_OPEN_CHANGED | VA_OPEN_TARGET);
            }
        } else if (zs.relayEngaged) {
            if (holdWanted && (int8_t)i == _overrunKeeperZone) { // FR-3
                if (!_holdLogged) {
                    _holdLogged = true;
                    outActions[i].flags |= VA_HOLD_STARTED;
                }
            } else {
                bool wasHeld = _holdLogged && (int8_t)i == _overrunKeeperZone;
                if (wasHeld) _holdLogged = false;
                zs.relayEngaged = false;
                if ((int8_t)i == _overrunKeeperZone) _overrunKeeperZone = -1;
                outActions[i].flags |= VA_OPEN_CHANGED;
                if (wasHeld) outActions[i].flags |= VA_OVERRUN_END;
            }
        }

        // FR-62.1: the interlock of a valve ends when it has been energised for interlockDelay (FR-2)
        if (!zs.relayEngaged) {
            zs.valveReady = false;
        } else if (!zs.valveReady && (uint32_t)(now - zs.energisedMs) >= _interlockDelayMs) {
            zs.valveReady = true;
            outActions[i].flags |= VA_INTERLOCK_DONE;
            outActions[i].interlockElapsedSec = (uint16_t)((now - zs.energisedMs) / 1000UL);
        }
    }
}

// FR-16.1: any zone, enabled or disabled, with accepted reading < freezeProtectionTemp heats until +1.5 C hysteresis
SafetySupervisor::FrostEvalResult SafetySupervisor::evaluateFrost(bool wasFrostActive, bool found, float currentTemp) const {
    FrostEvalResult res;
    res.frostActive = wasFrostActive;

    if (found && currentTemp < _freezeProtectionTemp) {
        res.frostActive = true;
    } else if (wasFrostActive && (!found || currentTemp >= _freezeProtectionTemp + 1.5f)) {
        res.frostActive = false;
    }

    res.becameActive = (!wasFrostActive && res.frostActive);
    res.becameInactive = (wasFrostActive && !res.frostActive);
    return res;
}

void SafetySupervisor::syncValveState(size_t index, bool engaged, uint32_t now) {
    if (index >= MAX_ZONES) return;
    _zones[index].relayEngaged = engaged;
    if (engaged) {
        _zones[index].energisedMs = now;
    } else {
        _zones[index].valveReady = false;
    }
}

void SafetySupervisor::onZoneRemoved(size_t index) {
    if (index >= MAX_ZONES) return;
    if (_overrunKeeperZone == (int8_t)index) {
        _overrunKeeperZone = -1;
        _holdLogged = false;
    } else if (_overrunKeeperZone > (int8_t)index) {
        _overrunKeeperZone--;
    }
    for (size_t i = index; i + 1 < MAX_ZONES; i++) {
        _zones[i] = _zones[i + 1];
    }
    _zones[MAX_ZONES - 1] = {};
}
