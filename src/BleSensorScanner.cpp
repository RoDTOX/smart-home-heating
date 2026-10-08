#include "BleSensorScanner.h"
#include "NetworkConsole.h"
#include "MqttTelemetryClient.h"

BleSensorScanner bleScanner;

BleSensorScanner::BleSensorScanner()
    : _pBLEScan(nullptr),
      _sensorCount(0),
      _isScanning(false),
      _lastScanCheckMs(0) {
    for (size_t i = 0; i < MAX_BLE_SENSORS; i++) {
        _sensors[i].mac[0] = '\0';
        _sensors[i].name[0] = '\0';
        _sensors[i].temperature = -999.0f;
        _sensors[i].humidity = -999.0f;
        _sensors[i].battery = 0;
        _sensors[i].rssi = 0;
        _sensors[i].lastSeenMs = 0;
        _sensors[i].packetCount = 0;
        _sensors[i].source = SRC_NONE;
        _sensors[i].valid = false;
    }
}

void BleSensorScanner::begin() {
    netConsole.println(F("[BLE] Initializing ESP32 NimBLE stack (passive observer)..."));
    NimBLEDevice::init("ESP32-SmartHeating");

    _pBLEScan = NimBLEDevice::getScan();
    if (!_pBLEScan) {
        netConsole.println(F("[BLE] ERROR: Failed to acquire NimBLEScan instance!"));
        return;
    }

    _pBLEScan->setScanCallbacks(this, true); // true = wantDuplicates for continuous updates
    _pBLEScan->setActiveScan(false);         // Passive scan: zero RF emissions, zero battery impact on sensors
    _pBLEScan->setInterval(BLE_SCAN_INTERVAL_MS);
    _pBLEScan->setWindow(BLE_SCAN_WINDOW_MS);

    restartScan();
}

void BleSensorScanner::restartScan() {
    if (!_pBLEScan) return;
    
    _pBLEScan->stop();
    delay(20);

    // start(durationSeconds, isContinue, restart) -> 0 = scan indefinitely
    bool started = _pBLEScan->start(0, false, true);
    _isScanning = started;
    _lastScanCheckMs = millis();

    if (started) {
        netConsole.println(F("[BLE] Continuous passive BLE scanner active (listening for BTHome v2 & PVVX)..."));
    } else {
        netConsole.println(F("[BLE] WARNING: BLE scan failed to start. Will retry in update loop."));
    }
}

void BleSensorScanner::update() {
    uint32_t now = millis();
    if (now - _lastScanCheckMs >= 30000UL) {
        _lastScanCheckMs = now;
        // Watchdog check: ensure scan is alive
        if (!_isScanning || !_pBLEScan->isScanning()) {
            netConsole.println(F("[BLE] Watchdog detected inactive scanner. Restarting scan..."));
            restartScan();
        }
    }
}

