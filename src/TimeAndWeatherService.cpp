#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"
#include "ParamStore.h"
#include <WiFi.h>
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
      _lastWeatherSuccessMs(0),
      _ntpAnnounced(false),
      _weatherAnnounced(false) {
    _location[0] = '\0';
}

void TimeAndWeatherService::begin() {
    Serial.println(F("[Time] Configuring NTP synchronization (Timezone: Europe/Bucharest)..."));
    // Standard Romania POSIX Timezone with daylight saving auto-transition
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    _ntpConfigured = true;

    // Location lives in ParamStore (FR-9.2); the old "location" / "passport" keys are migrated there
    strlcpy(_location, paramStore.location(), sizeof(_location));
    _countedKey = (int32_t)paramStore.state(StateKey::SeasonDayKey);
}

bool TimeAndWeatherService::setLocation(const char* coords) {
    if (!paramStore.setLocation(coords)) return false;
    strlcpy(_location, paramStore.location(), sizeof(_location));
    return true;
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

int TimeAndWeatherService::getHour() const {
    if (!isTimeSynced()) return 0;
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_hour;
}

int TimeAndWeatherService::getMinute() const {
    if (!isTimeSynced()) return 0;
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_min;
}

int TimeAndWeatherService::getDayOfWeekIndex() const {
    if (!isTimeSynced()) return 1; // default Monday
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti.tm_wday; // 0 = Sunday, 1 = Monday ... 6 = Saturday
}

bool TimeAndWeatherService::isWeekend() const {
    int w = getDayOfWeekIndex();
    return (w == 0 || w == 6);
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

static portMUX_TYPE wxMux = portMUX_INITIALIZER_UNLOCKED;  // worker task -> loop(): short copies only

uint32_t TimeAndWeatherService::getWeatherAgeSec() const {
    if (!_weatherValid || _lastWeatherSuccessMs == 0) return 999999;
    return (millis() - _lastWeatherSuccessMs) / 1000;
}

TimeAndWeatherService::WeatherState TimeAndWeatherService::weatherState() const {
    if (getWeatherAgeSec() > 6UL * 3600UL) return WX_LOST;  // never polled, or older than 6 h (FR-9)
    return _lastPollOk ? WX_OK : WX_HELD;
}

int32_t TimeAndWeatherService::dayKey() const {
    if (!isTimeSynced()) return (int32_t)(millis() / 86400000UL);  // no clock: a day of uptime
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    return (ti.tm_year + 1900) * 400 + ti.tm_yday;
}

bool TimeAndWeatherService::isPrevDayMeanValid() const {
    return _prevDayKey >= 0 && _prevDayKey == dayKey();  // Lost = not fetched today
}

bool TimeAndWeatherService::takeNewDayMean(float& mean) {
    if (!_dayMeanNew) return false;
    _dayMeanNew = false;
    mean = _prevDayMean;
    paramStore.saveState(StateKey::SeasonDayKey, (uint32_t)_countedKey);
    return true;
}

#ifdef SH_DEV_OPEN
void TimeAndWeatherService::devSetPollFailed(uint32_t ageSec) {
    portENTER_CRITICAL(&wxMux);
    _weatherValid = true;
    _lastPollOk = false;
    _lastWeatherSuccessMs = millis() - ageSec * 1000UL;
    portEXIT_CRITICAL(&wxMux);
}
#endif

void TimeAndWeatherService::forceWeatherUpdate() {
    _lastWeatherPollMs = 0;
}

// The blocking Open-Meteo call runs in its own short-lived task on core 0 (architecture 5.1 network worker), so a slow
// server never stalls loop(). The task exists only while it polls: no stack is held between polls.
volatile uint32_t g_wxStackFreeMin = 0;  // FR-10.6: lowest free stack of the fetch task seen so far (0 = no fetch yet)

void TimeAndWeatherService::fetchTask(void* arg) {
    static_cast<TimeAndWeatherService*>(arg)->fetchWeather();
    uint32_t hw = uxTaskGetStackHighWaterMark(nullptr);
    if (g_wxStackFreeMin == 0 || hw < g_wxStackFreeMin) g_wxStackFreeMin = hw;
    static_cast<TimeAndWeatherService*>(arg)->_fetching = false;
    vTaskDelete(nullptr);
}

void TimeAndWeatherService::fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) { _lastPollOk = false; return; }

    char lat[20] = "45.6579";
    char lon[20] = "25.6012";

    if (strlen(_location) > 3) {
        const char* comma = strchr(_location, ',');
        if (comma && comma > _location) {
            size_t l1 = (size_t)(comma - _location);
            if (l1 >= sizeof(lat)) l1 = sizeof(lat) - 1;
            memcpy(lat, _location, l1);
            lat[l1] = '\0';
            const char* p2 = comma + 1;
            while (*p2 == ' ' || *p2 == '\t') p2++;
            strlcpy(lon, p2, sizeof(lon));
            for (int k = (int)strlen(lat) - 1; k >= 0 && (lat[k] == ' ' || lat[k] == '\t'); k--) lat[k] = '\0';
            for (int k = (int)strlen(lon) - 1; k >= 0 && (lon[k] == ' ' || lon[k] == '\t'); k--) lon[k] = '\0';
        }
    }

    WiFiClient client;
    client.setTimeout(4000);
    if (!client.connect("api.open-meteo.com", 80)) {
        _lastPollOk = false;
        return;
    }

    client.printf(
        "GET /v1/forecast?latitude=%s&longitude=%s"
        "&current=temperature_2m,relative_humidity_2m"
        "&daily=temperature_2m_mean&past_days=1&forecast_days=1&timezone=auto HTTP/1.0\r\n"
        "Host: api.open-meteo.com\r\n"
        "Connection: close\r\n\r\n",
        lat, lon);

    char statusLine[64];
    size_t slen = client.readBytesUntil('\n', statusLine, sizeof(statusLine) - 1);
    statusLine[slen] = '\0';
    int httpCode = 0;
    if (strncmp(statusLine, "HTTP/", 5) == 0) {
        const char* sp = strchr(statusLine, ' ');
        if (sp) httpCode = atoi(sp + 1);
    }

    if (httpCode == 200 && client.find("\r\n\r\n")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, client);
        if (!err && doc.containsKey("current")) {
            JsonObject current = doc["current"];
            float t = _outdoorTemp, h = _outdoorHumidity;
            if (current.containsKey("temperature_2m")) {
                t = current["temperature_2m"].as<float>();
            }
            if (current.containsKey("relative_humidity_2m")) {
                h = current["relative_humidity_2m"].as<float>();
            }
            JsonVariant mean = doc["daily"]["temperature_2m_mean"][0];  // past_days=1: the first day is yesterday
            int32_t key = dayKey();  // localtime_r takes a lock: never inside the critical section
            bool synced = isTimeSynced();
            portENTER_CRITICAL(&wxMux);
            _outdoorTemp = t;
            _outdoorHumidity = h;
            _weatherValid = true;
            _lastPollOk = true;
            _lastWeatherSuccessMs = millis();
            if (mean.is<float>()) {
                // a day counts once, only with a valid clock, and not again after a restart (FR-12.2)
                if (synced && key != _prevDayKey && key != _countedKey) { _dayMeanNew = true; _countedKey = key; }
                _prevDayMean = mean.as<float>();
                _prevDayKey = key;
            }
            portEXIT_CRITICAL(&wxMux);
            if (!_weatherAnnounced) {
                _weatherAnnounced = true;
                netConsole.printf("[INIT]   * Outdoor Weather:  Synchronized: %.1f \xC2\xB0""C, %.1f %% RH (Open-Meteo API)\n",
                                  _outdoorTemp, _outdoorHumidity);
            } else {
                Serial.printf("[Weather] Outdoor Weather synced: %.1f C, %.1f %% RH\n",
                              _outdoorTemp, _outdoorHumidity);
            }
        }
    } else {
        _lastPollOk = false;
    }
    client.stop();
}

