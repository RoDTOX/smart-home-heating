#ifndef TIME_AND_WEATHER_SERVICE_H
#define TIME_AND_WEATHER_SERVICE_H

#include <Arduino.h>

class TimeAndWeatherService {
public:
    TimeAndWeatherService();

    void begin();
    void update();

    // Time & Date
    bool isTimeSynced() const;
    String getFormattedTime() const;
    String getFormattedDate() const;
    int getYear() const;
    int getMonth() const;
    int getDay() const;
    int getHour() const;
    int getMinute() const;
    int getDayOfWeekIndex() const;
    bool isWeekend() const;
    String getDayOfWeek() const;
    String getSeason() const;

    // Outdoor Weather
    float getOutdoorTemperature() const { return _outdoorTemp; }
    float getOutdoorHumidity() const { return _outdoorHumidity; }
    // Validity (architecture 8.1): Ok = last poll succeeded, Held = last poll failed and the value is 6 h old at most,
    // Lost = older (FR-9). Only an Ok or Held value is used.
    enum WeatherState : uint8_t { WX_OK = 0, WX_HELD, WX_LOST };
    WeatherState weatherState() const;
    bool isWeatherValid() const { return weatherState() != WX_LOST; }
    uint32_t getWeatherAgeSec() const;
    void forceWeatherUpdate();
    // Mean outdoor temperature of the previous day, fetched once per day (FR-12.2)
    bool isPrevDayMeanValid() const;
    float getPrevDayMean() const { return _prevDayMean; }
    bool takeNewDayMean(float& mean);  // true once for each new day's mean
#ifdef SH_DEV_OPEN
    void devSetPollFailed(uint32_t ageSec);  // test hook: last success ageSec ago, the poll after it failed
#endif

    // Location for weather ("lat,lon"), persisted in NVS
    const char* getLocation() const { return _location; }
    bool setLocation(const char* coords);  // false: not "lat,lon" in range (FR-9.2)

    String toJson() const;

private:
    bool _ntpConfigured;
    uint32_t _lastWeatherPollMs;
    uint32_t _weatherPollIntervalMs;

    float _outdoorTemp;
    float _outdoorHumidity;
    bool _weatherValid;       // a poll has succeeded at least once
    bool _lastPollOk = false;
    volatile bool _fetching = false;
    uint32_t _lastWeatherSuccessMs;
    float _prevDayMean = 0;
    int32_t _prevDayKey = -1;    // year and day of the year of the fetch of the mean
    bool _dayMeanNew = false;
    static void fetchTask(void* arg);
    int32_t dayKey() const;
    bool _ntpAnnounced;
    bool _weatherAnnounced;
    char _location[32];

    void fetchWeather();
};

extern TimeAndWeatherService timeAndWeather;

#endif // TIME_AND_WEATHER_SERVICE_H
