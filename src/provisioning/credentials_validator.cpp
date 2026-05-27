#include "credentials_validator.h"

ValidationResult prov_validateDeviceName(const String& name) {
    ValidationResult result;
    result.valid = true;
    result.errorField = "device_name";
    
    if (name.length() < 3 || name.length() > 32) {
        result.valid = false;
        result.errorMessage = "Device name must be 3-32 characters";
        return result;
    }
    
    for (size_t i = 0; i < name.length(); i++) {
        char c = name.charAt(i);
        if (!isalnum(c) && c != '_' && c != '-') {
            result.valid = false;
            result.errorMessage = "Device name can only contain letters, numbers, underscore, hyphen";
            return result;
        }
    }
    
    return result;
}

ValidationResult prov_validateWiFiSSID(const String& ssid) {
    ValidationResult result;
    result.valid = true;
    result.errorField = "wifi_ssid";
    
    if (ssid.length() == 0) {
        result.valid = false;
        result.errorMessage = "WiFi SSID is required";
        return result;
    }
    
    if (ssid.length() > 32) {
        result.valid = false;
        result.errorMessage = "WiFi SSID too long (max 32 characters)";
        return result;
    }
    
    return result;
}

ValidationResult prov_validateWiFiPassword(const String& password) {
    ValidationResult result;
    result.valid = true;
    result.errorField = "wifi_password";
    
    if (password.length() < 8) {
        result.valid = false;
        result.errorMessage = "WiFi password must be at least 8 characters (WPA2 requirement)";
        return result;
    }
    
    if (password.length() > 64) {
        result.valid = false;
        result.errorMessage = "WiFi password too long (max 64 characters)";
        return result;
    }
    
    return result;
}

ValidationResult prov_validateAdminPassword(const String& password) {
    ValidationResult result;
    result.valid = true;
    result.errorField = "admin_password";
    
    if (password.length() < 8) {
        result.valid = false;
        result.errorMessage = "Admin password must be at least 8 characters";
        return result;
    }
    
    if (password.length() > 64) {
        result.valid = false;
        result.errorMessage = "Admin password too long (max 64 characters)";
        return result;
    }
    
    return result;
}

ValidationResult prov_validateAllCredentials(
    const String& deviceName,
    const String& wifiSSID,
    const String& wifiPassword,
    const String& adminPassword
) {
    ValidationResult result;

    result = prov_validateDeviceName(deviceName);
    if (!result.valid) return result;

    result = prov_validateWiFiSSID(wifiSSID);
    if (!result.valid) return result;

    if (wifiPassword.length() > 0) {
        result = prov_validateWiFiPassword(wifiPassword);
        if (!result.valid) return result;
    }

    if (adminPassword.length() > 0) {
        result = prov_validateAdminPassword(adminPassword);
        if (!result.valid) return result;
    }

    result.valid = true;
    result.errorField = "";
    result.errorMessage = "";
    return result;
}