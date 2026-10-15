#include "DevicePassport.h"
#include "NetworkConsole.h"
#include <WiFi.h>
#include <time.h>

DevicePassport devicePassport;

DevicePassport::DevicePassport()
    : _isCommissioned(false),
      _commissionTimestamp(0) {
    _serialNumber[0] = '\0';
    _commissionDate[0] = '\0';
    _customerName[0] = '\0';
    _customerAddress[0] = '\0';
    _installerNotes[0] = '\0';
    _locationCity[0] = '\0';
    _locationCoords[0] = '\0';
}

void DevicePassport::generateDefaultSerialNumber() {
    uint64_t chipid = ESP.getEfuseMac();
    snprintf(_serialNumber, sizeof(_serialNumber), "SH-%04X%08X", 
             (uint16_t)(chipid >> 32), (uint32_t)chipid);
}

void DevicePassport::begin() {
    load();
    if (strlen(_serialNumber) == 0) {
        generateDefaultSerialNumber();
        save();
    }
    Serial.printf("[Passport] Serial: %s | Commissioned: %s | Customer: %s\n",
                  _serialNumber, _isCommissioned ? "YES" : "NO",
                  strlen(_customerName) > 0 ? _customerName : "Unassigned");
}

void DevicePassport::load() {
    if (!_prefs.begin("passport", true)) { // Read-only
        generateDefaultSerialNumber();
        return;
    }

    _prefs.getString("serial", _serialNumber, sizeof(_serialNumber));
    _isCommissioned = _prefs.getBool("comm", false);
    _commissionTimestamp = _prefs.getULong("comm_ts", 0);
    _prefs.getString("comm_dt", _commissionDate, sizeof(_commissionDate));
    _prefs.getString("cust_nm", _customerName, sizeof(_customerName));
    _prefs.getString("cust_addr", _customerAddress, sizeof(_customerAddress));
    _prefs.getString("notes", _installerNotes, sizeof(_installerNotes));
    _prefs.getString("city", _locationCity, sizeof(_locationCity));
    _prefs.getString("coords", _locationCoords, sizeof(_locationCoords));

    _prefs.end();

    if (strlen(_serialNumber) == 0) {
        generateDefaultSerialNumber();
    }
}

void DevicePassport::save() {
    if (!_prefs.begin("passport", false)) { // Read-write
        netConsole.println(F("[Passport] ERROR: Failed to open NVS namespace 'passport' for writing!"));
        return;
    }

    _prefs.putString("serial", _serialNumber);
    _prefs.putBool("comm", _isCommissioned);
    _prefs.putULong("comm_ts", _commissionTimestamp);
    _prefs.putString("comm_dt", _commissionDate);
    _prefs.putString("cust_nm", _customerName);
    _prefs.putString("cust_addr", _customerAddress);
    _prefs.putString("notes", _installerNotes);
    _prefs.putString("city", _locationCity);
    _prefs.putString("coords", _locationCoords);

    _prefs.end();
    netConsole.println(F("[Passport] Installation passport saved to non-volatile storage (NVS)."));
}

void DevicePassport::commission(const char* customerName, const char* customerAddress, const char* notes, const char* city, const char* coords) {
    _isCommissioned = true;
    
    // Acquire current time
    time_t now = time(nullptr);
    if (now > 1600000000) { // Valid NTP time
        _commissionTimestamp = (uint32_t)now;
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(_commissionDate, sizeof(_commissionDate), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        _commissionTimestamp = 0;
        strncpy(_commissionDate, "Pending NTP Sync", sizeof(_commissionDate) - 1);
    }

    if (customerName) strncpy(_customerName, customerName, sizeof(_customerName) - 1);
    if (customerAddress) strncpy(_customerAddress, customerAddress, sizeof(_customerAddress) - 1);
    if (notes) strncpy(_installerNotes, notes, sizeof(_installerNotes) - 1);
    if (city) strncpy(_locationCity, city, sizeof(_locationCity) - 1);
    if (coords) strncpy(_locationCoords, coords, sizeof(_locationCoords) - 1);

    save();
}

void DevicePassport::updateNotes(const char* notes) {
    if (notes) {
        strncpy(_installerNotes, notes, sizeof(_installerNotes) - 1);
        _installerNotes[sizeof(_installerNotes) - 1] = '\0';
        save();
    }
}

uint32_t DevicePassport::getInstallationAgeDays() const {
    if (!_isCommissioned || _commissionTimestamp == 0) return 0;
    time_t now = time(nullptr);
    if (now > _commissionTimestamp) {
        return (uint32_t)((now - _commissionTimestamp) / 86400UL);
    }
    return 0;
}

String DevicePassport::toJson() const {
    String json;
    json.reserve(512);
    json = "{";
    json += "\"serialNumber\":\"" + String(_serialNumber) + "\",";
    json += "\"isCommissioned\":" + String(_isCommissioned ? "true" : "false") + ",";
    json += "\"commissionTimestamp\":" + String(_commissionTimestamp) + ",";
    json += "\"commissionDate\":\"" + String(_commissionDate) + "\",";
    json += "\"customerName\":\"" + String(_customerName) + "\",";
    json += "\"customerAddress\":\"" + String(_customerAddress) + "\",";
    json += "\"installerNotes\":\"" + String(_installerNotes) + "\",";
    json += "\"locationCity\":\"" + String(_locationCity) + "\",";
    json += "\"locationCoords\":\"" + String(_locationCoords) + "\",";
    json += "\"ageDays\":" + String(getInstallationAgeDays()) + ",";
    json += "\"macAddress\":\"" + WiFi.macAddress() + "\"";
    json += "}";
    return json;
}
