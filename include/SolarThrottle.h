#ifndef SOLAR_THROTTLE_H
#define SOLAR_THROTTLE_H

#include <stddef.h>
#include <stdint.h>

// FR-70: SolarThrottle (Layer 4 Advisor)
// Throttles TPI duty for zones flagged with solarGain (e.g. South/West windows)
// when forecast irradiance exceeds solarLimit.
// Fallback: no forecast or weather-lost -> 0% reduction.
class SolarThrottle {
public:
    static SolarThrottle& instance() {
        static SolarThrottle s_inst;
        return s_inst;
    }

    SolarThrottle();

    void begin(uint32_t now = 0);
    void update(uint32_t now);

    // Pure calculation helpers (host-testable without hardware/NVS)
    static bool computeThrottlingActive(bool solarGain, bool weatherValid,
                                        float forecastIrradiance, float solarLimit);
    static float computeThrottleFactor(bool solarGain, bool weatherValid,
                                       float forecastIrradiance, float solarLimit,
                                       float solarReductionPct);

    // Queries
    float getForecastIrradiance() const { return _forecastIrradiance; }
    void setForecastIrradiance(float w_m2) { _forecastIrradiance = w_m2; }
    bool isThrottlingActive(size_t zoneIndex) const;
    float getThrottleFactor(size_t zoneIndex) const; // 1.0 = normal, 0.7 = 30% reduction

private:
    float _forecastIrradiance; // W/m2 direct/global solar radiation
    uint32_t _lastUpdateMs;
};

extern SolarThrottle solarThrottle;

#endif // SOLAR_THROTTLE_H
