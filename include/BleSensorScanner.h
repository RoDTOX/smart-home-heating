#ifndef BLE_SENSOR_SCANNER_H
#define BLE_SENSOR_SCANNER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "Config.h"

// FR-11.5, FR-11.6: a place for the sensor of every zone (32) and for the sensors a search lists (32)
#define MAX_BLE_SENSORS 64
#define BLE_SEARCH_MAX_UNASSIGNED 32
#define BLE_SEARCH_MAX_MS (10UL * 60UL * 1000UL)  // a search ends after 10 min (FR-11.5)

// true when a zone uses this sensor (by MAC or by alias)
typedef bool (*SensorAssignedFn)(const char* mac, const char* name);

enum SensorSource {
    SRC_NONE = 0,
    SRC_BLE_DIRECT
};

struct BleSensorData {
    char mac[18];
    char name[32];
    float temperature;
    float humidity;
    uint8_t battery;
    int8_t rssi;
    int8_t minRssi;
    int8_t maxRssi;
    uint32_t lastSeenMs;
    uint32_t packetCount;
    SensorSource source;
    bool valid;
    // SensorHub plausibility state (FR-53)
    float pending;            // jump held until the next reading confirms it
    uint32_t discardWindowMs; // start of the 1 h window of discarded readings
    uint32_t lastDiscardMs;
    uint8_t discardCount;
    bool pendValid;
    bool suspect;             // DTC-104 raised for this sensor
    bool outOfBounds;         // last reading outside validMin..validMax (DTC-103)
};

class BleSensorScanner : public NimBLEScanCallbacks {
public:
    BleSensorScanner();

    void begin();
    void update();

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;

    size_t getSensorCount() const { return _sensorCount; }
    const BleSensorData* getSensor(size_t index) const;
    const BleSensorData* findSensorByName(const char* name) const;
    const BleSensorData* findSensorByMac(const char* mac) const;
    float getTemperatureByName(const char* name, float fallback = -999.0f) const;

    // Reading hook of the development build and the system tests (POST /api/dev/sensor)
    void ingestExternalReading(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi);

    // Sensor search (FR-11.5): outside a search only the sensors assigned to zones are kept (FR-11.6)
    void setAssignedFn(SensorAssignedFn fn) { _assigned = fn; }
    void startSearch();
    void stopSearch();
    bool isSearching() const { return _searching; }
    uint32_t searchRemainingSec() const;
    void prune();  // outside a search: drops the sensors no zone uses (FR-11.6)
    bool isAssignedSensor(const BleSensorData* s) const { return isAssigned(s->mac, s->name); }

    bool isScanning() const { return _isScanning; }
    void restartScan();

    const char* getSourceStr(SensorSource src) const;

private:
    NimBLEScan* _pBLEScan;
    BleSensorData _sensors[MAX_BLE_SENSORS];
    size_t _sensorCount;
    bool _isScanning;
    uint32_t _lastScanCheckMs;
    SensorAssignedFn _assigned = nullptr;
    bool _searching = false;
    uint32_t _searchStartMs = 0;
    uint32_t _lastPruneMs = 0;
    uint32_t _lastSuspectCheckMs = 0;
    bool noteDiscard(BleSensorData* s);
    // DTC work is handed from the BLE task to update() in loop() (the DTC and log rings are not thread-safe)
    volatile bool _raise103 = false, _raise104 = false;
    bool _dtc103Active = false, _dtc104Active = false;
    char _evtName[32] = "";
    float _evtTemp = 0;
    bool isAssigned(const char* mac, const char* name) const { return _assigned && _assigned(mac, name); }

    void updateOrAddSensor(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi, SensorSource src);
    const char* resolveNameForMac(const char* mac, const char* advName, char* outBuf, size_t bufLen);
    bool parsePayload(const uint8_t* payload, size_t length, float& outTemp, float& outHum, uint8_t& outBat);
};

extern BleSensorScanner bleScanner;

#endif // BLE_SENSOR_SCANNER_H
