#ifndef CREDENTIALS_VALIDATOR_H
#define CREDENTIALS_VALIDATOR_H

#include <Arduino.h>

// ===============================
// CREDENTIALS VALIDATION
// (Provisioning-specific - different from fram_encryption validators)
// ===============================

/**
 * Validation result structure
 */
struct ValidationResult {
    bool valid;
    String errorField;
    String errorMessage;
};

/**
 * Validate device name for provisioning
 * Pattern: [a-zA-Z0-9_-]{3,32}
 */
ValidationResult prov_validateDeviceName(const String& name);

/**
 * Validate WiFi SSID for provisioning
 * Length: 1-32 characters
 */
ValidationResult prov_validateWiFiSSID(const String& ssid);

/**
 * Validate WiFi password for provisioning
 * Length: 8-64 characters (WPA2 requirement)
 */
ValidationResult prov_validateWiFiPassword(const String& password);

/**
 * Validate admin password for provisioning
 * Length: 8-64 characters
 */
ValidationResult prov_validateAdminPassword(const String& password);

/**
 * Validate all credentials at once (passwords optional — empty = keep existing)
 * Returns first validation error found
 */
ValidationResult prov_validateAllCredentials(
    const String& deviceName,
    const String& wifiSSID,
    const String& wifiPassword,
    const String& adminPassword
);

#endif