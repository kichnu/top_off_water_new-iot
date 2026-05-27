#ifndef PROV_CONFIG_H
#define PROV_CONFIG_H

#include <Arduino.h>

// ===============================
// PROVISIONING MODE CONFIGURATION
// ===============================

// Button Configuration
#define PROV_BUTTON_PIN         14       // GPIO5 (same as RESET_PIN)
#define PROV_BUTTON_HOLD_MS     5000    // Hold time to enter provisioning (5 seconds)
#define PROV_BUTTON_DEBOUNCE_MS 100     // Debounce time

// Audio feedback during button hold (DFPlayer Pro)
#define PROV_AUDIO_TRACK        6       // 006.mp3 — odliczanie + "system gotowy do konfiguracji"

// Access Point Configuration
#define PROV_AP_SSID            "ESP32-WATER-SETUP"
#define PROV_AP_PASSWORD        "setup12345"
#define PROV_AP_CHANNEL         6
#define PROV_AP_MAX_CLIENTS     4

// DNS Server Configuration
#define PROV_DNS_PORT           53

// Web Server Configuration
#define PROV_WEB_PORT           80

// Test Configuration
#define PROV_WIFI_TIMEOUT_MS    10000   // WiFi connection timeout per attempt
#define PROV_WIFI_RETRY_COUNT   3       // Number of retry attempts
#define PROV_DNS_TIMEOUT_MS     10000   // DNS resolution timeout
#define PROV_HTTP_TIMEOUT_MS    10000   // HTTP request timeout
#define PROV_WS_TIMEOUT_MS      15000   // WebSocket handshake timeout

// Log Buffer Configuration
#define PROV_LOG_BUFFER_SIZE    50      // Max log entries in RAM

// Session Configuration
#define PROV_SESSION_TIMEOUT_MS 600000  // 10 minutes idle timeout

#endif