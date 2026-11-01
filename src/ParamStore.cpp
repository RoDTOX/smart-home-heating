#include "ParamStore.h"
#include "Config.h"
#include "NetworkConsole.h"
#include <Preferences.h>

ParamStore paramStore;

// Schema of the NVS layout below. Version 1 replaces the per-module keys used up to firmware 1.2.0
// (namespaces "zones", "boiler", "location", "passport"), copied once by migrateLegacy(). The old keys are kept
// (not cleared) during migration step 1, so flashing back the previous firmware still finds its settings.
static const uint8_t SCHEMA_VERSION = 1;

static const char* NS_CFG = "cfg";      // parameters, schema, location
static const char* NS_ZONE = "zone";    // ZoneRecord per zone
static const char* NS_LEARN = "learn";  // learned TPI base duty per zone
static const char* NS_STATE = "state";  // persistent state
static const char* NS_BLOB = "sched";   // schedule blob (owned by the schedule)

// Defaults and ranges are requirements §6 (process rule R2), written in the parameter's own unit and stored as
// value * scale. tools/sh_process_check.py reads these rows and compares them with §6.
#define P(name, key, def, lo, hi, scale, flags) \
    {name, key, (int32_t)((def) * (scale) + 0.5f), (int32_t)((lo) * (scale) + 0.5f), (int32_t)((hi) * (scale) + 0.5f), scale, flags}

static const ParamDef PARAMS[(uint8_t)Param::Count] = {
    P("interlockDelay", "intlk", 180, 120, 600, 1, P_AUTH),
    P("overrun", "ovr", 180, 180, 600, 1, P_AUTH),
    P("burnerMinRun", "brun", 180, 120, 600, 1, P_AUTH),
    P("burnerMinPause", "bpause", 180, 120, 600, 1, P_AUTH),
    P("hysteresis", "hyst", 0.3, 0.1, 1.0, 100, P_NONE),
    P("sensorTimeout", "stout", 1200, 60, 3600, 1, P_AUTH),
    P("frostTemp", "frost", 12.0, 8.0, 16.0, 10, P_AUTH),
    P("windowThreshold", "window", 1.2, 0.8, 2.0, 10, P_NONE),
    P("hbInterval", "hb", 60, 30, 900, 1, P_AUTH),
    P("boilerLinkHeartbeat", "hb_en", 1, 0, 1, 1, P_AUTH),
    P("openLoopBypass", "olb", 0, 0, 1, 1, P_AUTH),
    P("actuatorType", "act", 0, 0, 1, 1, P_AUTH),
    P("pumpOutput", "pump", 0, 0, 1, 1, P_AUTH),
};

static const char* STATE_KEYS[(uint8_t)StateKey::Count] = {
    "auto", "as_epoch", "b_on", "rt_tot", "rt_today", "rt_yest", "rt_mon", "rt_lmon", "rt_day", "rt_month",
};

static const uint8_t DEFAULT_PINS[PARAM_ZONES] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3};

bool ParamStore::validRelayPin(uint8_t pin) {
    return pin == PIN_RELAY_ZONE_1 || pin == PIN_RELAY_ZONE_2 || pin == PIN_RELAY_ZONE_3 || pin == PIN_RELAY_SPARE;
}

static bool validPin(uint8_t pin) {
    return ParamStore::validRelayPin(pin);
}

static bool terminated(const char* s, size_t len) {
    return memchr(s, '\0', len) != nullptr;
}

static const char* zoneKey(size_t i) {
    static const char* keys[PARAM_ZONES] = {"z0", "z1", "z2"};
    return keys[i];
}

// ------------------------------------------------------------------------------------------------------------

const ParamDef& ParamStore::def(Param p) {
    return PARAMS[(uint8_t)p < (uint8_t)Param::Count ? (uint8_t)p : 0];
}

bool ParamStore::inRange(Param p, int32_t raw) const {
    const ParamDef& d = def(p);
    return raw >= d.min && raw <= d.max;
}

int32_t ParamStore::toRaw(Param p, float value) {
    return (int32_t)lroundf(value * def(p).scale);
}

