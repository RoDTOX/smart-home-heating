#include "ConsoleAuth.h"
#include "NetworkConsole.h"
#include "ParamStore.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <time.h>

ConsoleAuth consoleAuth;

static const char* BLOB_KEY = "cpw";
static const int HASH_ROUNDS = 2000;  // a light stretch: well inside one loop() pass (NFR-1.8)

#pragma pack(push, 1)
struct PwRecord {
    uint8_t magic;  // 0xC5 = a password is set
    uint8_t must;   // 1 = the first network login must change it
    uint8_t salt[16];
    uint8_t hash[32];
};
#pragma pack(pop)

static uint32_t ipKey(const uint8_t ip[4]) {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | ip[3];
}

void ConsoleAuth::begin() {
    memset(_cl, 0, sizeof(_cl));
    PwRecord r;
    _set = false;
    _must = false;
    if (paramStore.loadBlob(BLOB_KEY, &r, sizeof(r)) && r.magic == 0xC5) {
        _set = true;
        _must = r.must != 0;
        memcpy(_salt, r.salt, 16);
        memcpy(_hash, r.hash, 32);
    }
}

void ConsoleAuth::hashPw(const char* pw, const uint8_t* salt, uint8_t* out) const {
    uint8_t buf[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, salt, 16);
    mbedtls_sha256_update_ret(&ctx, (const uint8_t*)pw, strlen(pw));
    mbedtls_sha256_finish_ret(&ctx, buf);
    for (int i = 1; i < HASH_ROUNDS; i++) {
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, buf, 32);
        mbedtls_sha256_update_ret(&ctx, salt, 16);
        mbedtls_sha256_finish_ret(&ctx, buf);
    }
    mbedtls_sha256_free(&ctx);
    memcpy(out, buf, 32);
}

bool ConsoleAuth::verify(const char* pw) const {
    if (!_set) return false;
    uint8_t h[32];
    hashPw(pw, _salt, h);
    uint8_t diff = 0;  // constant time compare
    for (int i = 0; i < 32; i++) diff |= h[i] ^ _hash[i];
    return diff == 0;
}

bool ConsoleAuth::store(const char* pw, bool must) {
    size_t len = strlen(pw);
    if (len < 8 || len > 63) return false;
    PwRecord r;
    r.magic = 0xC5;
    r.must = must ? 1 : 0;
    esp_fill_random(r.salt, 16);
    hashPw(pw, r.salt, r.hash);
    paramStore.saveBlob(BLOB_KEY, &r, sizeof(r));
    // fail closed: use the new values only if they can be read back
    PwRecord back;
    if (!paramStore.loadBlob(BLOB_KEY, &back, sizeof(back)) || memcmp(&back, &r, sizeof(r)) != 0) return false;
    _set = true;
    _must = must;
    memcpy(_salt, r.salt, 16);
    memcpy(_hash, r.hash, 32);
    return true;
}

ConsoleAuth::Client* ConsoleAuth::client(const uint8_t ip[4], bool create) {
    uint32_t k = ipKey(ip);
    Client* freeSlot = nullptr;
    Client* oldest = nullptr;
    for (int i = 0; i < AUTH_CLIENTS; i++) {
        if (_cl[i].ip == k && k != 0) return &_cl[i];
        if (_cl[i].ip == 0 && !freeSlot) freeSlot = &_cl[i];
        bool lockedNow = _cl[i].lockedUntilMs != 0 && (int32_t)(_cl[i].lockedUntilMs - millis()) > 0;
        if (!lockedNow && (!oldest || _cl[i].lastMs < oldest->lastMs)) oldest = &_cl[i];
    }
    if (!create) return nullptr;
    Client* c = freeSlot ? freeSlot : oldest;
    if (!c) return nullptr;
    memset(c, 0, sizeof(*c));
    c->ip = k;
    return c;
}

bool ConsoleAuth::locked(const uint8_t ip[4]) {
    uint32_t k = ipKey(ip);
    for (int i = 0; i < AUTH_CLIENTS; i++) {
        if (_cl[i].ip == k && k != 0 && _cl[i].lockedUntilMs != 0) {
            if ((int32_t)(_cl[i].lockedUntilMs - millis()) > 0) return true;
            _cl[i].lockedUntilMs = 0;  // expired: must not come back after 2^31 ms
            return false;
        }
    }
    // a new address finds no room while every entry is locked: refuse it too
    bool full = true;
    for (int i = 0; i < AUTH_CLIENTS; i++) {
        if (_cl[i].ip == 0 || _cl[i].lockedUntilMs == 0 || (int32_t)(_cl[i].lockedUntilMs - millis()) <= 0) full = false;
    }
    return full && k != 0;
}

