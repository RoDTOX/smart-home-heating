#ifndef BOILER_LINK_H
#define BOILER_LINK_H

#include <Arduino.h>

// BoilerLink (docs/architecture.md §7.4, §12.2, FR-55):
// L2 I/O abstraction layer for boiler ON/OFF transport.
// Decouples control and safety logic from physical transmission media (RF 433, dry contact, or future receiver).

enum class SendResult : uint8_t {
    Ok = 0,
    Idempotent = 1,
    Cooldown = 2,
    Error = 3
};

enum class LinkHealth : uint8_t {
    Unknown = 0,
    Ok = 1,
    Lost = 2
};

enum class BoilerLinkType : uint8_t {
    Rf433 = 0,
    Wired = 1,
    Receiver = 2
};

class BoilerLink {
public:
    virtual ~BoilerLink() {}
    virtual void begin(uint8_t pin) = 0;
    virtual SendResult send(bool on, bool force = false) = 0;
    virtual void tick(uint32_t nowMs) = 0;
    virtual bool resend() = 0;
    virtual bool isFailSafe() const = 0;
    virtual LinkHealth health() const = 0;
    virtual bool lastState() const = 0;
    virtual uint32_t lastTxMs() const = 0;
    virtual BoilerLinkType type() const = 0;
    virtual const char* typeName() const = 0;
};

// Rf433Link: 433.92 MHz OOK pulse train (FR-5.1), 3s cooldown (FR-17.2), heartbeat at hbInterval (FR-17.7, FR-55).
// Fail-safe only when periodic heartbeat is enabled (FR-55).
class Rf433Link : public BoilerLink {
public:
    Rf433Link();
    void begin(uint8_t pin) override;
    SendResult send(bool on, bool force = false) override;
    void tick(uint32_t nowMs) override;
    bool resend() override;
    bool isFailSafe() const override { return _heartbeatEnabled; }
    LinkHealth health() const override { return LinkHealth::Unknown; }
    bool lastState() const override { return _lastState; }
    uint32_t lastTxMs() const override { return _lastTxMs; }
    BoilerLinkType type() const override { return BoilerLinkType::Rf433; }
    const char* typeName() const override { return "rf433"; }

    void setHeartbeatEnabled(bool en) { _heartbeatEnabled = en; }
    bool isHeartbeatEnabled() const { return _heartbeatEnabled; }
    void setHeartbeatIntervalMs(uint32_t ms) { _heartbeatIntervalMs = ms; }
    uint32_t getHeartbeatIntervalMs() const { return _heartbeatIntervalMs; }

private:
    void transmitBurst(bool on);
    uint8_t _pin;
    bool _lastState;
    bool _hasTransmitted;
    uint32_t _lastTxMs;
    uint32_t _heartbeatIntervalMs;
    bool _heartbeatEnabled;
};

// ContactLink: dry Normally-Open (NO) relay contact on the controller.
// Inherently fail-safe: power loss opens contact -> boiler turns OFF (FR-55).
class ContactLink : public BoilerLink {
public:
    ContactLink();
    void begin(uint8_t pin) override;
    SendResult send(bool on, bool force = false) override;
    void tick(uint32_t nowMs) override;
    bool resend() override;
    bool isFailSafe() const override { return true; }
    LinkHealth health() const override { return LinkHealth::Ok; }
    bool lastState() const override { return _lastState; }
    uint32_t lastTxMs() const override { return _lastTxMs; }
    BoilerLinkType type() const override { return BoilerLinkType::Wired; }
    const char* typeName() const override { return "wired"; }

private:
    uint8_t _pin;
    bool _lastState;
    bool _hasTransmitted;
    uint32_t _lastTxMs;
};

#endif // BOILER_LINK_H