bool ParamStore::set(Param p, int32_t raw) {
    if (!inRange(p, raw)) return false;
    if (_values[(uint8_t)p] != raw) {
        _values[(uint8_t)p] = raw;
        writeParam(p);
    }
    return true;
}

bool ParamStore::setf(Param p, float value) {
    return set(p, toRaw(p, value));
}

void ParamStore::defaultZone(size_t i, ZoneRecord& z) {
    memset(&z, 0, sizeof(z));
    // FR-11.1: no installation data in firmware; the zone stays off until it is mapped at commissioning.
    snprintf(z.name, sizeof(z.name), "Zone %u", (unsigned)(i + 1));
    z.relayPin = DEFAULT_PINS[i];
    z.enabled = 0;
    z.targetC10 = (int16_t)lroundf(DEFAULT_SETPOINT_TEMP * 10);
}

bool ParamStore::validZone(const ZoneRecord& z) {
    return terminated(z.name, sizeof(z.name)) && terminated(z.sensorMac, sizeof(z.sensorMac)) &&
           terminated(z.sensorName, sizeof(z.sensorName)) && validPin(z.relayPin) && z.enabled <= 1 &&
           z.offsetC10 >= -30 && z.offsetC10 <= 30 && z.targetC10 >= 50 && z.targetC10 <= 300;
}

bool ParamStore::validLocation(const char* coords) {
    if (!coords) return false;
    float lat, lon;
    char tail;
    if (sscanf(coords, " %f , %f %c", &lat, &lon, &tail) != 2) return false;
    return lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f;
}

// ------------------------------------------------------------------------------------------------------------

void ParamStore::begin() {
    for (uint8_t p = 0; p < (uint8_t)Param::Count; p++) _values[p] = PARAMS[p].def;
    for (size_t i = 0; i < PARAM_ZONES; i++) {
        defaultZone(i, _zones[i]);
        _learned[i] = 0;
    }
    memset(_state, 0, sizeof(_state));
    _state[(uint8_t)StateKey::AutoMode] = 1;
    _location[0] = '\0';

    Preferences prefs;
    _schema = 0;
    if (prefs.begin(NS_CFG, true)) {
        _schema = prefs.getUChar("schema", 0);
        prefs.end();
    }
    if (_schema == 0) {
        migrateLegacy();  // no schema key: first boot of this layout
    } else {
        if (_schema != SCHEMA_VERSION) {
            // Written by another firmware version: never overwrite it with defaults, load with range checks
            netConsole.printf("[Params] NVS schema %u, this firmware knows %u - loading with range checks\n",
                              _schema, SCHEMA_VERSION);
        }
        loadAll();
    }
}

void ParamStore::loadAll() {
    Preferences prefs;
    bool repair[(uint8_t)Param::Count] = {false};
    if (prefs.begin(NS_CFG, true)) {
        for (uint8_t i = 0; i < (uint8_t)Param::Count; i++) {
            int32_t v = prefs.getInt(PARAMS[i].key, PARAMS[i].def);
            if (inRange((Param)i, v)) {
                _values[i] = v;
            } else {
                netConsole.printf("[Params] %s = %d out of range, default %d used\n", PARAMS[i].name, v, PARAMS[i].def);
                repair[i] = true;
            }
        }
        prefs.getString("loc", _location, sizeof(_location));
        prefs.end();
    }
    for (uint8_t i = 0; i < (uint8_t)Param::Count; i++) {
        if (repair[i]) writeParam((Param)i);
    }
    if (!validLocation(_location)) _location[0] = '\0';

    if (prefs.begin(NS_ZONE, true)) {
        for (size_t i = 0; i < PARAM_ZONES; i++) {
            ZoneRecord z;
            if (prefs.getBytes(zoneKey(i), &z, sizeof(z)) == sizeof(z) && validZone(z)) {
                _zones[i] = z;
            } else if (prefs.isKey(zoneKey(i))) {
                netConsole.printf("[Params] zone %u record invalid, defaults used\n", (unsigned)(i + 1));
            }
        }
        prefs.end();
    }

    if (prefs.begin(NS_LEARN, true)) {
        for (size_t i = 0; i < PARAM_ZONES; i++) {
            int16_t v = prefs.getShort(zoneKey(i), 0);
            if (v >= 0 && v <= PARAM_LEARNED_MAX) {
                _learned[i] = v;
            } else {
                netConsole.printf("[Params] zone %u learned duty %d out of bounds, reset (DTC-301)\n", (unsigned)(i + 1), v);
                _learnedResets++;
            }
        }
        prefs.end();
    }

    if (prefs.begin(NS_STATE, true)) {
        for (uint8_t k = 0; k < (uint8_t)StateKey::Count; k++) {
            _state[k] = prefs.getUInt(STATE_KEYS[k], _state[k]);
        }
        prefs.end();
    }
    netConsole.printf("[Params] Loaded (schema %u)\n", _schema);
}

