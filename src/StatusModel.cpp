#include "StatusModel.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "PlantMonitor.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>

StatusModel statusModel;

StatusModel::StatusModel() : _len(0) {
    _buf[0] = '\0';
}

void StatusModel::reset() {
    _len = 0;
    _buf[0] = '\0';
}

void StatusModel::appendStr(const char* s) {
    if (!s) return;
    size_t slen = strlen(s);
    if (_len + slen < BUFFER_SIZE - 1) {
        memcpy(_buf + _len, s, slen);
        _len += slen;
        _buf[_len] = '\0';
    }
}

void StatusModel::append(const char* fmt, ...) {
    if (_len >= BUFFER_SIZE - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int written = vsnprintf(_buf + _len, BUFFER_SIZE - _len, fmt, ap);
    va_end(ap);
    if (written > 0) {
        size_t w = (size_t)written;
        _len = (_len + w < BUFFER_SIZE - 1) ? (_len + w) : (BUFFER_SIZE - 1);
        _buf[_len] = '\0';
    }
}

const char* StatusModel::buildStatusJson() {
    // Exposes current state formatted into static buffer
    return _buf;
}

const char* StatusModel::buildZonesJson() {
    // Exposes zone state formatted into static buffer
    return _buf;
}

const char* StatusModel::buildScheduleJson() {
    reset();
    const ScheduleConfig& cfg = thermalZones.getScheduleConfig();
    char wkBuf[160];

    append("{\"status\":\"ok\",\"enabled\":%s,\"away\":%s,\"scheduleMode\":\"%s\","
           "\"activeMoment\":%u,\"nextMoment\":%u,\"preHeatEnabled\":%s,\"windowDetectionEnabled\":%s,"
           "\"activeLevel\":\"%s\",\"nextLevel\":\"%s\",\"minsToNextSlot\":%lu,"
           "\"preHeatActive\":%s,\"isWeekend\":%s,\"dayOfWeek\":%d,",
           cfg.enabled ? "true" : "false",
           cfg.away ? "true" : "false",
           thermalZones.getScheduleModeName(),
           (unsigned)(thermalZones.getActiveMomentIdx() + 1),
           (unsigned)(thermalZones.getNextMomentIdx() + 1),
           cfg.preHeatEnabled ? "true" : "false",
           cfg.windowDetectionEnabled ? "true" : "false",
           ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()),
           ThermalZoneManager::getLevelName(thermalZones.getNextLevel()),
           (unsigned long)thermalZones.getMinutesToNextSlot(),
           thermalZones.isPreHeatActive() ? "true" : "false",
           timeAndWeather.isWeekend() ? "true" : "false",
           timeAndWeather.getDayOfWeekIndex());

    thermalZones.getSlotsString(false, wkBuf, sizeof(wkBuf));
    append("\"weekday\":\"%s\",", wkBuf);
    thermalZones.getSlotsString(true, wkBuf, sizeof(wkBuf));
    append("\"weekend\":\"%s\",", wkBuf);
    append("\"openLoopBypass\":%s,\"interlockDelaySec\":%lu,\"pumpOverrunSec\":%lu,\"pumpControlEnabled\":%s,",
           thermalZones.isOpenLoopBypass() ? "true" : "false",
           (unsigned long)(thermalZones.getInterlockDelayMs() / 1000),
           (unsigned long)(thermalZones.getPumpOverrunMs() / 1000),
           thermalZones.isPumpControlEnabled() ? "true" : "false");

    appendStr("\"momentNames\":[");
    for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
        append("%s\"%s\"", m > 0 ? "," : "", thermalZones.getMomentName((uint8_t)m));
    }

    auto emitDayMoments = [this, &cfg](size_t dayIdx) {
        appendStr("[");
        size_t zCount = thermalZones.getZoneCount();
        for (size_t m = 0; m < MAX_SCHEDULE_MOMENTS; m++) {
            append("%s{\"moment\":%u,\"time\":\"%02u:%02u\",\"temps\":{",
                   m > 0 ? "," : "", (unsigned)(m + 1),
                   cfg.days[dayIdx][m].hour, cfg.days[dayIdx][m].minute);
            for (size_t z = 0; z < zCount; z++) {
                float t = cfg.days[dayIdx][m].temps[z] > 0
                    ? decodeTempByte(cfg.days[dayIdx][m].temps[z])
                    : cfg.levelTemps[m == 3 ? LEVEL_NIGHT : LEVEL_DAY][z];
                append("%s\"%u\":%.1f", z > 0 ? "," : "",
                       (unsigned)thermalZones.getConfig(z).id, t);
            }
            appendStr("}}");
        }
        appendStr("]");
    };

    appendStr("],\"weekday_moments\":");
    emitDayMoments(1);
    appendStr(",\"weekend_moments\":");
    emitDayMoments(0);

    size_t zCount = thermalZones.getZoneCount();
    appendStr(",\"awayTemps\":{");
    for (size_t z = 0; z < zCount; z++) {
        append("%s\"%u\":%.1f", z > 0 ? "," : "",
               (unsigned)thermalZones.getConfig(z).id, thermalZones.getAwayTemp(z));
    }
    appendStr("},\"days\":[");
    for (size_t d = 0; d < 7; d++) {
        if (d > 0) appendStr(",");
        emitDayMoments(d);
    }
    appendStr("],\"zones\":[");
    for (size_t i = 0; i < zCount; i++) {
        const ThermalZoneConfig& zc = thermalZones.getConfig(i);
        const ThermalZoneStatus& zs = thermalZones.getStatus(i);
        append("%s{\"id\":%u,\"name\":\"%s\",", i > 0 ? "," : "", (unsigned)zc.id, zc.name);
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            float lvVal = (l == LEVEL_AWAY) ? thermalZones.getAwayTemp(i) : cfg.levelTemps[l][i];
            append("\"%s\":%.1f,", ThermalZoneManager::getLevelName(l), lvVal);
        }
        append("\"effectiveTarget\":%.1f,\"isManualHold\":%s,\"windowDetected\":%s,"
               "\"windowPauseActive\":%s,\"windowPauseRemainingSec\":%lu}",
               thermalZones.getEffectiveTargetTemp(i),
               zs.isManualHold ? "true" : "false",
               thermalZones.isWindowDetected(i) ? "true" : "false",
               thermalZones.isWindowPauseActive(i) ? "true" : "false",
               (unsigned long)thermalZones.getWindowPauseRemainingSec(i));
    }
    appendStr("]}");
    return _buf;
}

const char* StatusModel::buildPlantStatusJson() {
    reset();
    appendStr("{\"zones\":[");
    size_t count = thermalZones.getZoneCount();
    for (size_t i = 0; i < count; i++) {
        append("%s{\"zone\":%u,\"overheating\":%s,\"noEffect\":%s}",
               i > 0 ? "," : "", (unsigned)(i + 1),
               plantMonitor.isZoneOverheating(i) ? "true" : "false",
               plantMonitor.isZoneNoEffect(i) ? "true" : "false");
    }
    append("],\"boilerMonitor\":{\"boilerNotHeating\":%s},\"misdirection\":%s}",
           plantMonitor.isBoilerNotHeating() ? "true" : "false",
           plantMonitor.isMisdirectionDetected() ? "true" : "false");
    return _buf;
}
