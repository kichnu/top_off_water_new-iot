
#ifndef FRAM_CONSTANTS_H
#define FRAM_CONSTANTS_H

// ===============================
// SHARED FRAM CONSTANTS
// ===============================

// Magic number and version (shared between FRAM programmer and ESP32)
#define FRAM_MAGIC_NUMBER      0x57415452  // "WATR" in hex
#define FRAM_DATA_VERSION      0x0003      // 🆕 Version 3 (with VPS_URL support)

// FRAM Programmer encryption constants
#define ENCRYPTION_SALT "ESP32_FRAM_SALT_2024"
#define ENCRYPTION_SEED "WATER_SYSTEM_SEED_V1"

// Maximum field sizes for encrypted data
#define MAX_DEVICE_NAME_LEN 31
#define MAX_WIFI_SSID_LEN 63
#define MAX_WIFI_PASSWORD_LEN 127
#define MAX_VPS_TOKEN_LEN 255
#define MAX_VPS_URL_LEN 100        // 🆕 NEW: Maximum VPS URL length

#endif