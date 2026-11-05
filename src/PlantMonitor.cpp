#include "PlantMonitor.h"
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "DiagnosticEngine.h"
#include "ParamStore.h"
#include "EventLog.h"

PlantMonitor plantMonitor;

PlantMonitor::PlantMonitor()
    : _boilerNotHeating(false), _boilerDemandingSinceMs(0), _boilerResendAttempted(false), _lastCheckMs(0) {
    memset(_zones, 0, sizeof(_zones));
}

void PlantMonitor::begin() {
    _boilerNotHeating = false;
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
        if (duty >= noEffectDutyMin && valveOpen) {
            anyDemandActive = true;
            demandingCount++;

            if (tr.highDutySinceMs == 0) {
                tr.highDutySinceMs = now;
                tr.highDutyStartTemp = curTemp;
            } else if (now - tr.highDutySinceMs >= noEffectTimeMs) {
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
            // Floor heating feedback wait: 30 min initial check
            if (elapsed >= 30UL * 60UL * 1000UL && !_boilerResendAttempted) {
                if (allDemandingZonesStagnant) {
                    // Re-send ON once
                    boilerRadio.sendBoilerOn(false, RS_DEMAND);
                    _boilerResendAttempted = true;
                }
            } else if (elapsed >= 60UL * 60UL * 1000UL && _boilerResendAttempted) {
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
    out += "}}";
    return out;
}
