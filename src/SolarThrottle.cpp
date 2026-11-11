#include "SolarThrottle.h"

#ifdef ARDUINO
#include <Arduino.h>
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "FaultInjector.h"
#include "ParamStore.h"
#endif

SolarThrottle solarThrottle;

SolarThrottle::SolarThrottle()
    : _forecastIrradiance(0.0f), _lastUpdateMs(0) {}

void SolarThrottle::begin(uint32_t now) {
    _lastUpdateMs = now;
}

bool SolarThrottle::computeThrottlingActive(bool solarGain, bool weatherValid,
                                            float forecastIrradiance, float solarLimit) {
    if (!solarGain) return false;
    // Fallback: weather lost -> no reduction (NFR-5.1)
    if (!weatherValid) return false;
    return forecastIrradiance >= solarLimit;
}

float SolarThrottle::computeThrottleFactor(bool solarGain, bool weatherValid,
                                           float forecastIrradiance, float solarLimit,
                                           float solarReductionPct) {
    if (!computeThrottlingActive(solarGain, weatherValid, forecastIrradiance, solarLimit)) {
        return 1.0f; // No reduction
    }

    float factor = 1.0f - (solarReductionPct / 100.0f);
    if (factor < 0.1f) factor = 0.1f;
    return factor;
}

void SolarThrottle::update(uint32_t now) {
    if (now - _lastUpdateMs < 10000) return; // Evaluate every 10 seconds (<= 2 ms)
    _lastUpdateMs = now;
}

#ifdef ARDUINO
bool SolarThrottle::isThrottlingActive(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return false;
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
    float limit = (float)paramStore.get(Param::SolarLimit);
    return computeThrottlingActive(thermalZones.isSolarGain(zoneIndex), wValid, _forecastIrradiance, limit);
}

float SolarThrottle::getThrottleFactor(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return 1.0f;
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();
    float limit = (float)paramStore.get(Param::SolarLimit);
    float reductionPct = (float)paramStore.get(Param::SolarReduction); // e.g. 30%
    return computeThrottleFactor(thermalZones.isSolarGain(zoneIndex), wValid, _forecastIrradiance, limit, reductionPct);
}

String getSolarStatusJson() {
    float limit = (float)paramStore.get(Param::SolarLimit);
    float reduction = (float)paramStore.get(Param::SolarReduction);
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();

    String out = "{\"forecastIrradiance\":";
    out += String(solarThrottle.getForecastIrradiance(), 1);
    out += ",\"solarLimit\":";
    out += String(limit, 0);
    out += ",\"solarReduction\":";
    out += String(reduction, 0);
    out += ",\"weatherValid\":";
    out += wValid ? "true" : "false";
    out += ",\"zones\":[";

    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        bool throttled = solarThrottle.isThrottlingActive(i);
        out += "{\"zone\":";
        out += String(i + 1);
        out += ",\"solarGain\":";
        out += thermalZones.isSolarGain(i) ? "true" : "false";
        out += ",\"throttled\":";
        out += throttled ? "true" : "false";
        out += ",\"factor\":";
        out += String(solarThrottle.getThrottleFactor(i), 2);
        out += "}";
    }
    out += "]}";
    return out;
}
#else
bool SolarThrottle::isThrottlingActive(size_t zoneIndex) const {
    (void)zoneIndex;
    return false;
}

float SolarThrottle::getThrottleFactor(size_t zoneIndex) const {
    (void)zoneIndex;
    return 1.0f;
}
#endif
