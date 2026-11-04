#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <Arduino.h>

// FR-62.1: the event log of safety and I/O. add() only copies into a fixed RAM queue (safe in every decision path,
// no heap, no flash); update() appends the queue to LittleFS from loop().
enum EvType : uint8_t {
    EV_OUT = 1,     // pin level change: subject GPIO, zone, old / new level (255 = unknown)
    EV_RF,          // boiler frame on the 433 MHz line: old / new command (0 OFF, 1 ON)
    EV_REFUSED,     // boiler command not sent: reason, old = wait s, new = requested state
    EV_PENDING,     // pending boiler action: old / new 0 none, 1 OFF, 2 ON
    EV_INTERLOCK,   // zone: new 1 start (old = delay s), 2 end (old = elapsed s)
    EV_OVERRUN,     // new 1 start (old = overrun s), 2 end, 3 close of the last valve held
    EV_FROST,       // zone: old / new 0 or 1
    EV_SENSOR,      // zone: new 1 lost, 0 back
    EV_MODE,        // old / new 1 AUTO, 0 MANUAL
    EV_BOOT,        // new = reset reason
    EV_WARMUP,      // end of the boot warm-up: old = acquired, new = paired
    EV_TIME_SYNC,   // old / new = uptime s at that moment, high / low 16 bits
    EV_DTC,         // subject = Dtc index, zone, new 1 raised, 0 cleared
    EV_DROPPED,     // new = entries lost
    EV_TYPE_COUNT
};

enum EvReason : uint8_t {
    RS_NONE = 0, RS_BOOT, RS_DEMAND, RS_NO_DEMAND, RS_MANUAL, RS_FROST, RS_SENSOR_LOSS, RS_OVERRUN_END,
    RS_EXERCISE, RS_CONFIG, RS_SAFETY, RS_USER, RS_QUEUE, RS_HEARTBEAT, RS_GUARD_RUN, RS_GUARD_PAUSE,
    RS_GUARD_COOLDOWN, RS_INTERLOCK, RS_DEMAND_LOST, RS_PUMP_GUARD, RS_WIFI,
    RS_COUNT
};

// context byte of every entry: the boiler side at that moment
#define EVCTX_BOILER_ON   0x01
#define EVCTX_OVERRUN     0x02
#define EVCTX_PENDING_OFF 0x04
#define EVCTX_PENDING_ON  0x08
#define EVCTX_AUTO        0x10

struct EventEntry {      // 16 B
    uint32_t time;       // epoch seconds when flags bit 0 is set, else uptime seconds
    uint8_t type;        // EvType
    uint8_t subject;
    uint8_t reason;      // EvReason
    uint8_t zone;        // zone id, 0 = none
    uint8_t ctx;         // EVCTX_*
    uint8_t flags;       // bit 0: time is epoch
    uint16_t oldV;
    uint16_t newV;
    uint8_t pad[2];
};

typedef uint8_t (*EventContextFn)();
typedef void (*EventVisitFn)(const EventEntry &e, void *arg);

class EventLog {
public:
    void begin();    // mounts LittleFS, finds the file in use
    void update();   // loop(): append the queue when 8 entries wait or 30 s after the first
    void setContextFn(EventContextFn fn) { _ctxFn = fn; }

    // dedupe: the same entry again within 10 s is not written (a repeated refusal)
    void add(uint8_t type, uint8_t subject, uint8_t reason, uint8_t zone, uint16_t oldV, uint16_t newV,
             bool dedupe = false);

    size_t queued() const { return _qCount; }
    size_t stored() const { return _segCount + _otherCount; }
    // newest first: the queue, then the file in use, then the other file; at most n entries
    size_t forEachNewest(size_t n, EventVisitFn fn, void *arg);

    static const char *typeName(uint8_t type);
    static const char *reasonName(uint8_t reason);

private:
    static const uint8_t QUEUE_SIZE = 24;
    EventEntry _q[QUEUE_SIZE];
    uint8_t _qHead = 0;      // slot of the next entry
    uint8_t _qCount = 0;
    uint32_t _firstQueuedMs = 0;
    uint16_t _dropped = 0;
    EventContextFn _ctxFn = nullptr;
    bool _mounted = false;
    uint8_t _seg = 0;        // file in use
    uint32_t _seq = 0;       // header sequence of the file in use
    uint16_t _segCount = 0;
    uint16_t _otherCount = 0;
    bool _needNew = false;   // no usable file: the first write creates one
    uint32_t _lastSig = 0;
    uint32_t _lastSigMs = 0;
    bool _hasSig = false;

    bool readHeader(uint8_t seg, uint32_t &seq, uint16_t &count);
    bool rotate();
    void flush();
};

extern EventLog eventLog;

#endif
