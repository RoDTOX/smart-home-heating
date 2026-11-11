#include "CycleAligner.h"

#ifdef ARDUINO
#include <Arduino.h>
#include "ParamStore.h"
#include "ThermalZoneManager.h"
#include "ZoneController.h"
#include <time.h>
#endif

CycleAligner cycleAligner;

CycleAligner::CycleAligner()
    : _cycleStartEpoch(0), _lastUpdateMs(0) {}

void CycleAligner::begin(uint32_t now) {
    _lastUpdateMs = now;
}

void CycleAligner::update(uint32_t now) {
    _lastUpdateMs = now;
}

uint32_t CycleAligner::computeCycleElapsedSec(uint32_t nowMs, int64_t epochNow) {
    if (epochNow > 100000) {
        // Aligned to top-of-hour
        return (uint32_t)(epochNow % 3600);
    }
    return (nowMs / 1000UL) % 3600UL;
}

bool CycleAligner::computeZoneExtended(bool activeDemandOrRelay, uint8_t circuits, uint8_t minLoops) {
    if (!activeDemandOrRelay) return false;
    return circuits < minLoops;
}

bool CycleAligner::isZoneAligned(size_t zoneIndex) const {
    (void)zoneIndex;
    return true; // All TPI zones follow the synchronized top-of-hour cycle (FR-63)
}

#ifdef ARDUINO
uint32_t CycleAligner::getCycleElapsedSec(uint32_t nowMs) const {
    time_t epochNow = time(nullptr);
    return computeCycleElapsedSec(nowMs != 0 ? nowMs : _lastUpdateMs, (int64_t)epochNow);
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

bool CycleAligner::isZoneExtended(size_t zoneIndex) const {
    if (zoneIndex >= thermalZones.getZoneCount()) return false;
    const auto& st = thermalZones.getStatus(zoneIndex);
    return computeZoneExtended(st.demand || st.relayEngaged,
                               thermalZones.getCircuits(zoneIndex),
                               getMinLoops());
}

String getCycleAlignerStatusJson(uint32_t nowMs) {
    String out = "{";
    out += "\"cycleLengthSec\":" + String(cycleAligner.getCycleLengthSec()) + ",";
    out += "\"cycleElapsedSec\":" + String(cycleAligner.getCycleElapsedSec(nowMs)) + ",";
    out += "\"minLoops\":" + String((unsigned)cycleAligner.getMinLoops()) + ",";
    out += "\"activeCircuits\":" + String((unsigned)cycleAligner.getActiveCircuits()) + ",";
    out += "\"zones\":[";

    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        if (i > 0) out += ",";
        out += "{";
        out += "\"index\":" + String((unsigned)i) + ",";
        out += "\"circuits\":" + String((unsigned)thermalZones.getCircuits(i)) + ",";
        out += "\"aligned\":" + String(cycleAligner.isZoneAligned(i) ? "true" : "false") + ",";
        out += "\"extended\":" + String(cycleAligner.isZoneExtended(i) ? "true" : "false");
        out += "}";
    }

    out += "]}";
    return out;
}
#else
uint32_t CycleAligner::getCycleElapsedSec(uint32_t nowMs) const {
    return computeCycleElapsedSec(nowMs != 0 ? nowMs : _lastUpdateMs, 0);
}

uint8_t CycleAligner::getMinLoops() const { return 3; }
uint8_t CycleAligner::getActiveCircuits() const { return 0; }
bool CycleAligner::isZoneExtended(size_t zoneIndex) const {
    (void)zoneIndex;
    return false;
}
#endif
