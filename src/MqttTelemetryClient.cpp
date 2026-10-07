#include "MqttTelemetryClient.h"
#include "NetworkConsole.h"

static MqttTelemetryClient* _globalInstance = nullptr;

MqttTelemetryClient mqttTelemetry;

MqttTelemetryClient::MqttTelemetryClient()
    : _mqtt(_wifiClient),
      _brokerHost(MQTT_BROKER_HOST),
      _brokerPort(MQTT_BROKER_PORT),
      _lastReconnectAttempt(0),
      _sensorCount(0) {
    _globalInstance = this;
    for (size_t i = 0; i < MAX_TRACKED_SENSORS; i++) {
        _sensors[i].valid = false;
        _sensors[i].mac[0] = '\0';
        _sensors[i].name[0] = '\0';
        _sensors[i].temperature = 0.0f;
        _sensors[i].humidity = 0.0f;
        _sensors[i].battery = 0;
        _sensors[i].rssi = 0;
        _sensors[i].lastSeenMs = 0;
    }
}

void MqttTelemetryClient::begin(const char* brokerHost, uint16_t brokerPort) {
    _brokerHost = brokerHost;
    _brokerPort = brokerPort;
    _mqtt.setServer(_brokerHost, _brokerPort);
    _mqtt.setCallback(staticCallback);
    _mqtt.setBufferSize(512);

    netConsole.printf("[MQTT] Initialized client for broker %s:%d\n", _brokerHost, _brokerPort);
}

void MqttTelemetryClient::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (_globalInstance) {
        _globalInstance->onMessage(topic, payload, length);
    }
}

void MqttTelemetryClient::update() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!_mqtt.connected()) {
        uint32_t now = millis();
        if (now - _lastReconnectAttempt > 5000) {
            _lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        _mqtt.loop();
    }
}

bool MqttTelemetryClient::isConnected() {
    return _mqtt.connected();
}

void MqttTelemetryClient::reconnect() {
    if (_mqtt.connected()) return;

    netConsole.printf("[MQTT] Connecting to broker %s:%d...\n", _brokerHost, _brokerPort);
    String clientId = "ESP32-SmartHeating-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (_mqtt.connect(clientId.c_str())) {
        netConsole.println(F("[MQTT] Connected successfully to Mosquitto broker!"));
        _mqtt.subscribe(MQTT_TOPIC_SUB);
        netConsole.printf("[MQTT] Subscribed to topic '%s'\n", MQTT_TOPIC_SUB);
    } else {
        netConsole.printf("[MQTT] Connection error (rc=%d). Retrying in 5s...\n", _mqtt.state());
    }
}

void MqttTelemetryClient::onMessage(char* topic, byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        netConsole.printf("[MQTT] JSON parse error on topic %s: %s\n", topic, err.c_str());
        return;
    }

    const char* mac = doc["mac"] | "";
    const char* name = doc["name"] | "";
    if (strlen(mac) == 0 && strlen(name) == 0) {
        return;
    }

    float temp = doc["temperature"] | -999.0f;
    float hum = doc["humidity"] | -999.0f;
    uint8_t bat = doc["battery"] | 0;
    int8_t rssi = doc["rssi"] | -70;

    // Căutare senzor existent după MAC sau Nume
    RoomSensor* target = nullptr;
    for (size_t i = 0; i < _sensorCount; i++) {
        if (strlen(mac) > 0 && strcasecmp(_sensors[i].mac, mac) == 0) {
            target = &_sensors[i];
            break;
        }
        if (strlen(name) > 0 && strcasecmp(_sensors[i].name, name) == 0) {
            target = &_sensors[i];
            break;
        }
    }

    // Dacă nu a fost găsit și avem loc în tabel, adăugăm senzor nou
    if (!target && _sensorCount < MAX_TRACKED_SENSORS) {
        target = &_sensors[_sensorCount++];
        target->mac[0] = '\0';
        target->name[0] = '\0';
    }

    if (target) {
        if (strlen(mac) > 0) strncpy(target->mac, mac, sizeof(target->mac) - 1);
        if (strlen(name) > 0) strncpy(target->name, name, sizeof(target->name) - 1);
        target->temperature = temp;
        target->humidity = hum;
        target->battery = bat;
        target->rssi = rssi;
        target->lastSeenMs = millis();
        target->valid = true;

        netConsole.printf("[Telemetry] %s (%s): %.1f C | %.1f %% | Bat: %d %% | RSSI: %d dBm\n",
            target->name, target->mac, target->temperature, target->humidity, target->battery, target->rssi);
    }
}

const RoomSensor* MqttTelemetryClient::getSensor(size_t index) const {
    if (index < _sensorCount && _sensors[index].valid) {
        return &_sensors[index];
    }
    return nullptr;
}

const RoomSensor* MqttTelemetryClient::findSensorByName(const char* name) const {
    for (size_t i = 0; i < _sensorCount; i++) {
        if (_sensors[i].valid && strcasecmp(_sensors[i].name, name) == 0) {
            return &_sensors[i];
        }
    }
    return nullptr;
}

const RoomSensor* MqttTelemetryClient::findSensorByMac(const char* mac) const {
    for (size_t i = 0; i < _sensorCount; i++) {
        if (_sensors[i].valid && strcasecmp(_sensors[i].mac, mac) == 0) {
            return &_sensors[i];
        }
    }
    return nullptr;
}

float MqttTelemetryClient::getTemperatureByName(const char* name, float fallback) const {
    const RoomSensor* s = findSensorByName(name);
    if (s && s->valid) {
        return s->temperature;
    }
    return fallback;
}
