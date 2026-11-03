#ifndef CONSOLE_AUTH_H
#define CONSOLE_AUTH_H

#include <Arduino.h>

// FR-75 / FR-77: console password (salted SHA-256 hash in NVS), lockout per client address, security-event ring.

enum SecSource : uint8_t { SEC_HTTP = 0, SEC_TELNET = 1, SEC_SERIAL = 2 };
enum SecEventType : uint8_t {
    SEC_LOGIN_FAIL = 0,
    SEC_LOGIN_OK,
    SEC_LOCKOUT,
    SEC_COMMAND,
    SEC_PASSWORD_CHANGE,
    SEC_FACTORY_RESET,
};
enum AuthResult : uint8_t { AUTH_OK = 0, AUTH_BAD, AUTH_LOCKED, AUTH_NOT_SET };

#define SEC_EVENT_COUNT 50  // FR-77: the last 50 events
#define AUTH_CLIENTS 4      // lockout table (FR-75.4), RAM only

struct SecEvent {
    uint32_t time;     // epoch, or uptime in seconds while the clock is not set
    uint8_t type;
    uint8_t source;
    uint8_t ip[3];     // first three parts of the client address: the last part is never kept (FR-77)
    uint8_t hasIp;
};

class ConsoleAuth {
public:
    void begin();  // after ParamStore::begin()

    bool passwordSet() const { return _set; }
    bool mustChange() const { return _set && _must; }

    // Check a password for a network client. Counts failures and records the events (FR-75.4, FR-77).
    AuthResult check(const char* password, const uint8_t ip[4], SecSource src, bool recordOk = true);
    void registerFail(const uint8_t ip[4], SecSource src);  // a wrong user name counts like a wrong password
    bool locked(const uint8_t ip[4]);

    // Serial: first password (initial, must be changed). Network: change with the old password.
    bool setInitial(const char* newPw, bool must = true);                                   // false: length outside 8..63
    AuthResult change(const char* oldPw, const char* newPw, const uint8_t ip[4], SecSource src);  // AUTH_BAD also for a bad new one

    void addEvent(SecEventType type, SecSource src, const uint8_t ip[4]);
    uint8_t eventCount() const { return _n; }
    String eventsJson() const;

private:
    bool _set = false;
    bool _must = false;
    uint8_t _salt[16] = {0};
    uint8_t _hash[32] = {0};
    SecEvent _ev[SEC_EVENT_COUNT];
    uint8_t _head = 0, _n = 0;
    struct Client {
        uint32_t ip;
        uint8_t fails;
        uint32_t lockedUntilMs;
        uint32_t lastMs;
    } _cl[AUTH_CLIENTS];

    void hashPw(const char* pw, const uint8_t* salt, uint8_t* out) const;
    bool verify(const char* pw) const;
    bool store(const char* pw, bool must);
    Client* client(const uint8_t ip[4], bool create);  // nullptr: no room, every entry is locked
    void noteFail(Client* c, SecSource src, const uint8_t ip[4]);
};

extern ConsoleAuth consoleAuth;

#endif
