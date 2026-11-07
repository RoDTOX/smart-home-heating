#include "PlantMonitor.h"
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "DiagnosticEngine.h"
#include "ParamStore.h"
#include "EventLog.h"

PlantMonitor plantMonitor;

PlantMonitor::PlantMonitor()
    : _boilerNotHeating(false), _misdirectionDetected(false), _boilerDemandingSinceMs(0), _boilerResendAttempted(false), _lastCheckMs(0) {
    memset(_zones, 0, sizeof(_zones));
}

void PlantMonitor::begin() {
    _boilerNotHeating = false;
    _misdirectionDetected = false;
    _boilerDemandingSinceMs = 0;
    _boilerResendAttempted = false;
    _lastCheckMs = millis();
}

void PlantMonitor::update() {
    uint32_t now = millis();
    // Run evaluation every 5 seconds (lightweight observer, <= 2 ms)
    if (now - _lastCheckMs < 5000) return;
    _lastCheckMs = now;

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
        ZoneTracking& tr = _zones[i];

        if (!cfg.enabled || !st.sensorValid) {
            tr.closedSinceMs = 0;
            tr.highDutySinceMs = 0;
            continue;
        }

        float curTemp = st.currentTemp;
        float target = thermalZones.getEffectiveTargetTemp(i);
        bool valveOpen = st.relayEngaged;
        float duty = thermalZones.getDuty(i);

        // --- 1. FR-56: Zone Overheat Check (Valve closed, temp > target + margin, and still rising) ---
        if (!valveOpen && curTemp > (target + overheatMargin)) {
            if (tr.closedSinceMs == 0) {
                tr.closedSinceMs = now;
                tr.closedStartTemp = curTemp;
            } else if (now - tr.closedSinceMs >= overheatTimeMs) {
                // If temperature is still rising
                if (curTemp >= tr.closedStartTemp + 0.1f) {
                    if (tr.overheatDetectedMs == 0) {
                        tr.overheatDetectedMs = now;
                        diagEngine.raiseDtc(Dtc::ERR_ZONE_OVERHEAT, "Zone overheat with closed valve", SEV_ERROR, i + 1);
                    }
                }
            }
        } else {
            tr.closedSinceMs = 0;
            if (tr.overheatDetectedMs != 0 && curTemp <= target) {
                tr.overheatDetectedMs = 0;
                diagEngine.clearDtc(Dtc::ERR_ZONE_OVERHEAT, i + 1);
            }
        }

        // --- 2. FR-57: Zone No Effect Check (Duty >= noEffectDuty, time >= noEffectTime, rise < noEffectRise) ---
        // For underfloor heating, slab thermal inertia requires 3.5h before concluding no effect.
        uint32_t effectiveNoEffectTimeMs = (cfg.heatingType == HEATING_TYPE_FLOOR)
            ? (noEffectTimeMs >= 12600000UL ? noEffectTimeMs : 12600000UL)  // 3.5 hours for floor
            : (noEffectTimeMs >= 2700000UL ? noEffectTimeMs : 2700000UL);   // 45 min for radiators

        if (duty >= noEffectDutyMin) {
            anyDemandActive = true;
            demandingCount++;

            if (tr.highDutySinceMs == 0) {
                tr.highDutySinceMs = now;
                tr.highDutyStartTemp = curTemp;
            } else if (now - tr.highDutySinceMs >= effectiveNoEffectTimeMs) {
                float rise = curTemp - tr.highDutyStartTemp;
                if (rise < noEffectRiseMin) {
                    if (tr.noEffectDetectedMs == 0) {
                        tr.noEffectDetectedMs = now;
                        diagEngine.raiseDtc(Dtc::ERR_ZONE_NO_EFFECT, "Zone heating without effect", SEV_ERROR, i + 1);
                    }
                } else {
                    allDemandingZonesStagnant = false;
                }
            } else {
                allDemandingZonesStagnant = false;
            }
        } else {
            tr.highDutySinceMs = 0;
            if (tr.noEffectDetectedMs != 0) {
                if (curTemp >= tr.highDutyStartTemp + noEffectRiseMin) {
                    tr.noEffectDetectedMs = 0;
                    diagEngine.clearDtc(Dtc::ERR_ZONE_NO_EFFECT, i + 1);
                }
            }
        }
    }

    // --- 3. FR-57: Boiler Not Heating Check ---
    // If all demanding zones show no rise after boiler active
    if (anyDemandActive && demandingCount > 0 && boilerRadio.isLastCommandOn()) {
        if (_boilerDemandingSinceMs == 0) {
            _boilerDemandingSinceMs = now;
            _boilerResendAttempted = false;
        } else {
            uint32_t elapsed = now - _boilerDemandingSinceMs;
            // Check if any demanding zone is underfloor heating
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
            uint32_t dtcWaitMs = hasFloorDemand ? (12600000UL) : (45UL * 60UL * 1000UL); // 3.5h floor vs 45m rad

            if (elapsed >= resendWaitMs && !_boilerResendAttempted) {
                if (allDemandingZonesStagnant) {
                    // Re-send ON once
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
    // At underfloor heating, slab thermal inertia takes 2-3 hours to propagate.
    // Misdirection threshold: zone demanding heat continuously for >= misdirectionMinMs (3.5h for floor, 45 min for radiators)
    // without temperature rise, while an uncommanded closed zone overheats >= 0.5 C above target.
    bool crossMisdirection = false;
    for (size_t d = 0; d < zoneCount; d++) {
        const auto& cfgD = thermalZones.getConfig(d);
        uint32_t misdirectionMinMs = (cfgD.heatingType == HEATING_TYPE_FLOOR)
            ? (noEffectTimeMs >= 12600000UL ? noEffectTimeMs : 12600000UL) // 3.5 hours for floor
            : (noEffectTimeMs >= 2700000UL ? noEffectTimeMs : 2700000UL);  // 45 min for radiators
        if (_zones[d].highDutySinceMs > 0 && (now - _zones[d].highDutySinceMs >= misdirectionMinMs)) {
            const auto& stD = thermalZones.getStatus(d);
            if (stD.currentTemp - _zones[d].highDutyStartTemp < noEffectRiseMin) {
                // Demanding zone is not heating; check if another uncommanded zone is heating
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
}

bool PlantMonitor::isZoneOverheating(size_t zoneIndex) const {
    if (zoneIndex >= 32) return false;
    return _zones[zoneIndex].overheatDetectedMs != 0;
}

bool PlantMonitor::isZoneNoEffect(size_t zoneIndex) const {
    if (zoneIndex >= 32) return false;
    return _zones[zoneIndex].noEffectDetectedMs != 0;
}

String PlantMonitor::getStatusJson() const {
    String out = "{\"zones\":[";
    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        out += "{\"zone\":";
        out += String(i + 1);
        out += ",\"overheating\":";
        out += isZoneOverheating(i) ? "true" : "false";
        out += ",\"noEffect\":";
        out += isZoneNoEffect(i) ? "true" : "false";
        out += "}";
    }
    out += "],\"boilerMonitor\":{\"boilerNotHeating\":";
    out += _boilerNotHeating ? "true" : "false";
    out += "},\"misdirection\":";
    out += _misdirectionDetected ? "true" : "false";
    out += "}";
    return out;
}
