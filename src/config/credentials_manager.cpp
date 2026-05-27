#include "credentials_manager.h"
#include "../core/logging.h"
#include "../hardware/fram_controller.h"
#include "../crypto/fram_encryption.h"
#include "config.h"  // For fallback hardcoded credentials

// Global dynamic credentials instance
DynamicCredentials dynamicCredentials;

bool initCredentialsManager() {
    LOG_INFO("");
    LOG_INFO("Initializing credentials manager...");
    
    // Initialize with empty values
    dynamicCredentials.wifi_ssid = "";
    dynamicCredentials.wifi_password = "";
    dynamicCredentials.admin_password_hash = "";
    dynamicCredentials.device_id = "";
    dynamicCredentials.loaded_from_fram = false;
    
    // Try to load from FRAM
    if (loadCredentialsFromFRAM()) {
        LOG_INFO("");
        LOG_INFO("✅ Credentials loaded from FRAM successfully");
        return true;
    } else {
        LOG_WARNING("");
        LOG_WARNING("⚠️ Could not load credentials from FRAM, using fallback");
        fallbackToHardcodedCredentials();
        return false;
    }
}

bool loadCredentialsFromFRAM() {
    LOG_INFO("Attempting to load credentials from FRAM...");
    
    // First verify that credentials exist and are valid
    if (!verifyCredentialsInFRAM()) {
        LOG_WARNING("");
        LOG_WARNING("No valid credentials found in FRAM");
        return false;
    }
    
    // Read encrypted credentials
    FRAMCredentials fram_creds;
    if (!readCredentialsFromFRAM(fram_creds)) {
        LOG_ERROR("");
        LOG_ERROR("Failed to read credentials from FRAM");
        return false;
    }
    
    // Check version compatibility - support v2 (without VPS_URL) and v3 (with VPS_URL)
    if (fram_creds.version != 0x0002 && fram_creds.version != 0x0003) {
        LOG_ERROR("");
        LOG_ERROR("Unsupported FRAM credentials version: %d", fram_creds.version);
        return false;
    }
    
    // Decrypt credentials
    DeviceCredentials creds;
    if (!decryptCredentials(fram_creds, creds)) {
        LOG_ERROR("");
        LOG_ERROR("Failed to decrypt credentials from FRAM");
        return false;
    }
    
    // Validate decrypted credentials
    if (creds.device_name.length() == 0 ||
        creds.wifi_ssid.length() == 0 ||
        creds.wifi_password.length() == 0 ||
        creds.admin_password.length() == 0) {
        LOG_ERROR("");
        LOG_ERROR("Decrypted credentials are incomplete");
        return false;
    }
    
    // Store in dynamic credentials
    dynamicCredentials.wifi_ssid = creds.wifi_ssid;
    dynamicCredentials.wifi_password = creds.wifi_password;
    dynamicCredentials.admin_password_hash = creds.admin_password;  // Already hashed
    dynamicCredentials.device_id = creds.device_name;
    dynamicCredentials.loaded_from_fram = true;
    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("🔐 Dynamic credentials loaded:");
    LOG_INFO("  Device ID: %s", dynamicCredentials.device_id.c_str());
    LOG_INFO("  WiFi SSID: %s", dynamicCredentials.wifi_ssid.c_str());
    LOG_INFO("  WiFi Password: ******* (%d chars)", dynamicCredentials.wifi_password.length());
    LOG_INFO("  Admin Hash: %s", dynamicCredentials.admin_password_hash.substring(0, 16).c_str());
    LOG_INFO("====================================");
    
    return true;
}

bool areCredentialsLoaded() {
    return dynamicCredentials.loaded_from_fram;
}

void fallbackToHardcodedCredentials() {
    LOG_WARNING("");
    LOG_WARNING("====================================");
    LOG_WARNING("🔄 Falling back to placeholder credentials");
    LOG_WARNING("⚠️ Web authentication will be DISABLED until FRAM is programmed!");
    LOG_WARNING("====================================");
    
    // Use placeholder credentials from config.h - NOT FUNCTIONAL!
    dynamicCredentials.wifi_ssid = String(WIFI_SSID);
    dynamicCredentials.wifi_password = String(WIFI_PASSWORD);
    dynamicCredentials.device_id = String(DEVICE_ID);
    dynamicCredentials.loaded_from_fram = false;
    
    // NO FALLBACK AUTHENTICATION - Force FRAM setup
    if (ADMIN_PASSWORD_HASH != nullptr) {
        dynamicCredentials.admin_password_hash = String(ADMIN_PASSWORD_HASH);
    } else {
        dynamicCredentials.admin_password_hash = "NO_AUTH_REQUIRES_FRAM_PROGRAMMING";
    }
    
    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("📌 Placeholder fallback credentials (NON-FUNCTIONAL):");
    LOG_INFO("  Device ID: %s", dynamicCredentials.device_id.c_str());
    LOG_INFO("  WiFi SSID: %s", dynamicCredentials.wifi_ssid.c_str());
    LOG_INFO("  Admin Hash: %s", dynamicCredentials.admin_password_hash.c_str());
    LOG_WARNING("🔧 Program FRAM credentials to enable web authentication!");
    LOG_INFO("====================================");
}

// Accessor functions for compatibility with existing code
const char* getWiFiSSID() {
    return dynamicCredentials.wifi_ssid.c_str();
}

const char* getWiFiPassword() {
    return dynamicCredentials.wifi_password.c_str();
}

const char* getAdminPasswordHash() {
    return dynamicCredentials.admin_password_hash.c_str();
}

const char* getDeviceID() {
    return dynamicCredentials.device_id.c_str();
}
