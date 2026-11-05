#include "CycleAligner.h"
#include "ParamStore.h"
#include "ThermalZoneManager.h"
#include "ZoneController.h"
#include <time.h>

CycleAligner cycleAligner;

CycleAligner::CycleAligner()
    : _cycleStartEpoch(0), _lastUpdateMs(0) {}

void CycleAligner::begin() {
    _lastUpdateMs = millis();
}

void CycleAligner::update() {
    _lastUpdateMs = millis();
}

uint32_t CycleAligner::getCycleElapsedSec() const {
    time_t now = time(nullptr);
    if (now > 100000) {
        // Aligned to top-of-hour
        return (uint32_t)(now % 3600);
    }
    return (millis() / 1000UL) % 3600UL;
}

uint8_t CycleAligner::getMinLoops() const {
    return (uint8_t)paramStore.get(Param::MinLoops);
}

uint8_t CycleAligner::getActiveCircuits() const {
    uint8_t total = 0;
    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        const auto& st = thermalZones.getStatus(i);
        if (st.relayEngaged || st.demand) {
            total += thermalZones.getCircuits(i);
        }
    }
    return total;
}

bool CycleAligner::isZoneAligned(size_t zoneIndex) const {
    return true; // All TPI zones follow the synchronized top-of-hour cycle (FR-63)
}

bool CycleAligner::isZoneExtended(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return false;
    const auto& st = thermalZones.getStatus(zoneIndex);
    if (!st.demand && !st.relayEngaged) return false;

    uint8_t circuits = thermalZones.getCircuits(zoneIndex);
    uint8_t minL = getMinLoops();
    // If zone circuits alone are below minLoops and total active circuits are limited, it is extended
    return circuits < minL;
}

String CycleAligner::getStatusJson() const {
    String out = "{";
    out += "\"cycleLengthSec\":" + String(getCycleLengthSec()) + ",";
    out += "\"cycleElapsedSec\":" + String(getCycleElapsedSec()) + ",";
    out += "\"minLoops\":" + String((unsigned)getMinLoops()) + ",";
    out += "\"activeCircuits\":" + String((unsigned)getActiveCircuits()) + ",";
    out += "\"zones\":[";

    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        out += "{";
        out += "\"index\":" + String((unsigned)i) + ",";
        out += "\"circuits\":" + String((unsigned)thermalZones.getCircuits(i)) + ",";
        out += "\"aligned\":" + String(isZoneAligned(i) ? "true" : "false") + ",";
        out += "\"extended\":" + String(isZoneExtended(i) ? "true" : "false");
        out += "}";
    }

    out += "]}";
    return out;
}
