#include "SolarThrottle.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "FaultInjector.h"
#include "ParamStore.h"

SolarThrottle solarThrottle;

SolarThrottle::SolarThrottle()
    : _forecastIrradiance(0.0f), _lastUpdateMs(0) {}

void SolarThrottle::begin() {
    _lastUpdateMs = millis();
}

bool SolarThrottle::isThrottlingActive(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return false;
    if (!thermalZones.isSolarGain(zoneIndex)) return false;

    // Fallback: weather lost -> no reduction (NFR-5.1)
    if (!timeAndWeather.isWeatherValid() || faultInjector.isWeatherLost()) {
        return false;
    }

    float limit = (float)paramStore.get(Param::SolarLimit);
    return _forecastIrradiance >= limit;
}

float SolarThrottle::getThrottleFactor(size_t zoneIndex) const {
    if (!isThrottlingActive(zoneIndex)) {
        return 1.0f; // No reduction
    }

    float reductionPct = (float)paramStore.get(Param::SolarReduction); // e.g. 30%
    float factor = 1.0f - (reductionPct / 100.0f);
    if (factor < 0.1f) factor = 0.1f;
    return factor;
}

void SolarThrottle::update() {
    uint32_t now = millis();
    if (now - _lastUpdateMs < 10000) return; // Evaluate every 10 seconds (<= 2 ms)
    _lastUpdateMs = now;
}

String SolarThrottle::getStatusJson() const {
    float limit = (float)paramStore.get(Param::SolarLimit);
    float reduction = (float)paramStore.get(Param::SolarReduction);
    bool wValid = timeAndWeather.isWeatherValid() && !faultInjector.isWeatherLost();

    String out = "{\"forecastIrradiance\":";
    out += String(_forecastIrradiance, 1);
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
        bool throttled = isThrottlingActive(i);
        out += "{\"zone\":";
        out += String(i + 1);
        out += ",\"solarGain\":";
        out += thermalZones.isSolarGain(i) ? "true" : "false";
        out += ",\"throttled\":";
        out += throttled ? "true" : "false";
        out += ",\"factor\":";
        out += String(getThrottleFactor(i), 2);
        out += "}";
    }
    out += "]}";
    return out;
}
