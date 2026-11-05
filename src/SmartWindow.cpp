#include "SmartWindow.h"
#include "ThermalZoneManager.h"
#include "BleSensorScanner.h"
#include "TimeAndWeatherService.h"
#include <ArduinoJson.h>
#include <math.h>

extern ThermalZoneManager thermalZones;
extern BleSensorScanner bleScanner;
extern TimeAndWeatherService timeAndWeather;

SmartWindow smartWindow;

SmartWindow::SmartWindow()
    : _lastUpdateMs(0) {
}

void SmartWindow::begin() {
    _lastUpdateMs = millis();
}

void SmartWindow::update() {
    uint32_t now = millis();
    if (now - _lastUpdateMs < 10000UL) {
        return;
    }
    _lastUpdateMs = now;
}

float SmartWindow::calculateAbsoluteHumidity(float tempC, float humidityRh) {
    if (humidityRh <= 0.0f || isnan(tempC) || isnan(humidityRh)) {
        return 0.0f;
    }
    // Magnus-Tetens formula for saturation vapor pressure P_sat (hPa):
    // P_sat = 6.112 * exp((17.67 * T) / (T + 243.5))
    // Vapor pressure P_v = P_sat * (RH / 100)
    // Absolute humidity AH (g/m3) = (P_v * 100 * 2.1674) / (273.15 + T) / 100
    // => AH = (6.112 * exp((17.67 * tempC) / (tempC + 243.5)) * humidityRh * 2.1674) / (273.15 + tempC)
    float satVapor = 6.112f * expf((17.67f * tempC) / (tempC + 243.5f));
    float ah = (satVapor * (humidityRh / 100.0f) * 216.74f) / (273.15f + tempC);
    return (ah > 0.0f) ? ah : 0.0f;
}

String SmartWindow::getStatusJson() const {
    JsonDocument doc;
    float outdoor = timeAndWeather.getOutdoorTemperature();
    bool wxValid = timeAndWeather.isWeatherValid();
    if (wxValid) {
        doc["outdoorTemp"] = serialized(String(outdoor, 1));
    } else {
        doc["outdoorTemp"] = nullptr;
    }
    doc["fallbackRule"] = "FR-40";

    JsonArray zones = doc["zones"].to<JsonArray>();
    size_t count = thermalZones.getZoneCount();

    for (size_t i = 0; i < count; ++i) {
        JsonObject z = zones.add<JsonObject>();
        z["index"] = i;
        const ThermalZoneConfig& cfg = thermalZones.getConfig(i);
        const ThermalZoneStatus& st = thermalZones.getStatus(i);

        float roomTemp = st.currentTemp;
        z["temp"] = serialized(String(roomTemp, 1));

        const BleSensorData* sens = (cfg.sensorMac[0] != '\0') ? bleScanner.findSensorByMac(cfg.sensorMac) : nullptr;
        float rh = (sens && sens->valid) ? sens->humidity : 0.0f;
        z["humidity"] = serialized(String(rh, 1));

        float ah = calculateAbsoluteHumidity(roomTemp, rh);
        z["absoluteHumidity"] = serialized(String(ah, 1));

        // Temperature slope (°C/min) - currently 0.0 before trace-based model tuning
        z["tempSlope"] = 0.0;

        float deltaOutdoor = wxValid ? (roomTemp - outdoor) : 0.0f;
        z["deltaOutdoor"] = serialized(String(deltaOutdoor, 1));

        // Window open detection state from ThermalZoneManager
        bool isOpen = thermalZones.isWindowDetected(i) || thermalZones.isWindowPauseActive(i);
        z["windowOpen"] = isOpen;

        // dT < 5.0 C suspension rule (FR-40.2)
        bool suspended = (!wxValid) || (deltaOutdoor < 5.0f);
        z["suspended"] = suspended;
    }

    String out;
    serializeJson(doc, out);
    return out;
}
