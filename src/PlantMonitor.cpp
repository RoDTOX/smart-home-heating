#include "PlantMonitor.h"
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "DiagnosticEngine.h"
#include "ParamStore.h"
#include "EventLog.h"
#endif

PlantMonitor plantMonitor;

PlantMonitor::PlantMonitor()
    : _boilerNotHeating(false), _misdirectionDetected(false), _boilerDemandingSinceMs(0), _boilerResendAttempted(false), _lastCheckMs(0) {
    memset(_zones, 0, sizeof(_zones));
}

void PlantMonitor::begin(uint32_t now) {
    _boilerNotHeating = false;
    _misdirectionDetected = false;
    _boilerDemandingSinceMs = 0;
    _boilerResendAttempted = false;
    _lastCheckMs = now;
}

uint32_t PlantMonitor::effectiveNoEffectTimeMs(bool isFloor, uint32_t configuredNoEffectTimeMs) {
    // For underfloor heating, slab thermal inertia requires 3.5h before concluding no effect.
    return isFloor
        ? (configuredNoEffectTimeMs >= 12600000UL ? configuredNoEffectTimeMs : 12600000UL) // 3.5 hours for floor
        : (configuredNoEffectTimeMs >= 2700000UL ? configuredNoEffectTimeMs : 2700000UL);  // 45 min for radiators
}

PlantMonitor::ZoneEvalResult PlantMonitor::evaluateZone(
    size_t zoneIndex, uint32_t now, bool enabled, bool sensorValid,
    float curTemp, float target, bool valveOpen, float duty,
    bool isFloor, float overheatMargin, uint32_t overheatTimeMs,
    float noEffectDutyMin, uint32_t noEffectTimeMs, float noEffectRiseMin) {
    ZoneEvalResult res;
    if (zoneIndex >= 32) return res;

    ZoneTracking& tr = _zones[zoneIndex];
    if (!enabled || !sensorValid) {
        tr.closedSinceMs = 0;
        tr.highDutySinceMs = 0;
        return res;
    }

    // --- 1. FR-56: Zone Overheat Check (Valve closed, temp > target + margin, and still rising) ---
    if (!valveOpen && curTemp > (target + overheatMargin)) {
        if (tr.closedSinceMs == 0) {
            tr.closedSinceMs = now;
            tr.closedStartTemp = curTemp;
        } else if (now - tr.closedSinceMs >= overheatTimeMs) {
            if (curTemp >= tr.closedStartTemp + 0.1f) {
                if (tr.overheatDetectedMs == 0) {
                    tr.overheatDetectedMs = now;
                    res.raiseOverheat = true;
                }
            }
        }
    } else {
        tr.closedSinceMs = 0;
        if (tr.overheatDetectedMs != 0 && curTemp <= target) {
            tr.overheatDetectedMs = 0;
            res.clearOverheat = true;
        }
    }

    // --- 2. FR-57: Zone No Effect Check (Duty >= noEffectDuty, time >= effectiveNoEffectTime, rise < noEffectRise) ---
    uint32_t effTimeMs = effectiveNoEffectTimeMs(isFloor, noEffectTimeMs);
    if (duty >= noEffectDutyMin) {
        res.demandActive = true;

        if (tr.highDutySinceMs == 0) {
            tr.highDutySinceMs = now;
            tr.highDutyStartTemp = curTemp;
            res.zoneStagnant = false;
        } else if (now - tr.highDutySinceMs >= effTimeMs) {
            float rise = curTemp - tr.highDutyStartTemp;
            if (rise < noEffectRiseMin) {
                if (tr.noEffectDetectedMs == 0) {
                    tr.noEffectDetectedMs = now;
                    res.raiseNoEffect = true;
                }
                res.zoneStagnant = true;
            } else {
                res.zoneStagnant = false;
            }
        } else {
            res.zoneStagnant = false;
        }
    } else {
        tr.highDutySinceMs = 0;
        if (tr.noEffectDetectedMs != 0) {
            if (curTemp >= tr.highDutyStartTemp + noEffectRiseMin) {
                tr.noEffectDetectedMs = 0;
                res.clearNoEffect = true;
            }
        }
    }

    return res;
}

