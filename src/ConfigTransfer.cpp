#include "ConfigTransfer.h"
#include "BoilerRadioTrigger.h"
#include "NetworkConsole.h"
#include "ParamStore.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "Version.h"
#include <ArduinoJson.h>
#include <math.h>

// Configuration export / import (FR-65.1 ... FR-65.8). Every value is checked like the same setting set alone
// (requirements §6 ranges FR-42, zone rules FR-11.3, levels FR-29, slots FR-31, location FR-9.2, learned duty FR-47);
// what passes is applied through the same setters as the single-setting endpoints, so NVS is written on change only.

namespace {

struct NumParam {
    const char* name;
    Param param;
};
const NumParam NUM_PARAMS[] = {
    {"actuatorDelaySec", Param::InterlockDelay},
    {"pumpOverrunSec", Param::Overrun},
    {"burnerMinRunSec", Param::BurnerMinRun},
    {"burnerMinPauseSec", Param::BurnerMinPause},
    {"rfSafetyHeartbeatSec", Param::HbInterval},
    {"freezeProtectionTemp", Param::FrostTemp},
    {"sensorTimeoutSec", Param::SensorTimeout},
    {"hysteresis", Param::Hysteresis},
    {"windowThreshold", Param::WindowThreshold},
};
const char* const FLAG_PARAMS[] = {"rfSafetyHeartbeatEnabled", "openLoopBypass", "actuatorNormallyClosed",
                                   "pumpControlEnabled"};
const char* const ZONE_FIELDS[] = {"id", "name", "mac", "sensorName", "relayPin", "enabled", "offset", "solarGain", "circuits"};
const char* const SCHEDULE_KEYS[] = {"enabled", "preheat", "window", "weekday", "weekend", "levels"};
const char* const TOP_KEYS[] = {"format", "firmwareVersion", "params", "zones", "schedule", "location",
                                "learnedDuty"};
const char* const LEVELS[] = {"day", "night", "away"};

template <size_t N>
bool oneOf(const char* const (&list)[N], const char* key) {
    for (const char* k : list)
        if (strcmp(k, key) == 0) return true;
    return false;
}

double round1(double v) { return (double)lround(v * 10.0) / 10.0; }

bool flagValue(const char* name) {
    if (strcmp(name, "rfSafetyHeartbeatEnabled") == 0) return boilerRadio.isSafetyHeartbeatEnabled();
    if (strcmp(name, "openLoopBypass") == 0) return thermalZones.isOpenLoopBypass();
    if (strcmp(name, "actuatorNormallyClosed") == 0) return thermalZones.isActuatorNormallyClosed();
    return thermalZones.isPumpControlEnabled();
}

// Valid UTF-8 (no overlong forms, no surrogates), so the export is valid JSON text (FR-65.1)
bool validUtf8(const char* s) {
    const uint8_t* p = (const uint8_t*)s;
    while (*p) {
        if (*p < 0x80) { p++; continue; }
        int extra = (*p >= 0xC2 && *p <= 0xDF) ? 1 : (*p >= 0xE0 && *p <= 0xEF) ? 2 : (*p >= 0xF0 && *p <= 0xF4) ? 3 : -1;
        if (extra < 0) return false;
        uint32_t cp = *p & (0x3F >> extra);
        for (int k = 1; k <= extra; k++) {
            if ((p[k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[k] & 0x3F);
        }
        if ((extra == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (extra == 3 && (cp < 0x10000 || cp > 0x10FFFF))) return false;
        p += extra + 1;
    }
    return true;
}

// Printable text that fits the record and cannot break the JSON it is written into (FR-11.3)
bool validLabel(const char* s, size_t capacity, bool allowEmpty) {
    size_t n = strlen(s);
    if (n >= capacity || (!allowEmpty && n == 0)) return false;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c == 0x7F || c == '"' || c == '\\') return false;
    }
    return validUtf8(s);
}

bool validMac(const char* s) {
    size_t n = strlen(s);
    if (n == 0) return true;
    if (n != 17) return false;
    for (size_t i = 0; i < 17; i++) {
        if (i % 3 == 2 ? s[i] != ':' : !isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

bool isNumber(JsonVariantConst v) { return v.is<double>() && !v.is<bool>(); }

bool inRange(JsonVariantConst v, double lo, double hi) {
    if (!isNumber(v)) return false;
    double x = v.as<double>();
    return x >= lo && x <= hi;
}

struct Report {
    JsonArray applied, kept, skipped;
    void ok(const String& name, const String& value) {
        applied.add(name);
        netConsole.printf("[Import] %s = %s\n", name.c_str(), value.c_str());
    }
    void keep(const String& name) { kept.add(name); }
    void skip(const String& name, const char* reason) {
        JsonObject o = skipped.add<JsonObject>();
        o["name"] = name;
        o["reason"] = reason;
    }
};

String zoneName(size_t id, const char* field = nullptr) {
    String s = "zones[" + String((unsigned)id) + "]";
    if (field) s += String(".") + field;
    return s;
}

// ---- params -----------------------------------------------------------------------------------------------

bool applyNum(Param p, int32_t raw) {
    float f = (float)raw / ParamStore::def(p).scale;
    switch (p) {
        case Param::InterlockDelay: return thermalZones.setInterlockDelaySec((uint32_t)raw);
        case Param::Overrun: return thermalZones.setOverrunSec((uint32_t)raw);
        case Param::BurnerMinRun: return boilerRadio.setMinRunSec((uint32_t)raw);
        case Param::BurnerMinPause: return boilerRadio.setMinPauseSec((uint32_t)raw);
        case Param::HbInterval: return boilerRadio.setHeartbeatSec((uint32_t)raw);
        case Param::FrostTemp: return thermalZones.setFreezeProtectionTemp(f);
        case Param::SensorTimeout: return thermalZones.setSensorTimeoutSec((uint32_t)raw);
        case Param::Hysteresis: return thermalZones.setHysteresis(f);
        case Param::WindowThreshold: return thermalZones.setWindowThreshold(f);
        default: return false;
    }
}

void importParams(JsonVariantConst pv, Report& rep) {
    bool isObj = pv.is<JsonObjectConst>();
    if (!pv.isNull() && !isObj) rep.skip("params", "invalid");
    JsonObjectConst po = isObj ? pv.as<JsonObjectConst>() : JsonObjectConst();
    if (isObj) {
        for (JsonPairConst kv : po) {
            const char* k = kv.key().c_str();
            bool known = oneOf(FLAG_PARAMS, k);
            for (const NumParam& n : NUM_PARAMS) known = known || strcmp(n.name, k) == 0;
            if (!known) rep.skip(String("params.") + k, "unknown");
        }
    }
    for (const NumParam& n : NUM_PARAMS) {
        String name = String("params.") + n.name;
        if (!isObj || !po.containsKey(n.name)) { rep.keep(name); continue; }
        JsonVariantConst v = po[n.name];
        double val = isNumber(v) ? v.as<double>() : 0;
        double scaled = val * ParamStore::def(n.param).scale;
        int32_t raw = (int32_t)lround(scaled);
        if (!isNumber(v) || fabs(val) > 1e6 || fabs(scaled - raw) > 1e-6 || !paramStore.inRange(n.param, raw) ||
            !applyNum(n.param, raw)) {
            rep.skip(name, "invalid");
            continue;
        }
        rep.ok(name, String(val, 2));
    }
    for (const char* f : FLAG_PARAMS) {
        String name = String("params.") + f;
        if (!isObj || !po.containsKey(f)) { rep.keep(name); continue; }
        JsonVariantConst v = po[f];
        if (!v.is<bool>()) { rep.skip(name, "invalid"); continue; }
        bool on = v.as<bool>();
        bool ok = true;
        if (strcmp(f, "rfSafetyHeartbeatEnabled") == 0) boilerRadio.setSafetyHeartbeatEnabled(on);
        else if (strcmp(f, "openLoopBypass") == 0) thermalZones.setOpenLoopBypass(on);
        else if (strcmp(f, "actuatorNormallyClosed") == 0) thermalZones.setActuatorNormallyClosed(on);
        else ok = thermalZones.setPumpControlEnabled(on);  // false: a zone uses the pump relay (FR-11.3)
        if (ok) rep.ok(name, on ? "true" : "false");
        else rep.skip(name, "invalid");
    }
}

// ---- zones ------------------------------------------------------------------------------------------------

struct ZonePlan {
    uint8_t entries = 0;
    JsonObjectConst obj;
    const char* reason = nullptr;  // null: applied
    char name[32];
    char mac[18];
    char sensorName[32];
    uint8_t pin = 0;
    bool enabled = false;
    bool solarGain = false;
    uint8_t circuits = 1;
    float offset = 0, target = 0;
};

void planZone(size_t i, ZonePlan& zp, Report& rep) {
    const ThermalZoneConfig& cur = thermalZones.getConfig(i);
    size_t id = i + 1;
    strlcpy(zp.name, cur.name, sizeof(zp.name));
    strlcpy(zp.mac, cur.sensorMac, sizeof(zp.mac));
    strlcpy(zp.sensorName, cur.sensorName, sizeof(zp.sensorName));
    zp.pin = cur.relayPin;
    zp.enabled = cur.enabled;
    zp.solarGain = cur.solarGain;
    zp.circuits = cur.circuits;
    zp.offset = cur.tempOffset;
    zp.target = cur.targetTemp;

    for (JsonPairConst kv : zp.obj) {
        if (!oneOf(ZONE_FIELDS, kv.key().c_str())) rep.skip(zoneName(id, kv.key().c_str()), "unknown");
    }
    for (const char* f : ZONE_FIELDS) {
        if (strcmp(f, "id") != 0 && !zp.obj.containsKey(f)) rep.keep(zoneName(id, f));
    }
    bool bad = false;
    JsonVariantConst v;
    if (zp.obj.containsKey("name")) {
        v = zp.obj["name"];
        if (v.is<const char*>() && validLabel(v.as<const char*>(), sizeof(zp.name), false)) strlcpy(zp.name, v.as<const char*>(), sizeof(zp.name));
        else bad = true;
    }
    if (zp.obj.containsKey("mac")) {
        v = zp.obj["mac"];
        if (v.is<const char*>() && validMac(v.as<const char*>())) {
            strlcpy(zp.mac, v.as<const char*>(), sizeof(zp.mac));
            for (char* c = zp.mac; *c; c++) *c = toupper((unsigned char)*c);
        } else bad = true;
    }
    if (zp.obj.containsKey("sensorName")) {
        v = zp.obj["sensorName"];
        if (v.is<const char*>() && validLabel(v.as<const char*>(), sizeof(zp.sensorName), true)) strlcpy(zp.sensorName, v.as<const char*>(), sizeof(zp.sensorName));
        else bad = true;
    }
    if (zp.obj.containsKey("relayPin")) {
        v = zp.obj["relayPin"];
        if (v.is<int>() && v.as<int>() >= 0 && v.as<int>() < 256 && ParamStore::validRelayPin((uint8_t)v.as<int>())) zp.pin = (uint8_t)v.as<int>();
        else bad = true;
    }
    if (zp.obj.containsKey("enabled")) {
        v = zp.obj["enabled"];
        if (v.is<bool>()) zp.enabled = v.as<bool>();
        else bad = true;
    }
    if (zp.obj.containsKey("offset")) {
        v = zp.obj["offset"];
        if (inRange(v, -3.0, 3.0)) zp.offset = (float)round1(v.as<double>());  // FR-49
        else bad = true;
    }
    if (zp.obj.containsKey("solarGain")) {
        v = zp.obj["solarGain"];
        if (v.is<bool>()) zp.solarGain = v.as<bool>();
        else bad = true;
    }
    if (zp.obj.containsKey("circuits")) {
        v = zp.obj["circuits"];
        if (inRange(v, 1, 16)) zp.circuits = (uint8_t)v.as<int>();
        else bad = true;
    }
    if (bad) { zp.reason = "invalid"; return; }
    // A zone is switched on only with a sensor (FR-11.3)
    if (zp.enabled && zp.mac[0] == '\0' && zp.sensorName[0] == '\0') { zp.reason = "invalid"; return; }
    // An energised valve keeps its relay and stays on (FR-11.3, FR-3)
    if (thermalZones.getStatus(i).relayEngaged && (zp.pin != cur.relayPin || (cur.enabled && !zp.enabled)))
        zp.reason = "valve-energised";
}

void importZones(JsonVariantConst zv, Report& rep) {
    // One plan per zone in use, on the heap for the length of the import only (32 zones would be 4 KB of stack)
    const size_t zones = thermalZones.getZoneCount();
    if (zones == 0) {
        if (!zv.isNull() && !zv.is<JsonArrayConst>()) rep.skip("zones", "invalid");
        else if (zv.is<JsonArrayConst>()) {
            size_t k = 0;
            for (JsonVariantConst e : zv.as<JsonArrayConst>()) {
                k++;
                if (!e.is<JsonObjectConst>() || !e["id"].is<int>()) rep.skip("zones[#" + String((unsigned)k) + "]", "invalid");
                else rep.skip(zoneName((size_t)(e["id"].as<int>() < 0 ? 0 : e["id"].as<int>())), "no-zone");
            }
        }
        return;
    }
    ZonePlan* plan = new ZonePlan[zones];
    struct Free { ZonePlan* p; ~Free() { delete[] p; } } freePlan{plan};
    bool isArr = zv.is<JsonArrayConst>();
    if (!zv.isNull() && !isArr) rep.skip("zones", "invalid");
    if (isArr) {
        size_t k = 0;
        for (JsonVariantConst e : zv.as<JsonArrayConst>()) {
            k++;
            if (!e.is<JsonObjectConst>() || !e["id"].is<int>()) { rep.skip("zones[#" + String((unsigned)k) + "]", "invalid"); continue; }
            int id = e["id"].as<int>();
            if (id < 1 || id > (int)thermalZones.getZoneCount()) { rep.skip(zoneName((size_t)(id < 0 ? 0 : id)), "no-zone"); continue; }
            ZonePlan& zp = plan[id - 1];
            if (zp.entries < 255) zp.entries++;
            zp.obj = e.as<JsonObjectConst>();
        }
    }
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        if (plan[i].entries == 0) rep.keep(zoneName(i + 1));
        else if (plan[i].entries > 1) { plan[i].reason = "duplicate"; }
        else planZone(i, plan[i], rep);
    }
    // The configuration that results: a relay serves at most one zone and not the pump (FR-11.3, FR-14.1).
    // The pump setting of the file was applied before (FR-65.5). A zone that moves onto a taken relay is skipped
    // and keeps its relay; the check repeats until no conflict is left.
    auto pinOf = [&](size_t j) { return (plan[j].entries == 1 && !plan[j].reason) ? plan[j].pin : thermalZones.getConfig(j).relayPin; };
    // A sensor MAC serves at most one zone in the result as well (FR-11.3, FR-65.5): two zones may exchange sensors
    auto macOf = [&](size_t j) { return (plan[j].entries == 1 && !plan[j].reason) ? (const char*)plan[j].mac : thermalZones.getConfig(j).sensorMac; };
    bool changed;
    do {
        changed = false;
        for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
            if (plan[i].entries != 1 || plan[i].reason) continue;
            if (plan[i].pin != thermalZones.getConfig(i).relayPin) {
                if (thermalZones.isPumpControlEnabled() && plan[i].pin == thermalZones.getPumpRelayPin()) {
                    plan[i].reason = "pump-relay";
                    changed = true;
                    continue;
                }
                for (size_t j = 0; j < thermalZones.getZoneCount(); j++) {
                    if (j != i && pinOf(j) == plan[i].pin) {
                        plan[i].reason = "relay-taken";
                        changed = true;
                        break;
                    }
                }
                if (plan[i].reason) continue;
            }
            if (plan[i].mac[0] != '\0' && strcasecmp(plan[i].mac, thermalZones.getConfig(i).sensorMac) != 0) {
                for (size_t j = 0; j < thermalZones.getZoneCount(); j++) {
                    if (j != i && strcasecmp(macOf(j), plan[i].mac) == 0) {
                        plan[i].reason = "sensor-taken";
                        changed = true;
                        break;
                    }
                }
            }
        }
    } while (changed);
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        if (plan[i].entries == 0) continue;
        if (plan[i].reason) { rep.skip(zoneName(i + 1), plan[i].reason); continue; }
        ZonePlan& zp = plan[i];
        thermalZones.mapZone(i, zp.name, zp.mac, zp.sensorName, zp.pin, zp.target, zp.enabled);
        thermalZones.setTempOffset(i, zp.offset);
        thermalZones.setSolarGain(i, zp.solarGain);
        thermalZones.setCircuits(i, zp.circuits);
        rep.ok(zoneName(i + 1), String("name '") + zp.name + "', mac '" + zp.mac + "', sensorName '" + zp.sensorName +
                                    "', relayPin " + zp.pin + ", enabled " + (zp.enabled ? "true" : "false") +
                                    ", offset " + String(zp.offset, 1));
    }
}

// ---- schedule, location, learned duty -----------------------------------------------------------------------

String levelName(size_t id, const char* lv) { return "schedule.levels[" + String((unsigned)id) + "]." + lv; }

void importSchedule(JsonVariantConst sv, Report& rep) {
    bool isObj = sv.is<JsonObjectConst>();
    if (!sv.isNull() && !isObj) rep.skip("schedule", "invalid");
    JsonObjectConst so = isObj ? sv.as<JsonObjectConst>() : JsonObjectConst();
    if (isObj) {
        for (JsonPairConst kv : so)
            if (!oneOf(SCHEDULE_KEYS, kv.key().c_str())) rep.skip(String("schedule.") + kv.key().c_str(), "unknown");
    }
    for (const char* k : SCHEDULE_KEYS) {
        if (strcmp(k, "levels") == 0) continue;
        String name = String("schedule.") + k;
        if (!isObj || !so.containsKey(k)) { rep.keep(name); continue; }
        JsonVariantConst v = so[k];
        bool isSwitch = strcmp(k, "weekday") != 0 && strcmp(k, "weekend") != 0;
        if (isSwitch) {
            if (!v.is<bool>()) { rep.skip(name, "invalid"); continue; }
            bool on = v.as<bool>();
            if (strcmp(k, "enabled") == 0) thermalZones.setScheduleEnabled(on);
            else if (strcmp(k, "preheat") == 0) thermalZones.setPreHeatEnabled(on);
            else thermalZones.setWindowDetectionEnabled(on);
            rep.ok(name, on ? "true" : "false");
        } else {
            // the slot list buffer of setSlots holds 159 characters: a longer text is not silently cut
            if (!v.is<const char*>() || strlen(v.as<const char*>()) >= 160 ||
                !thermalZones.setSlots(strcmp(k, "weekend") == 0, v.as<const char*>())) {
                rep.skip(name, "invalid");
                continue;
            }
            rep.ok(name, v.as<const char*>());
        }
    }
    // levels per zone (FR-29)
    uint8_t count[MAX_THERMAL_ZONES] = {0};
    JsonObjectConst entry[MAX_THERMAL_ZONES];
    JsonVariantConst lv = isObj ? so["levels"] : JsonVariantConst();
    bool levelsArr = lv.is<JsonArrayConst>();
    if (!lv.isNull() && !levelsArr) rep.skip("schedule.levels", "invalid");
    if (levelsArr) {
        size_t k = 0;
        for (JsonVariantConst e : lv.as<JsonArrayConst>()) {
            k++;
            if (!e.is<JsonObjectConst>() || !e["id"].is<int>()) { rep.skip("schedule.levels[#" + String((unsigned)k) + "]", "invalid"); continue; }
            int id = e["id"].as<int>();
            if (id < 1 || id > (int)thermalZones.getZoneCount()) { rep.skip("schedule.levels[" + String(id < 0 ? 0 : id) + "]", "no-zone"); continue; }
            if (count[id - 1] < 255) count[id - 1]++;
            entry[id - 1] = e.as<JsonObjectConst>();
        }
    }
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        size_t id = i + 1;
        if (count[i] == 1) {
            for (JsonPairConst kv : entry[i])
                if (strcmp(kv.key().c_str(), "id") != 0 && !oneOf(LEVELS, kv.key().c_str())) rep.skip(levelName(id, kv.key().c_str()), "unknown");
        }
        for (uint8_t l = 0; l < LEVEL_COUNT; l++) {
            String name = levelName(id, LEVELS[l]);
            if (count[i] == 0 || (count[i] == 1 && !entry[i].containsKey(LEVELS[l]))) { rep.keep(name); continue; }
            if (count[i] > 1) { rep.skip(name, "duplicate"); continue; }
            JsonVariantConst v = entry[i][LEVELS[l]];
            if (!inRange(v, 5.0, TARGET_MAX_C)) { rep.skip(name, "invalid"); continue; }
            float t = (float)round1(v.as<double>());
            if (!thermalZones.setLevelTemp(i, (ComfortLevel)l, t)) { rep.skip(name, "invalid"); continue; }
            rep.ok(name, String(t, 1));
        }
    }
}

void importLocation(JsonVariantConst lv, Report& rep) {
    bool have = timeAndWeather.getLocation()[0] != '\0';
    if (lv.isNull()) {
        if (have) rep.keep("location");
        return;
    }
    if (!lv.is<const char*>() || !ParamStore::validLocation(lv.as<const char*>())) { rep.skip("location", "invalid"); return; }
    bool changed = strcmp(timeAndWeather.getLocation(), lv.as<const char*>()) != 0;
    if (!timeAndWeather.setLocation(lv.as<const char*>())) { rep.skip("location", "invalid"); return; }
    if (changed) timeAndWeather.forceWeatherUpdate();
    rep.applied.add("location");  // the coordinates never reach the log (FR-76)
}

void importLearned(JsonVariantConst dv, Report& rep) {
    uint8_t count[MAX_THERMAL_ZONES] = {0};
    JsonObjectConst entry[MAX_THERMAL_ZONES];
    bool isArr = dv.is<JsonArrayConst>();
    if (!dv.isNull() && !isArr) rep.skip("learnedDuty", "invalid");
    if (isArr) {
        size_t k = 0;
        for (JsonVariantConst e : dv.as<JsonArrayConst>()) {
            k++;
            if (!e.is<JsonObjectConst>() || !e["id"].is<int>()) { rep.skip("learnedDuty[#" + String((unsigned)k) + "]", "invalid"); continue; }
            int id = e["id"].as<int>();
            if (id < 1 || id > (int)thermalZones.getZoneCount()) { rep.skip("learnedDuty[" + String(id < 0 ? 0 : id) + "]", "no-zone"); continue; }
            if (count[id - 1] < 255) count[id - 1]++;
            entry[id - 1] = e.as<JsonObjectConst>();
        }
    }
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        String name = "learnedDuty[" + String((unsigned)(i + 1)) + "]";
        if (count[i] == 1) {
            for (JsonPairConst kv : entry[i])
                if (strcmp(kv.key().c_str(), "id") != 0 && strcmp(kv.key().c_str(), "duty") != 0) rep.skip(name + "." + kv.key().c_str(), "unknown");
        }
        if (count[i] == 0 || (count[i] == 1 && !entry[i].containsKey("duty"))) { rep.keep(name); continue; }
        if (count[i] > 1) { rep.skip(name, "duplicate"); continue; }
        JsonVariantConst v = entry[i]["duty"];
        if (!inRange(v, 0.0, 60.0)) { rep.skip(name, "invalid"); continue; }  // FR-47: base duty 0-60 %
        thermalZones.setLearnedDutyPct(i, (float)v.as<double>());
        rep.ok(name, String(thermalZones.getLearnedDutyPct(i), 1));
    }
}

}  // namespace

