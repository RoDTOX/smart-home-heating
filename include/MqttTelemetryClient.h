#ifndef MQTT_TELEMETRY_CLIENT_H
#define MQTT_TELEMETRY_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Config.h"

#define MAX_TRACKED_SENSORS 8

struct RoomSensor {
    char mac[18];
    char name[32];
    float temperature;
    float humidity;
    uint8_t battery;
    int8_t rssi;
    uint32_t lastSeenMs;
    bool valid;
};

class MqttTelemetryClient {
public:
    MqttTelemetryClient();

    void begin(const char* brokerHost = MQTT_BROKER_HOST, uint16_t brokerPort = MQTT_BROKER_PORT);
    void update();

    bool isConnected();
    size_t getSensorCount() const { return _sensorCount; }
    const RoomSensor* getSensor(size_t index) const;
    const RoomSensor* findSensorByName(const char* name) const;
    const RoomSensor* findSensorByMac(const char* mac) const;
    float getTemperatureByName(const char* name, float fallback = -999.0f) const;

private:
    WiFiClient _wifiClient;
    PubSubClient _mqtt;
    const char* _brokerHost;
    uint16_t _brokerPort;
    uint32_t _lastReconnectAttempt;

    RoomSensor _sensors[MAX_TRACKED_SENSORS];
    size_t _sensorCount;

    void reconnect();
    void onMessage(char* topic, byte* payload, unsigned int length);
    static void staticCallback(char* topic, byte* payload, unsigned int length);
};

extern MqttTelemetryClient mqttTelemetry;

#endif // MQTT_TELEMETRY_CLIENT_H
