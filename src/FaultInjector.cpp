#include "FaultInjector.h"

FaultInjector faultInjector;

bool FaultInjector::inject(const String& pointStr, uint8_t zone) {
    InjectPoint pt = InjectPoint::None;
    if (pointStr.equalsIgnoreCase("sensor-lost") || pointStr.equalsIgnoreCase("sensor_lost")) {
        pt = InjectPoint::SensorLost;
    } else if (pointStr.equalsIgnoreCase("weather-lost") || pointStr.equalsIgnoreCase("weather_lost")) {
        pt = InjectPoint::WeatherLost;
    } else if (pointStr.equalsIgnoreCase("time-lost") || pointStr.equalsIgnoreCase("time_lost")) {
        pt = InjectPoint::TimeLost;
    } else if (pointStr.equalsIgnoreCase("humidity-lost") || pointStr.equalsIgnoreCase("humidity_lost")) {
        pt = InjectPoint::HumidityLost;
    } else {
        return false;
    }

    // Check if already injected
    for (size_t i = 0; i < _count; i++) {
        if (_injections[i].point == pt && _injections[i].zone == zone) {
            return true;
        }
    }

    if (_count < MAX_INJECTIONS) {
        _injections[_count].point = pt;
        _injections[_count].zone = zone;
        _count++;
        return true;
    }
    return false;
}

bool FaultInjector::clear() {
    _count = 0;
    for (size_t i = 0; i < MAX_INJECTIONS; i++) {
        _injections[i].point = InjectPoint::None;
        _injections[i].zone = 0;
    }
    return true;
}

bool FaultInjector::isSensorLost(uint8_t zone) const {
    for (size_t i = 0; i < _count; i++) {
        if (_injections[i].point == InjectPoint::SensorLost && (_injections[i].zone == zone || _injections[i].zone == 0)) {
            return true;
        }
    }
    return false;
}

bool FaultInjector::isWeatherLost() const {
    for (size_t i = 0; i < _count; i++) {
        if (_injections[i].point == InjectPoint::WeatherLost) {
            return true;
        }
    }
    return false;
}

bool FaultInjector::isTimeLost() const {
    for (size_t i = 0; i < _count; i++) {
        if (_injections[i].point == InjectPoint::TimeLost) {
            return true;
        }
    }
    return false;
}

bool FaultInjector::isHumidityLost(uint8_t zone) const {
    for (size_t i = 0; i < _count; i++) {
        if (_injections[i].point == InjectPoint::HumidityLost && (_injections[i].zone == zone || _injections[i].zone == 0)) {
            return true;
        }
    }
    return false;
}

void FaultInjector::getInjectionsJson(JsonArray& arr) const {
    for (size_t i = 0; i < _count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        switch (_injections[i].point) {
            case InjectPoint::SensorLost:
                obj["point"] = "sensor-lost";
                obj["zone"] = _injections[i].zone;
                break;
            case InjectPoint::WeatherLost:
                obj["point"] = "weather-lost";
                break;
            case InjectPoint::TimeLost:
                obj["point"] = "time-lost";
                break;
            case InjectPoint::HumidityLost:
                obj["point"] = "humidity-lost";
                if (_injections[i].zone > 0) obj["zone"] = _injections[i].zone;
                break;
            default:
                break;
        }
    }
}

String FaultInjector::getInjectionsJsonString() const {
    JsonDocument doc;
    JsonArray arr = doc["injections"].to<JsonArray>();
    getInjectionsJson(arr);
    String out;
    serializeJson(doc, out);
    return out;
}
