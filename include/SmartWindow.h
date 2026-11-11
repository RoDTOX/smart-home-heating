#ifndef SMART_WINDOW_H
#define SMART_WINDOW_H

#include <stddef.h>
#include <stdint.h>

// FR-40.2: Smart Window Detection Advisor (Layer 4)
// Evaluates temperature slope, absolute humidity (via Magnus formula),
// and room-to-outdoor temperature difference (dT).
// When dT < 5.0 °C, detection is suspended.
// When outdoor/humidity data is missing or before trace tuning (Risk R-5),
// strictly falls back to the baseline simple drop rule (FR-40).
class SmartWindow {
public:
    static SmartWindow& instance() {
        static SmartWindow s_inst;
        return s_inst;
    }

    SmartWindow();

    void begin(uint32_t now = 0);
    void update(uint32_t now);

    // Magnus formula calculation of absolute humidity (g/m3) from T (deg C) and RH (%)
    // AH = (6.112 * exp((17.67 * T) / (T + 243.5)) * RH * 2.1674) / (273.15 + T)
    static float calculateAbsoluteHumidity(float tempC, float humidityRh);

    // FR-40.2: detection is suspended when weather is missing or (room - outdoor) < 5.0 C
    static bool isDetectionSuspended(bool weatherValid, float roomTempC, float outdoorTempC);

private:
    uint32_t _lastUpdateMs;
};

extern SmartWindow smartWindow;

#endif // SMART_WINDOW_H
