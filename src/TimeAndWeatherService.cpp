#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"
#include "DevicePassport.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

TimeAndWeatherService timeAndWeather;

TimeAndWeatherService::TimeAndWeatherService()
    : _ntpConfigured(false),
      _lastWeatherPollMs(0),
      _weatherPollIntervalMs(900000UL), // 15 minutes
      _outdoorTemp(-999.0f),
      _outdoorHumidity(-999.0f),
      _weatherValid(false),
      _lastWeatherSuccessMs(0) {}

void TimeAndWeatherService::begin() {
    netConsole.println(F("[Time] Configuring NTP synchronization (Timezone: Europe/Bucharest)..."));
    // Standard Romania POSIX Timezone with daylight saving auto-transition
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    _ntpConfigured = true;
}

bool TimeAndWeatherService::isTimeSynced() const {
    time_t now = time(nullptr);
    return (now > 1700000000); // Year 2023+
}

String TimeAndWeatherService::getFormattedTime() const {
    if (!isTimeSynced()) return "00:00:00";
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    return String(buf);
}

String TimeAndWeatherService::getFormattedDate() const {
    if (!isTimeSynced()) return "1970-01-01";
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    return String(buf);
}

int TimeAndWeatherService::getYear() const {
    if (!isTimeSynced()) return 1970;
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_year + 1900;
}

int TimeAndWeatherService::getMonth() const {
    if (!isTimeSynced()) return 1;
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_mon + 1;
}

int TimeAndWeatherService::getDay() const {
    if (!isTimeSynced()) return 1;
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_mday;
}

String TimeAndWeatherService::getDayOfWeek() const {
    if (!isTimeSynced()) return "Unknown";
    static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return String(days[ti.tm_wday]);
}

String TimeAndWeatherService::getSeason() const {
    int m = getMonth();
    if (m >= 3 && m <= 5) return "Spring";
    if (m >= 6 && m <= 8) return "Summer";
    if (m >= 9 && m <= 11) return "Autumn";
    return "Winter";
}

bool TimeAndWeatherService::isHeatingSeason() const {
    int m = getMonth();
    int d = getDay();
    // In Romania, standard heating season is typically Oct 15 - Apr 15
    if (m == 10) return (d >= 15);
    if (m == 11 || m == 12 || m == 1 || m == 2 || m == 3) return true;
    if (m == 4) return (d <= 15);
    return false;
}

uint32_t TimeAndWeatherService::getWeatherAgeSec() const {
    if (!_weatherValid || _lastWeatherSuccessMs == 0) return 999999;
    return (millis() - _lastWeatherSuccessMs) / 1000;
}

void TimeAndWeatherService::forceWeatherUpdate() {
    _lastWeatherPollMs = 0;
}

void TimeAndWeatherService::fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) return;

    String lat = "45.6579";
    String lon = "25.6012";

    const char* coords = devicePassport.getLocationCoords();
    if (coords && strlen(coords) > 3) {
        String s = String(coords);
        int comma = s.indexOf(',');
        if (comma > 0) {
            lat = s.substring(0, comma);
            lon = s.substring(comma + 1);
            lat.trim();
            lon.trim();
        }
    }

    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + lat +
                 "&longitude=" + lon +
                 "&current=temperature_2m,relative_humidity_2m";

    HTTPClient http;
    http.setTimeout(1500);
    if (!http.begin(url)) {
        return;
    }

    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            if (doc.containsKey("current")) {
                JsonObject current = doc["current"];
                if (current.containsKey("temperature_2m")) {
                    _outdoorTemp = current["temperature_2m"].as<float>();
                }
                if (current.containsKey("relative_humidity_2m")) {
                    _outdoorHumidity = current["relative_humidity_2m"].as<float>();
                }
                _weatherValid = true;
                _lastWeatherSuccessMs = millis();
                netConsole.printf("[Weather] Outdoor Open-Meteo: %.1f C | %.1f %% Hum (Coords: %s,%s)\n",
                                  _outdoorTemp, _outdoorHumidity, lat.c_str(), lon.c_str());
            }
        }
    } else {
        netConsole.printf("[Weather] Warning: HTTP %d while fetching Open-Meteo\n", httpCode);
    }
    http.end();
}

void TimeAndWeatherService::update() {
    uint32_t now = millis();

    // Initial weather fetch once connected to Wi-Fi
    if (WiFi.status() == WL_CONNECTED && (!_weatherValid || now - _lastWeatherPollMs >= _weatherPollIntervalMs)) {
        _lastWeatherPollMs = now;
        fetchWeather();
    }
}

String TimeAndWeatherService::toJson() const {
    String json = "{";
    json += "\"timeSynced\":" + String(isTimeSynced() ? "true" : "false") + ",";
    json += "\"time\":\"" + getFormattedTime() + "\",";
    json += "\"date\":\"" + getFormattedDate() + "\",";
    json += "\"year\":" + String(getYear()) + ",";
    json += "\"month\":" + String(getMonth()) + ",";
    json += "\"day\":" + String(getDay()) + ",";
    json += "\"dayOfWeek\":\"" + getDayOfWeek() + "\",";
    json += "\"season\":\"" + getSeason() + "\",";
    json += "\"isHeatingSeason\":" + String(isHeatingSeason() ? "true" : "false") + ",";
    json += "\"outdoorTemp\":" + String(_outdoorTemp, 1) + ",";
    json += "\"outdoorHumidity\":" + String(_outdoorHumidity, 1) + ",";
    json += "\"weatherValid\":" + String(_weatherValid ? "true" : "false") + ",";
    json += "\"weatherAgeSec\":" + String(getWeatherAgeSec());
    json += "}";
    return json;
}
