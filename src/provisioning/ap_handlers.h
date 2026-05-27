#ifndef AP_HANDLERS_H
#define AP_HANDLERS_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ===============================
// PROVISIONING API HANDLERS
// ===============================

/**
 * Handle configuration submission
 * POST /api/configure
 * 
 * Request body (JSON):
 * {
 *   "device_name": "water-pump-01",
 *   "wifi_ssid": "MyNetwork",
 *   "wifi_password": "password123",
 *   "admin_password": "admin123",
 *   "vps_token": "token123",
 *   "vps_url": "https://vps.example.com"
 * }
 * 
 * Response (JSON):
 * {
 *   "success": true/false,
 *   "message": "Configuration saved" / error message
 * }
 * 
 * @param request AsyncWebServerRequest
 * @param json Parsed JSON body
 */
void handleProvConfig(AsyncWebServerRequest *request);
void handleConfigureSubmit(AsyncWebServerRequest *request, JsonVariant &json);

#endif