// One-time copy of the keys used up to firmware 1.2.0. A value inside its §6 range is kept, anything else falls
// back to the default; both are logged. The schema key is written only when every record was stored, so a failed
// or interrupted copy is repeated at the next boot from the untouched old keys.
void ParamStore::migrateLegacy() {
    Preferences prefs;
    bool found = false;

    auto take = [this](Param p, int32_t raw, const char* from) {
        if (inRange(p, raw)) {
            _values[(uint8_t)p] = raw;
            netConsole.printf("[Params][Migrate] %s = %d (from %s)\n", def(p).name, raw, from);
        } else {
            netConsole.printf("[Params][Migrate] %s: old value %d out of range, default %d used\n",
                              def(p).name, raw, def(p).def);
        }
    };

    if (prefs.begin("zones", true)) {
        if (prefs.isKey("z_init")) {
            found = true;
            take(Param::FrostTemp, lroundf(prefs.getFloat("frz_temp", 12.0f) * 10), "zones/frz_temp");
            take(Param::SensorTimeout, (int32_t)prefs.getUInt("s_tout", 1200), "zones/s_tout");
            take(Param::ActuatorType, prefs.getBool("act_nc", true) ? 0 : 1, "zones/act_nc");
            take(Param::PumpOutput, prefs.getBool("pump_en", false) ? 1 : 0, "zones/pump_en");
            _state[(uint8_t)StateKey::AutoMode] = prefs.getBool("autoMode", true) ? 1 : 0;
            _state[(uint8_t)StateKey::AntiSeizeEpoch] = prefs.getUInt("as_epoch", 0);

            bool hystTaken = false, windowTaken = false;
            for (size_t i = 0; i < PARAM_ZONES; i++) {
                char k[16];
                ZoneRecord z;
                defaultZone(i, z);
                snprintf(k, sizeof(k), "z%u_name", (unsigned)i);
                prefs.getString(k, z.name, sizeof(z.name));
                snprintf(k, sizeof(k), "z%u_mac", (unsigned)i);
                prefs.getString(k, z.sensorMac, sizeof(z.sensorMac));
                snprintf(k, sizeof(k), "z%u_sname", (unsigned)i);
                prefs.getString(k, z.sensorName, sizeof(z.sensorName));
                snprintf(k, sizeof(k), "z%u_pin", (unsigned)i);
                uint8_t pin = prefs.getUChar(k, z.relayPin);
                if (validPin(pin)) z.relayPin = pin;
                snprintf(k, sizeof(k), "z%u_en", (unsigned)i);
                z.enabled = prefs.getBool(k, true) ? 1 : 0;
                snprintf(k, sizeof(k), "z%u_ofs", (unsigned)i);
                int32_t ofs = lroundf(prefs.getFloat(k, 0.0f) * 10);
                if (ofs >= -30 && ofs <= 30) z.offsetC10 = (int16_t)ofs;
                snprintf(k, sizeof(k), "z%u_target", (unsigned)i);
                int32_t target = lroundf(prefs.getFloat(k, DEFAULT_SETPOINT_TEMP) * 10);
                if (target >= 50 && target <= 300) z.targetC10 = (int16_t)target;
                if (z.name[0] == '\0') snprintf(z.name, sizeof(z.name), "Zone %u", (unsigned)(i + 1));
                _zones[i] = z;
                netConsole.printf("[Params][Migrate] zone %u '%s' sensor %s pin %u %s\n", (unsigned)(i + 1), z.name,
                                  z.sensorMac[0] ? z.sensorMac : "-", z.relayPin, z.enabled ? "enabled" : "disabled");

                // Hysteresis and window threshold become one global value each (§6): the first in-range zone value
                snprintf(k, sizeof(k), "z%u_hyst", (unsigned)i);
                int32_t hyst = lroundf(prefs.getFloat(k, 0.3f) * 100);
                if (!hystTaken && inRange(Param::Hysteresis, hyst)) {
                    take(Param::Hysteresis, hyst, k);
                    hystTaken = true;
                }
                snprintf(k, sizeof(k), "z%u_wdrop", (unsigned)i);
                int32_t wdrop = lroundf(prefs.getFloat(k, 1.2f) * 10);
                if (!windowTaken && inRange(Param::WindowThreshold, wdrop)) {
                    take(Param::WindowThreshold, wdrop, k);
                    windowTaken = true;
                } else if (!inRange(Param::WindowThreshold, wdrop)) {
                    netConsole.printf("[Params][Migrate] %s = %.1f C out of range, dropped\n", k, wdrop / 10.0f);
                }

                snprintf(k, sizeof(k), "z%u_tpi", (unsigned)i);
                int32_t learned = lroundf(prefs.getFloat(k, 0.0f) * 1000);
                if (learned >= 0 && learned <= PARAM_LEARNED_MAX) {
                    _learned[i] = (int16_t)learned;
                } else {
                    _learnedResets++;
                }
            }
        }
        prefs.end();
    }

    if (prefs.begin("boiler", true)) {
        if (prefs.isKey("min_run") || prefs.isKey("last_cmd") || prefs.isKey("rt_tot")) {
            found = true;
            take(Param::BurnerMinRun, (int32_t)(prefs.getUInt("min_run", 180000UL) / 1000UL), "boiler/min_run");
            take(Param::BurnerMinPause, (int32_t)(prefs.getUInt("min_pause", 180000UL) / 1000UL), "boiler/min_pause");
            take(Param::HbInterval, (int32_t)(prefs.getUInt("hb_ms", 60000UL) / 1000UL), "boiler/hb_ms");
            take(Param::BoilerLinkHeartbeat, prefs.getBool("hb_en", true) ? 1 : 0, "boiler/hb_en");
            _state[(uint8_t)StateKey::BoilerLastOn] = prefs.getBool("last_cmd", false) ? 1 : 0;
            _state[(uint8_t)StateKey::RuntimeTotal] = prefs.getUInt("rt_tot", 0);
            _state[(uint8_t)StateKey::RuntimeToday] = prefs.getUInt("rt_today", 0);
            _state[(uint8_t)StateKey::RuntimeYesterday] = prefs.getUInt("rt_yest", 0);
            _state[(uint8_t)StateKey::RuntimeThisMonth] = prefs.getUInt("rt_mon", 0);
            _state[(uint8_t)StateKey::RuntimeLastMonth] = prefs.getUInt("rt_lmon", 0);
            _state[(uint8_t)StateKey::RuntimeDay] = prefs.getUChar("last_day", 0);
            _state[(uint8_t)StateKey::RuntimeMonth] = prefs.getUChar("last_mon", 0);
        }
        prefs.end();
    }

    const char* locNs[] = {"location", "passport"};
    for (const char* ns : locNs) {
        if (_location[0] == '\0' && prefs.begin(ns, true)) {
            char loc[32] = "";
            prefs.getString("coords", loc, sizeof(loc));
            prefs.end();
            if (validLocation(loc)) {
                strlcpy(_location, loc, sizeof(_location));
                found = true;
            }
        }
    }

    // Write the new layout; the schema key last and only if every write succeeded.
    bool ok = true;
    for (uint8_t p = 0; p < (uint8_t)Param::Count; p++) ok &= writeParam((Param)p);
    for (size_t i = 0; i < PARAM_ZONES; i++) {
        ok &= writeZone(i);
        ok &= writeLearned(i);
    }
    for (uint8_t k = 0; k < (uint8_t)StateKey::Count; k++) ok &= writeState((StateKey)k);
    ok &= writeLocation();

    if (ok && prefs.begin(NS_CFG, false)) {
        ok = prefs.putUChar("schema", SCHEMA_VERSION) == 1;
        prefs.end();
    }
    if (ok) _schema = SCHEMA_VERSION;
    netConsole.printf("[Params][Migrate] %s, %s\n", found ? "Old settings copied" : "No old settings, defaults",
                      ok ? "schema written" : "WRITE FAILED - running on the copied values, retry at next boot");
}