String configExportJson() {
    JsonDocument d;
    d["format"] = 1;
    d["firmwareVersion"] = APP_VERSION;
    JsonObject p = d["params"].to<JsonObject>();
    for (const NumParam& n : NUM_PARAMS) {
        if (ParamStore::def(n.param).scale == 1) p[n.name] = (int32_t)paramStore.get(n.param);
        else p[n.name] = (double)paramStore.get(n.param) / ParamStore::def(n.param).scale;
    }
    for (const char* f : FLAG_PARAMS) p[f] = flagValue(f);

    JsonArray zones = d["zones"].to<JsonArray>();
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        const ThermalZoneConfig& c = thermalZones.getConfig(i);
        JsonObject z = zones.add<JsonObject>();
        z["id"] = c.id;
        z["name"] = c.name;
        z["mac"] = c.sensorMac;
        z["sensorName"] = c.sensorName;
        z["relayPin"] = c.relayPin;
        z["enabled"] = c.enabled;
        z["offset"] = round1(c.tempOffset);
        z["solarGain"] = c.solarGain;
        z["circuits"] = c.circuits;
    }

    JsonObject s = d["schedule"].to<JsonObject>();
    s["enabled"] = thermalZones.isScheduleEnabled();
    s["preheat"] = thermalZones.isPreHeatEnabled();
    s["window"] = thermalZones.isWindowDetectionEnabled();
    s["weekday"] = thermalZones.getSlotsString(false);
    s["weekend"] = thermalZones.getSlotsString(true);
    JsonArray levels = s["levels"].to<JsonArray>();
    const ScheduleConfig& sc = thermalZones.getScheduleConfig();
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        JsonObject l = levels.add<JsonObject>();
        l["id"] = thermalZones.getConfig(i).id;
        for (uint8_t k = 0; k < LEVEL_COUNT; k++) l[LEVELS[k]] = round1(sc.levelTemps[k][i]);
    }

    if (timeAndWeather.getLocation()[0] != '\0') d["location"] = timeAndWeather.getLocation();

    JsonArray learned = d["learnedDuty"].to<JsonArray>();
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        JsonObject l = learned.add<JsonObject>();
        l["id"] = thermalZones.getConfig(i).id;
        l["duty"] = thermalZones.getLearnedDutyPct(i);
    }
    String out;
    serializeJson(d, out);
    return out;
}

int configImport(const String& body, String& answer) {
    JsonDocument in;
    DeserializationError err = deserializeJson(in, body);
    if (err || !in.is<JsonObject>() || !in["format"].is<int>() || in["format"].as<int>() != 1) {
        answer = F("{\"status\":\"error\",\"message\":\"Not a JSON object with \\\"format\\\": 1; nothing changed\"}");
        return 400;
    }
    JsonDocument out;
    out["status"] = "ok";
    Report rep{out["applied"].to<JsonArray>(), out["kept"].to<JsonArray>(), out["skipped"].to<JsonArray>()};
    for (JsonPairConst kv : in.as<JsonObjectConst>())
        if (!oneOf(TOP_KEYS, kv.key().c_str())) rep.skip(kv.key().c_str(), "unknown");

    // The pump setting is checked first, against the zones as they are; the zones then against the resulting pump
    JsonObjectConst root = in.as<JsonObjectConst>();
    importParams(root["params"], rep);
    importZones(root["zones"], rep);
    importSchedule(root["schedule"], rep);
    importLocation(root["location"], rep);
    importLearned(root["learnedDuty"], rep);
    serializeJson(out, answer);
    return 200;
}
