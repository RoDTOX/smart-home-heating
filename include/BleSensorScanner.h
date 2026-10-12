#ifndef BLE_SENSOR_SCANNER_H
#define BLE_SENSOR_SCANNER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "Config.h"

#define MAX_BLE_SENSORS 8

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

    void ingestExternalReading(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi);

    bool isScanning() const { return _isScanning; }
    void restartScan();

    const char* getSourceStr(SensorSource src) const;

private:
    NimBLEScan* _pBLEScan;
    BleSensorData _sensors[MAX_BLE_SENSORS];
    size_t _sensorCount;
    bool _isScanning;
    uint32_t _lastScanCheckMs;

    void updateOrAddSensor(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi, SensorSource src);
    const char* resolveNameForMac(const char* mac, const char* advName, char* outBuf, size_t bufLen);
    bool parsePayload(const uint8_t* payload, size_t length, float& outTemp, float& outHum, uint8_t& outBat);
};

extern BleSensorScanner bleScanner;

#endif // BLE_SENSOR_SCANNER_H
