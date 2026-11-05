#ifndef STATUS_MODEL_H
#define STATUS_MODEL_H

#include <stddef.h>
#include <stdint.h>

// L6 StatusModel (architecture.md §3.2, G29, AR-16):
// Formats system and zone telemetry JSON into a single static 4 KB buffer.
// Zero dynamic heap allocation, zero String churn.
class StatusModel {
public:
    static const size_t BUFFER_SIZE = 4096;

    StatusModel();

    // Builds the full /api/status JSON payload into the static buffer
    const char* buildStatusJson();

    // Builds the /api/zones JSON payload into the static buffer
    const char* buildZonesJson();

    // Returns pointer to the static buffer and current length
    const char* getBuffer() const { return _buf; }
    size_t getLength() const { return _len; }

private:
    char _buf[BUFFER_SIZE];
    size_t _len;

    // Helper append methods
    void reset();
    void append(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void appendStr(const char* s);
};

extern StatusModel statusModel;

#endif // STATUS_MODEL_H
