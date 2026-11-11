#include "HeatUpModel.h"

#ifdef ARDUINO
#include <Arduino.h>
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "FaultInjector.h"
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
inline int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
}  // namespace

HeatUpModel heatUpModel;

HeatUpModel::HeatUpModel() : _lastUpdateMs(0) {
    for (size_t i = 0; i < 32; i++) {
        _zones[i].rateDegPerHour = 0.5f; // Baseline underfloor screed heat-up rate (0.5 C/h)
        _zones[i].samples = 0;
        _zones[i].learned = false;
        _zones[i].heatingStartMs = 0;
        _zones[i].heatingStartTemp = 0.0f;
        _zones[i].tracking = false;
    }
}

void HeatUpModel::begin(uint32_t now) {
    _lastUpdateMs = now;
}

uint32_t HeatUpModel::computePreHeatLeadMinutes(float outdoorTemp, bool weatherValid) {
    // FR-48: Weather lost or no weather data -> fixed 90 min fallback
    if (!weatherValid) {
        return 90;
    }

    // FR-48 formula: 120 + 9 * (5 - Tout), limited to [45 .. 240] min
    int lead = 120 + (int)((5.0f - outdoorTemp) * 9.0f);
    return (uint32_t)clampi(lead, 45, 240);
}

uint32_t HeatUpModel::getPreHeatLeadMinutes(float outdoorTemp, bool weatherValid) const {
    bool effectiveValid = weatherValid;
#ifdef ARDUINO
    if (faultInjector.isWeatherLost()) effectiveValid = false;
#endif
    return computePreHeatLeadMinutes(outdoorTemp, effectiveValid);
}

float HeatUpModel::getZoneRateDegPerHour(size_t zoneIndex) const {
    if (zoneIndex >= 32) return 0.5f;
    return _zones[zoneIndex].rateDegPerHour;
}

bool HeatUpModel::isZoneLearned(size_t zoneIndex) const {
    if (zoneIndex >= 32) return false;
    return _zones[zoneIndex].learned;
}

uint16_t HeatUpModel::getZoneSamples(size_t zoneIndex) const {
    if (zoneIndex >= 32) return 0;
    return _zones[zoneIndex].samples;
}

void HeatUpModel::evaluateZone(size_t zoneIndex, bool enabled, bool sensorValid,
                                bool heating, float curTemp, uint32_t now) {
    if (zoneIndex >= 32) return;
    ZoneHeatRate& z = _zones[zoneIndex];

    if (!enabled || !sensorValid) {
        z.tracking = false;
        return;
    }

    // When heating starts, begin tracking a heat-up interval
    if (heating && !z.tracking) {
        z.tracking = true;
        z.heatingStartMs = now;
        z.heatingStartTemp = curTemp;
    } else if (!heating && z.tracking) {
        // Heating ended: compute rate if run lasted at least 60 minutes
        uint32_t durationMs = now - z.heatingStartMs;
        if (durationMs >= 60UL * 60UL * 1000UL) {
            float rise = curTemp - z.heatingStartTemp;
            if (rise > 0.2f) { // Meaningful rise
                float hours = (float)durationMs / (3600.0f * 1000.0f);
                float measuredRate = rise / hours;
                // Clamp to physical underfloor bounds [0.1 .. 2.0 C/h]
                measuredRate = clampf(measuredRate, 0.1f, 2.0f);

                // Exponential moving average: 80% prior, 20% new observation
                if (!z.learned) {
                    z.rateDegPerHour = measuredRate;
                    z.learned = true;
                } else {
                    z.rateDegPerHour = 0.8f * z.rateDegPerHour + 0.2f * measuredRate;
                }
                if (z.samples < 65535) z.samples++;
            }
        }
        z.tracking = false;
    }
}

void HeatUpModel::update(uint32_t now) {
    if (now - _lastUpdateMs < 10000) return; // Evaluate every 10 seconds (L4 advisor, <= 2 ms)
    _lastUpdateMs = now;

#ifdef ARDUINO
    size_t count = thermalZones.getZoneCount();
    if (count > 32) count = 32;

    for (size_t i = 0; i < count; i++) {
        const auto& cfg = thermalZones.getConfig(i);
        const auto& st = thermalZones.getStatus(i);
        evaluateZone(i, cfg.enabled, st.sensorValid, st.relayEngaged, st.currentTemp, now);
    }
#endif
}

#ifdef ARDUINO
String getHeatUpStatusJson() {
    float outT = timeAndWeather.getOutdoorTemperature();
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
    uint32_t lead = heatUpModel.getPreHeatLeadMinutes(outT, wValid);

    String out = "{\"leadMinutes\":";
    out += String(lead);
    out += ",\"weatherValid\":";
    out += wValid ? "true" : "false";
    out += ",\"outdoorTemp\":";
    out += String(outT, 1);
    out += ",\"zones\":[";

    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        out += "{\"zone\":";
        out += String(i + 1);
        out += ",\"rateDegPerHour\":";
        out += String(heatUpModel.getZoneRateDegPerHour(i), 2);
        out += ",\"samples\":";
        out += String(heatUpModel.getZoneSamples(i));
        out += ",\"learned\":";
        out += heatUpModel.isZoneLearned(i) ? "true" : "false";
        out += "}";
    }
    out += "]}";
    return out;
}
#endif
