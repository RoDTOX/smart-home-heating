#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * FaultInjector (NFR-5.5, L1 test-only component)
 * Allows test-only masking of inputs in RAM without physical manipulation.
 * Refused where it would suppress safety protection (R-A5).
 */
enum class InjectPoint : uint8_t {
    None = 0,
    SensorLost,      // "sensor-lost" <zone>
    WeatherLost,     // "weather-lost"
    TimeLost,        // "time-lost"
    HumidityLost     // "humidity-lost" [zone]
};

struct ActiveInjection {
    InjectPoint point = InjectPoint::None;
    uint8_t zone = 0;
};

class FaultInjector {
public:
    static constexpr size_t MAX_INJECTIONS = 8;

    FaultInjector() = default;

    bool inject(const String& pointStr, uint8_t zone = 0);
    bool clear();

    bool isSensorLost(uint8_t zone) const;
    bool isWeatherLost() const;
    bool isTimeLost() const;
    bool isHumidityLost(uint8_t zone = 0) const;

    size_t count() const { return _count; }
    void getInjectionsJson(JsonArray& arr) const;
    String getInjectionsJsonString() const;

private:
    ActiveInjection _injections[MAX_INJECTIONS];
    size_t _count = 0;
};

extern FaultInjector faultInjector;
