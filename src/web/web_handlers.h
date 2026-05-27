

#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <ESPAsyncWebServer.h>
#include "../hardware/water_sensors.h"    // dla readWaterSensor1/2()
#include "../algorithm/water_algorithm.h" // dla waterAlgorithm

// Page handlers
void handleDashboard(AsyncWebServerRequest *request);
void handleLoginPage(AsyncWebServerRequest *request);

// Authentication handlers
void handleLogin(AsyncWebServerRequest *request);
void handleLogout(AsyncWebServerRequest *request);

// API handlers
void handleStatus(AsyncWebServerRequest *request);
void handlePumpStop(AsyncWebServerRequest *request);

// Direct pump control (bypasses algorithm, system disable and daily limit)
void handleDirectPumpOn(AsyncWebServerRequest *request);
void handleDirectPumpOff(AsyncWebServerRequest *request);
void handlePumpSettings(AsyncWebServerRequest *request);

// ATO calibration pump (bistable, user-controlled, safety timeout 1h)
void handleCalibrationOn(AsyncWebServerRequest *request);
void handleCalibrationOff(AsyncWebServerRequest *request);

void handleSystemToggle(AsyncWebServerRequest *request);

// Statistics handlers
void handleResetStatistics(AsyncWebServerRequest *request);
void handleGetStatistics(AsyncWebServerRequest *request);

void handleGetDailyVolume(AsyncWebServerRequest *request);
void handleResetDailyVolume(AsyncWebServerRequest *request);

// 🆕 NEW: Fill Water Max / Dose endpoints
void handleGetFillWaterMax(AsyncWebServerRequest* request);
void handleSetFillWaterMax(AsyncWebServerRequest* request);  // sets daily_limit_ml
void handleSetDose(AsyncWebServerRequest* request);          // sets dose_ml

// Cycle History endpoints
void handleGetCycleHistory(AsyncWebServerRequest* request);
void handleClearCycleHistory(AsyncWebServerRequest* request);

// Kalkwasser endpoints
void handleKalkwasserConfig(AsyncWebServerRequest* request);
void handleKalkwasserCalibrate(AsyncWebServerRequest* request);
void handleKalkwasserFlowRate(AsyncWebServerRequest* request);
void handleMixingPumpDirect(AsyncWebServerRequest* request);
void handlePeristalticPumpDirect(AsyncWebServerRequest* request);

// System reset (works from any state except LOGGING)
void handleSystemReset(AsyncWebServerRequest *request);

// Alarm audio mute toggle
void handleAlarmToggle(AsyncWebServerRequest *request);

// Algorithm config (GET = current, POST = save all 7 params)
void handleAlgConfig(AsyncWebServerRequest *request);

// Alarm volume control (GET = current, POST ?volume=3-30)
void handleAudioVolume(AsyncWebServerRequest *request);

// Health check endpoint (no session required)
void handleHealth(AsyncWebServerRequest *request);

#endif