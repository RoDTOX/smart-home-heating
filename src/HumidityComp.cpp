#include "HumidityComp.h"

#ifdef ARDUINO
#include <Arduino.h>
#include "ThermalZoneManager.h"
#include "BleSensorScanner.h"
#endif

namespace {
inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
}  // namespace

HumidityComp humidityComp;

HumidityComp::HumidityComp() : _lastUpdateMs(0) {}

void HumidityComp::begin(uint32_t now) {
    _lastUpdateMs = now;
}

void HumidityComp::update(uint32_t now) {
    _lastUpdateMs = now;
}

float HumidityComp::computeTargetReduction(bool enabled, bool sensorValid,
                                          bool sensorFault, float humidityRh) {
    if (!enabled) return 0.0f;
    if (!sensorValid || sensorFault) return 0.0f; // Fallback: no reading -> no compensation
    if (humidityRh < 60.0f) return 0.0f;          // FR-71: only at relative humidity >= 60%

    // Proportional reduction: 60% -> 0.0 C, 80% -> 0.5 C (clamped to 0.5 C)
    float reduction = (humidityRh - 60.0f) * 0.025f;
    return clampf(reduction, 0.0f, 0.5f);
}

#ifdef ARDUINO
float HumidityComp::getTargetReduction(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return 0.0f;
    if (!thermalZones.isHumidityComp(zoneIndex)) return 0.0f;

    const auto& cfg = thermalZones.getConfig(zoneIndex);
    const auto& st = thermalZones.getStatus(zoneIndex);
    if (!st.sensorValid || st.sensorFault) return 0.0f;

    // Read relative humidity from BLE sensor registry
    const BleSensorData* sens = bleScanner.findSensorByMac(cfg.sensorMac);
    if (!sens) return 0.0f;

    return computeTargetReduction(true, st.sensorValid, st.sensorFault, sens->humidity);
}

String getHumidityStatusJson() {
    String out = "{\"zones\":[";
    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        const auto& cfg = thermalZones.getConfig(i);
        const BleSensorData* sens = bleScanner.findSensorByMac(cfg.sensorMac);
        float rh = sens ? sens->humidity : 0.0f;
        float red = humidityComp.getTargetReduction(i);

        out += "{";
        out += "\"index\":" + String((unsigned)i) + ",";
        out += "\"humidityComp\":" + String(thermalZones.isHumidityComp(i) ? "true" : "false") + ",";
        out += "\"humidity\":" + String(rh, 1) + ",";
        out += "\"targetReduction\":" + String(red, 2);
        out += "}";
    }
    out += "]}";
    return out;
}
#else
float HumidityComp::getTargetReduction(size_t zoneIndex) const {
    (void)zoneIndex;
    return 0.0f;
}
#endif