void ConsoleAuth::unlockAll() {
    for (int i = 0; i < AUTH_CLIENTS; i++) {
        _cl[i].fails = 0;
        _cl[i].lockedUntilMs = 0;
    }
}

void ConsoleAuth::noteFail(Client* c, SecSource src, const uint8_t ip[4]) {
    c->lastMs = millis();
    if (c->fails < 255) c->fails++;
    addEvent(SEC_LOGIN_FAIL, src, ip);
    if (c->fails >= paramStore.get(Param::LoginAttempts)) {
        c->lockedUntilMs = millis() + (uint32_t)paramStore.get(Param::LoginLockout) * 1000UL;
        if (c->lockedUntilMs == 0) c->lockedUntilMs = 1;
        c->fails = 0;
        addEvent(SEC_LOCKOUT, src, ip);
    }
}

void ConsoleAuth::registerFail(const uint8_t ip[4], SecSource src) {
    Client* c = client(ip, true);
    if (c) noteFail(c, src, ip);
    else addEvent(SEC_LOGIN_FAIL, src, ip);
}

AuthResult ConsoleAuth::check(const char* password, const uint8_t ip[4], SecSource src, bool recordOk) {
    if (!_set) return AUTH_NOT_SET;
    if (locked(ip)) return AUTH_LOCKED;
    if (verify(password)) {
        Client* c = client(ip, false);
        if (c) c->fails = 0;
        if (recordOk) addEvent(SEC_LOGIN_OK, src, ip);
        return AUTH_OK;
    }
    registerFail(ip, src);
    return AUTH_BAD;
}

bool ConsoleAuth::setInitial(const char* newPw, bool must) {
    if (!store(newPw, must)) return false;
    uint8_t none[4] = {0, 0, 0, 0};
    addEvent(SEC_PASSWORD_CHANGE, SEC_SERIAL, none);
    return true;
}

AuthResult ConsoleAuth::change(const char* oldPw, const char* newPw, const uint8_t ip[4], SecSource src) {
    if (!_set) return AUTH_NOT_SET;
    if (locked(ip)) return AUTH_LOCKED;
    if (!verify(oldPw)) {
        registerFail(ip, src);
        return AUTH_BAD;
    }
    if (strcmp(oldPw, newPw) == 0 || !store(newPw, false)) return AUTH_BAD;  // the new one differs and is 8..63
    addEvent(SEC_PASSWORD_CHANGE, src, ip);
    return AUTH_OK;
}

void ConsoleAuth::addEvent(SecEventType type, SecSource src, const uint8_t ip[4]) {
    SecEvent& e = _ev[_head];
    time_t now = time(nullptr);
    e.time = (now > 1700000000) ? (uint32_t)now : (uint32_t)(millis() / 1000);
    e.type = type;
    e.source = src;
    e.hasIp = (src != SEC_SERIAL && ip && (ip[0] | ip[1] | ip[2] | ip[3])) ? 1 : 0;
    if (e.hasIp) memcpy(e.ip, ip, 3);  // never the last part
    _head = (_head + 1) % SEC_EVENT_COUNT;
    if (_n < SEC_EVENT_COUNT) _n++;
}

String ConsoleAuth::eventsJson() const {
    static const char* TYPES[] = {"login-fail", "login-ok", "lockout", "command", "password-change", "factory-reset"};
    static const char* SRC[] = {"http", "telnet", "serial"};
    String j;
    j.reserve(48 + (size_t)_n * 78);
    j = String("{\"count\":") + _n + ",\"events\":[";
    uint8_t start = (_n < SEC_EVENT_COUNT) ? 0 : _head;  // oldest first
    for (uint8_t i = 0; i < _n; i++) {
        const SecEvent& e = _ev[(start + i) % SEC_EVENT_COUNT];
        if (i) j += ",";
        j += String("{\"time\":") + e.time + ",\"type\":\"" + TYPES[e.type] + "\",\"source\":\"" + SRC[e.source] + "\"";
        if (e.hasIp) j += String(",\"client\":\"") + e.ip[0] + "." + e.ip[1] + "." + e.ip[2] + ".x\"";
        j += "}";
    }
    return j + "]}";
}