const char* BleSensorScanner::resolveNameForMac(const char* mac, const char* advName, char* outBuf, size_t bufLen) {
    if (!mac || bufLen == 0) return "";

    // 1. Check known devices from Config.h
    for (size_t i = 0; i < KNOWN_BLE_DEVICES_COUNT; i++) {
        if (strcasecmp(KNOWN_BLE_DEVICES[i].mac, mac) == 0) {
            strncpy(outBuf, KNOWN_BLE_DEVICES[i].name, bufLen - 1);
            outBuf[bufLen - 1] = '\0';
            return outBuf;
        }
    }

    // 2. Check if advertised name has useful text (e.g. "ATC_84A1E8")
    if (advName && strlen(advName) > 0 && strncasecmp(advName, "ESP", 3) != 0) {
        strncpy(outBuf, advName, bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return outBuf;
    }

    // 3. Fallback: generate friendly name from last 4 characters of MAC
    size_t macLen = strlen(mac);
    if (macLen >= 5) {
        snprintf(outBuf, bufLen, "BLE-%c%c%c%c",
                 mac[macLen - 5], mac[macLen - 4], mac[macLen - 2], mac[macLen - 1]);
    } else {
        snprintf(outBuf, bufLen, "BLE-Sensor");
    }
    return outBuf;
}

bool BleSensorScanner::parsePayload(const uint8_t* payload, size_t length, float& outTemp, float& outHum, uint8_t& outBat) {
    if (!payload || length < 4) return false;

    size_t idx = 0;
    while (idx < length) {
        uint8_t adLen = payload[idx];
        if (adLen == 0 || idx + adLen + 1 > length) break;
        uint8_t adType = payload[idx + 1];
        const uint8_t* adData = &payload[idx + 2];
        size_t adDataLen = adLen - 1;

        // 0x16 = Service Data (16-bit UUID)
        if (adType == 0x16 && adDataLen >= 3) {
            uint16_t uuid = (uint16_t)(adData[0] | (adData[1] << 8));

            // 1. PVVX Custom Format (UUID 0x181A)
            if (uuid == 0x181A && adDataLen >= 15) {
                int16_t temp_raw = (int16_t)(adData[8] | (adData[9] << 8));
                uint16_t hum_raw = (uint16_t)(adData[10] | (adData[11] << 8));
                outTemp = temp_raw / 100.0f;
                outHum = hum_raw / 100.0f;
                outBat = adData[14];
                return true;
            }

            // 2. BTHome v2 (UUID 0xFCD2)
            if (uuid == 0xFCD2 && adDataLen >= 4) {
                size_t bi = 3; // Skip 2 bytes UUID + 1 byte BTHome header
                bool gotTemp = false;
                bool gotHum = false;
                uint8_t bat = 100;
                while (bi < adDataLen) {
                    uint8_t objId = adData[bi++];
                    if (objId == 0x01 && bi < adDataLen) {
                        bat = adData[bi++];
                    } else if (objId == 0x02 && bi + 1 < adDataLen) {
                        int16_t t_raw = (int16_t)(adData[bi] | (adData[bi + 1] << 8));
                        outTemp = t_raw * 0.01f;
                        bi += 2;
                        gotTemp = true;
                    } else if (objId == 0x03 && bi + 1 < adDataLen) {
                        uint16_t h_raw = (uint16_t)(adData[bi] | (adData[bi + 1] << 8));
                        outHum = h_raw * 0.01f;
                        bi += 2;
                        gotHum = true;
                    } else if (objId == 0x0C && bi + 1 < adDataLen) {
                        bi += 2; // Voltage in mV
                    } else if (objId == 0x00 && bi < adDataLen) {
                        bi += 1; // Packet counter
                    } else {
                        break;
                    }
                }
                if (gotTemp && gotHum) {
                    outBat = bat;
                    return true;
                }
            }
        }
        idx += adLen + 1;
    }
    return false;
}

void BleSensorScanner::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!advertisedDevice) return;

    std::string macStd = advertisedDevice->getAddress().toString();
    char mac[18];
    strncpy(mac, macStd.c_str(), sizeof(mac) - 1);
    mac[sizeof(mac) - 1] = '\0';
    for (size_t i = 0; mac[i]; i++) {
        mac[i] = toupper((unsigned char)mac[i]);
    }

    int rssi = advertisedDevice->getRSSI();
    std::string advNameStd = advertisedDevice->getName();

    float temp = -999.0f;
    float hum = -999.0f;
    uint8_t bat = 0;
    bool decoded = false;

    // 1. Try parsing service data via UUID directly (NimBLE convenience methods)
    if (advertisedDevice->haveServiceData()) {
        // Check PVVX (0x181A)
        std::string pvvxData = advertisedDevice->getServiceData(NimBLEUUID((uint16_t)0x181A));
        if (pvvxData.size() >= 13) {
            const uint8_t* bytes = (const uint8_t*)pvvxData.data();
            int16_t temp_raw = (int16_t)(bytes[6] | (bytes[7] << 8));
            uint16_t hum_raw = (uint16_t)(bytes[8] | (bytes[9] << 8));
            temp = temp_raw / 100.0f;
            hum = hum_raw / 100.0f;
            bat = bytes[12];
            decoded = true;
        }

        // Check BTHome v2 (0xFCD2)
        if (!decoded) {
            std::string bthomeData = advertisedDevice->getServiceData(NimBLEUUID((uint16_t)0xFCD2));
            if (bthomeData.size() >= 2) {
                const uint8_t* bytes = (const uint8_t*)bthomeData.data();
                size_t len = bthomeData.size();
                size_t bi = 1; // Skip BTHome header byte
                bool gotTemp = false;
                bool gotHum = false;
                uint8_t b = 100;
                while (bi < len) {
                    uint8_t objId = bytes[bi++];
                    if (objId == 0x01 && bi < len) {
                        b = bytes[bi++];
                    } else if (objId == 0x02 && bi + 1 < len) {
                        int16_t t_raw = (int16_t)(bytes[bi] | (bytes[bi + 1] << 8));
                        temp = t_raw * 0.01f;
                        bi += 2;
                        gotTemp = true;
                    } else if (objId == 0x03 && bi + 1 < len) {
                        uint16_t h_raw = (uint16_t)(bytes[bi] | (bytes[bi + 1] << 8));
                        hum = h_raw * 0.01f;
                        bi += 2;
                        gotHum = true;
                    } else if (objId == 0x0C && bi + 1 < len) {
                        bi += 2;
                    } else if (objId == 0x00 && bi < len) {
                        bi += 1;
                    } else {
                        break;
                    }
                }
                if (gotTemp && gotHum) {
                    bat = b;
                    decoded = true;
                }
            }
        }
    }

    // 2. Fallback: Parse raw payload vector if service data lookup didn't decode
    if (!decoded) {
        const std::vector<uint8_t>& rawPayload = advertisedDevice->getPayload();
        if (!rawPayload.empty()) {
            decoded = parsePayload(rawPayload.data(), rawPayload.size(), temp, hum, bat);
        }
    }

    if (!decoded) {
        return; // Not a supported sensor advertisement
    }

    // Filter spurious out-of-range sensor spikes
    if (temp < -40.0f || temp > 80.0f || hum < 0.0f || hum > 100.0f) {
        return;
    }

    char nameBuf[32];
    resolveNameForMac(mac, advNameStd.c_str(), nameBuf, sizeof(nameBuf));

    updateOrAddSensor(mac, nameBuf, temp, hum, bat, (int8_t)rssi, SRC_BLE_DIRECT);
}