// ------------------------------------------------------------------------------------------------------------

bool ParamStore::writeParam(Param p) {
    Preferences prefs;
    if (!prefs.begin(NS_CFG, false)) return false;
    bool ok = prefs.putInt(def(p).key, _values[(uint8_t)p]) == sizeof(int32_t);
    prefs.end();
    return ok;
}

void ParamStore::saveZone(size_t i, const ZoneRecord& z) {
    if (i >= PARAM_ZONES || !validZone(z)) return;
    if (memcmp(&_zones[i], &z, sizeof(z)) == 0) return;
    _zones[i] = z;
    writeZone(i);
}

bool ParamStore::writeZone(size_t i) {
    Preferences prefs;
    if (!prefs.begin(NS_ZONE, false)) return false;
    bool ok = prefs.putBytes(zoneKey(i), &_zones[i], sizeof(ZoneRecord)) == sizeof(ZoneRecord);
    prefs.end();
    return ok;
}

void ParamStore::saveLearned(size_t i, int16_t perMille) {
    if (i >= PARAM_ZONES || perMille < 0 || perMille > PARAM_LEARNED_MAX || _learned[i] == perMille) return;
    _learned[i] = perMille;
    writeLearned(i);
}

bool ParamStore::writeLearned(size_t i) {
    Preferences prefs;
    if (!prefs.begin(NS_LEARN, false)) return false;
    bool ok = prefs.putShort(zoneKey(i), _learned[i]) == sizeof(int16_t);
    prefs.end();
    return ok;
}