void PlantMonitor::update(uint32_t now) {
    // Run evaluation every 5 seconds (lightweight observer, <= 2 ms)
    if (now - _lastCheckMs < 5000) return;
    _lastCheckMs = now;

#ifdef ARDUINO
    size_t zoneCount = thermalZones.getZoneCount();
    if (zoneCount > 32) zoneCount = 32;

    float overheatMargin = paramStore.getf(Param::OverheatMargin);
    uint32_t overheatTimeMs = (uint32_t)paramStore.get(Param::OverheatTime) * 60UL * 1000UL;

    float noEffectDutyMin = (float)paramStore.get(Param::NoEffectDuty) / 100.0f;
    uint32_t noEffectTimeMs = (uint32_t)paramStore.get(Param::NoEffectTime) * 3600UL * 1000UL;
    float noEffectRiseMin = paramStore.getf(Param::NoEffectRise);

    bool anyDemandActive = false;
    bool allDemandingZonesStagnant = true;
    size_t demandingCount = 0;

    for (size_t i = 0; i < zoneCount; i++) {
        const auto& cfg = thermalZones.getConfig(i);
        const auto& st = thermalZones.getStatus(i);

        ZoneEvalResult ev = evaluateZone(
            i, now, cfg.enabled, st.sensorValid,
            st.currentTemp, thermalZones.getEffectiveTargetTemp(i), st.relayEngaged, thermalZones.getDuty(i),
            cfg.heatingType == HEATING_TYPE_FLOOR, overheatMargin, overheatTimeMs,
            noEffectDutyMin, noEffectTimeMs, noEffectRiseMin);

        if (ev.raiseOverheat) diagEngine.raiseDtc(Dtc::ERR_ZONE_OVERHEAT, "Zone overheat with closed valve", SEV_ERROR, i + 1);
        if (ev.clearOverheat) diagEngine.clearDtc(Dtc::ERR_ZONE_OVERHEAT, i + 1);
        if (ev.raiseNoEffect) diagEngine.raiseDtc(Dtc::ERR_ZONE_NO_EFFECT, "Zone heating without effect", SEV_ERROR, i + 1);
        if (ev.clearNoEffect) diagEngine.clearDtc(Dtc::ERR_ZONE_NO_EFFECT, i + 1);
        if (ev.demandActive) {
            anyDemandActive = true;
            demandingCount++;
            if (!ev.zoneStagnant) allDemandingZonesStagnant = false;
        }
    }

    // --- 3. FR-57: Boiler Not Heating Check ---
    if (anyDemandActive && demandingCount > 0 && boilerRadio.isLastCommandOn()) {
        if (_boilerDemandingSinceMs == 0) {
            _boilerDemandingSinceMs = now;
            _boilerResendAttempted = false;
        } else {
            uint32_t elapsed = now - _boilerDemandingSinceMs;
            bool hasFloorDemand = false;
            for (size_t z = 0; z < zoneCount; z++) {
                if (thermalZones.getConfig(z).enabled && thermalZones.getDuty(z) >= noEffectDutyMin) {
                    if (thermalZones.getConfig(z).heatingType == HEATING_TYPE_FLOOR) {
                        hasFloorDemand = true;
                        break;
                    }
                }
            }
            uint32_t resendWaitMs = hasFloorDemand ? (3UL * 3600UL * 1000UL) : (20UL * 60UL * 1000UL);
            uint32_t dtcWaitMs = hasFloorDemand ? (12600000UL) : (45UL * 60UL * 1000UL);

            if (elapsed >= resendWaitMs && !_boilerResendAttempted) {
                if (allDemandingZonesStagnant) {
                    boilerRadio.sendBoilerOn(false, RS_DEMAND);
                    _boilerResendAttempted = true;
                }
            } else if (elapsed >= dtcWaitMs && _boilerResendAttempted) {
                if (allDemandingZonesStagnant && !_boilerNotHeating) {
                    _boilerNotHeating = true;
                    diagEngine.raiseDtc(Dtc::ERR_BOILER_NOT_HEATING, "Boiler not heating under demand", SEV_CRITICAL, 0);
                }
            }
        }
    } else {
        _boilerDemandingSinceMs = 0;
        _boilerResendAttempted = false;
        if (_boilerNotHeating) {
            _boilerNotHeating = false;
            diagEngine.clearDtc(Dtc::ERR_BOILER_NOT_HEATING, 0);
        }
    }

    // --- 4. FR-56 / FR-57 (PB-125): Cross-Zone Heat Misdirection Check ---
    bool crossMisdirection = false;
    for (size_t d = 0; d < zoneCount; d++) {
        const auto& cfgD = thermalZones.getConfig(d);
        uint32_t misdirectionMinMs = effectiveNoEffectTimeMs(cfgD.heatingType == HEATING_TYPE_FLOOR, noEffectTimeMs);
        if (_zones[d].highDutySinceMs > 0 && (now - _zones[d].highDutySinceMs >= misdirectionMinMs)) {
            const auto& stD = thermalZones.getStatus(d);
            if (stD.currentTemp - _zones[d].highDutyStartTemp < noEffectRiseMin) {
                for (size_t u = 0; u < zoneCount; u++) {
                    if (u == d) continue;
                    const auto& cfgU = thermalZones.getConfig(u);
                    const auto& stU = thermalZones.getStatus(u);
                    if (cfgU.enabled && stU.sensorValid && !stU.relayEngaged) {
                        float targetU = thermalZones.getEffectiveTargetTemp(u);
                        if (stU.currentTemp >= targetU + 0.5f) {
                            crossMisdirection = true;
                            break;
                        }
                    }
                }
            }
        }
        if (crossMisdirection) break;
    }

    if (crossMisdirection) {
        if (!_misdirectionDetected) {
            _misdirectionDetected = true;
            diagEngine.raiseDtc(Dtc::ERR_ZONE_OVERHEAT, "Cross-zone misdirection / inverted circuit", SEV_ERROR, 0);
        }
    } else {
        if (_misdirectionDetected) {
            _misdirectionDetected = false;
            diagEngine.clearDtc(Dtc::ERR_ZONE_OVERHEAT, 0);
        }
    }
#endif
}

bool PlantMonitor::isZoneOverheating(size_t zoneIndex) const {
    if (zoneIndex >= 32) return false;
    return _zones[zoneIndex].overheatDetectedMs != 0;
}

bool PlantMonitor::isZoneNoEffect(size_t zoneIndex) const {
    if (zoneIndex >= 32) return false;
    return _zones[zoneIndex].noEffectDetectedMs != 0;
}

#ifdef ARDUINO
String getPlantStatusJson() {
    String out = "{\"zones\":[";
    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        out += "{\"zone\":";
        out += String(i + 1);
        out += ",\"overheating\":";
        out += plantMonitor.isZoneOverheating(i) ? "true" : "false";
        out += ",\"noEffect\":";
        out += plantMonitor.isZoneNoEffect(i) ? "true" : "false";
        out += "}";
    }
    out += "],\"boilerMonitor\":{\"boilerNotHeating\":";
    out += plantMonitor.isBoilerNotHeating() ? "true" : "false";
    out += "},\"misdirection\":";
    out += plantMonitor.isMisdirectionDetected() ? "true" : "false";
    out += "}";
    return out;
}
#endif