void BleSensorScanner::updateOrAddSensor(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi, SensorSource src) {
    BleSensorData* target = nullptr;

    for (size_t i = 0; i < _sensorCount; i++) {
        if (strcasecmp(_sensors[i].mac, mac) == 0) {
            target = &_sensors[i];
            break;
        }
    }

    if (!target && _sensorCount < MAX_BLE_SENSORS) {
        target = &_sensors[_sensorCount++];
        strncpy(target->mac, mac, sizeof(target->mac) - 1);
        target->mac[sizeof(target->mac) - 1] = '\0';
        target->packetCount = 0;
    }

    if (target) {
        strncpy(target->name, name, sizeof(target->name) - 1);
        target->name[sizeof(target->name) - 1] = '\0';
        target->temperature = temp;
        target->humidity = hum;
        target->battery = bat;
        target->rssi = rssi;
        target->lastSeenMs = millis();
        target->packetCount++;
        target->source = src;
        target->valid = true;

        netConsole.printf("[BLE] %s (%s): %.1f C | %.1f %% | Bat: %d%% | RSSI: %d dBm | Src: %s\n",
                          target->name, target->mac, target->temperature, target->humidity,
                          target->battery, target->rssi, getSourceStr(src));

        // Mirror directly to MqttTelemetryClient so existing thermal loops and API keep running seamlessly
        mqttTelemetry.updateSensorReading(mac, name, temp, hum, bat, rssi);
    }
}

void BleSensorScanner::ingestExternalReading(const char* mac, const char* name, float temp, float hum, uint8_t bat, int8_t rssi) {
    if (!mac || strlen(mac) == 0) return;

    // Check if we recently heard this sensor directly via BLE (within 60s)
    for (size_t i = 0; i < _sensorCount; i++) {
        if (strcasecmp(_sensors[i].mac, mac) == 0) {
            if (_sensors[i].source == SRC_BLE_DIRECT && (millis() - _sensors[i].lastSeenMs < 60000UL)) {
                // Direct BLE is preferred; do not downgrade timestamp
                return;
            }
            break;
        }
    }

    char nameBuf[32];
    resolveNameForMac(mac, name, nameBuf, sizeof(nameBuf));
    updateOrAddSensor(mac, nameBuf, temp, hum, bat, rssi, SRC_MQTT_GATEWAY);
}

const BleSensorData* BleSensorScanner::getSensor(size_t index) const {
    if (index < _sensorCount && _sensors[index].valid) {
        return &_sensors[index];
    }
    return nullptr;
}

const BleSensorData* BleSensorScanner::findSensorByName(const char* name) const {
    for (size_t i = 0; i < _sensorCount; i++) {
        if (_sensors[i].valid && strcasecmp(_sensors[i].name, name) == 0) {
            return &_sensors[i];
        }
    }
    return nullptr;
}

const BleSensorData* BleSensorScanner::findSensorByMac(const char* mac) const {
    for (size_t i = 0; i < _sensorCount; i++) {
        if (_sensors[i].valid && strcasecmp(_sensors[i].mac, mac) == 0) {
            return &_sensors[i];
        }
    }
    return nullptr;
}

float BleSensorScanner::getTemperatureByName(const char* name, float fallback) const {
    const BleSensorData* s = findSensorByName(name);
    if (s && s->valid) {
        return s->temperature;
    }
    return fallback;
}

const char* BleSensorScanner::getSourceStr(SensorSource src) const {
    switch (src) {
        case SRC_BLE_DIRECT:   return "BLE-Direct (ESP32)";
        case SRC_MQTT_GATEWAY: return "MQTT-Gateway (A6)";
        default:               return "None";
    }
}
