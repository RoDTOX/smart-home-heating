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
    String getDayOfWeek() const;
    String getSeason() const;
    bool isHeatingSeason() const;

    // Outdoor Weather
    float getOutdoorTemperature() const { return _outdoorTemp; }
    float getOutdoorHumidity() const { return _outdoorHumidity; }
    bool isWeatherValid() const { return _weatherValid; }
    uint32_t getWeatherAgeSec() const;
    void forceWeatherUpdate();

    String toJson() const;

private:
    bool _ntpConfigured;
    uint32_t _lastWeatherPollMs;
    uint32_t _weatherPollIntervalMs;

    float _outdoorTemp;
    float _outdoorHumidity;
    bool _weatherValid;
    uint32_t _lastWeatherSuccessMs;

    void fetchWeather();
};

extern TimeAndWeatherService timeAndWeather;

#endif // TIME_AND_WEATHER_SERVICE_H
