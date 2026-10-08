#ifndef DEVICE_PASSPORT_H
#define DEVICE_PASSPORT_H

#include <Arduino.h>
#include <Preferences.h>

class DevicePassport {
public:
    DevicePassport();

    void begin();
    void load();
    void save();

    // Commissioning
    void commission(const char* customerName, const char* customerAddress, const char* notes, const char* city, const char* coords);
    void updateNotes(const char* notes);

    // Getters
    const char* getSerialNumber() const { return _serialNumber; }
    bool isCommissioned() const { return _isCommissioned; }
    uint32_t getCommissionTimestamp() const { return _commissionTimestamp; }
    const char* getCommissionDate() const { return _commissionDate; }
    const char* getCustomerName() const { return _customerName; }
    const char* getCustomerAddress() const { return _customerAddress; }
    const char* getInstallerNotes() const { return _installerNotes; }
    const char* getLocationCity() const { return _locationCity; }
    const char* getLocationCoords() const { return _locationCoords; }
    uint32_t getInstallationAgeDays() const;

    String toJson() const;

private:
    Preferences _prefs;
    char _serialNumber[24];
    bool _isCommissioned;
    uint32_t _commissionTimestamp;
    char _commissionDate[32];
    char _customerName[48];
    char _customerAddress[64];
    char _installerNotes[96];
    char _locationCity[32];
    char _locationCoords[32];

    void generateDefaultSerialNumber();
};

extern DevicePassport devicePassport;

#endif // DEVICE_PASSPORT_H