void ParamStore::saveState(StateKey k, uint32_t v) {
    if (_state[(uint8_t)k] == v) return;
    _state[(uint8_t)k] = v;
    writeState(k);
}

bool ParamStore::writeState(StateKey k) {
    Preferences prefs;
    if (!prefs.begin(NS_STATE, false)) return false;
    bool ok = prefs.putUInt(STATE_KEYS[(uint8_t)k], _state[(uint8_t)k]) == sizeof(uint32_t);
    prefs.end();
    return ok;
}

bool ParamStore::setLocation(const char* coords) {
    if (!validLocation(coords)) return false;
    if (strcmp(coords, _location) != 0) {
        strlcpy(_location, coords, sizeof(_location));
        writeLocation();
    }
    return true;
}

bool ParamStore::writeLocation() {
    Preferences prefs;
    if (!prefs.begin(NS_CFG, false)) return false;
    bool ok = prefs.putString("loc", _location) == strlen(_location);
    prefs.end();
    return ok;
}

bool ParamStore::loadBlob(const char* key, void* buf, size_t len) {
    Preferences prefs;
    if (!prefs.begin(NS_BLOB, true)) return false;
    bool ok = prefs.getBytes(key, buf, len) == len;
    prefs.end();
    return ok;
}

void ParamStore::saveBlob(const char* key, const void* buf, size_t len) {
    Preferences prefs;
    if (!prefs.begin(NS_BLOB, false)) return;
    uint8_t current[256];
    bool same = len <= sizeof(current) && prefs.getBytes(key, current, len) == len && memcmp(current, buf, len) == 0;
    if (!same) prefs.putBytes(key, buf, len);
    prefs.end();
}