void TimeAndWeatherService::update() {
    uint32_t now = millis();

    // Announce real-time clock synchronization as soon as NTP succeeds
    if (!_ntpAnnounced && isTimeSynced()) {
        _ntpAnnounced = true;
        time_t nowEpoch = time(nullptr);
        struct tm ti;
        localtime_r(&nowEpoch, &ti);
        char timeBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &ti);
        netConsole.printf("[INIT]   * Real-Time Clock:  Synchronized: %s (Europe/Bucharest via NTP)\n", timeBuf);
    }

    // Poll Open-Meteo once connected to Wi-Fi (15m interval when valid, 5m backoff if pending/failed)
    if (WiFi.status() == WL_CONNECTED) {
        // Allow network and NTP to settle for 5 seconds after boot before first weather query
        if (_lastWeatherPollMs == 0 && now < 5000) {
            return;
        }
        uint32_t interval = _lastPollOk ? _weatherPollIntervalMs : 300000UL;  // 15 min, retry after 5 min (FR-9)
        if (!_fetching && (_lastWeatherPollMs == 0 || (now - _lastWeatherPollMs >= interval)) && ESP.getFreeHeap() > 40000) {
            _lastWeatherPollMs = now;
            _fetching = true;
            if (xTaskCreatePinnedToCore(fetchTask, "wx", 4096, this, 1, nullptr, 0) != pdPASS) _fetching = false;
        }
    }
}

String TimeAndWeatherService::toJson() const {
    String json;
    json.reserve(512);
    json = "{";
    json += "\"timeSynced\":" + String(isTimeSynced() ? "true" : "false") + ",";
    json += "\"time\":\"" + getFormattedTime() + "\",";
    json += "\"date\":\"" + getFormattedDate() + "\",";
    json += "\"year\":" + String(getYear()) + ",";
    json += "\"month\":" + String(getMonth()) + ",";
    json += "\"day\":" + String(getDay()) + ",";
    json += "\"dayOfWeek\":\"" + getDayOfWeek() + "\",";
    json += "\"season\":\"" + getSeason() + "\",";
    json += "\"outdoorTemp\":" + String(_outdoorTemp, 1) + ",";
    json += "\"outdoorHumidity\":" + String(_outdoorHumidity, 1) + ",";
    json += "\"weatherValid\":" + String(isWeatherValid() ? "true" : "false") + ",";
    static const char* const WX_NAMES[] = {"ok", "held", "lost"};
    json += "\"weatherState\":\"" + String(WX_NAMES[weatherState()]) + "\",";
    json += "\"prevDayMean\":" + String(_prevDayMean, 1) + ",";
    json += "\"prevDayMeanValid\":" + String(isPrevDayMeanValid() ? "true" : "false") + ",";
    json += "\"weatherAgeSec\":" + String(getWeatherAgeSec());
    json += "}";
    return json;
}
