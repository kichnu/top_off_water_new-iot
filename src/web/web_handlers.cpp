
#include "web_handlers.h"
#include "web_server.h"
#include "html_pages.h"
#include "../security/auth_manager.h"
#include "../security/session_manager.h"
#include "../security/rate_limiter.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/fram_controller.h"
#include "../hardware/rtc_controller.h"
#include "../network/wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <ArduinoJson.h>
#include "../config/credentials_manager.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/kalkwasser_scheduler.h"
#include "../hardware/peristaltic_pump.h"
#include "../hardware/mixing_pump.h"
#include "../hardware/audio_player.h"

void handleDashboard(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->redirect("login");
        return;
    }
    AsyncWebServerResponse* response = request->beginResponse(
        200, "text/html", (const uint8_t*)DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    request->send(response);
}

void handleLoginPage(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse(
        200, "text/html", (const uint8_t*)LOGIN_HTML, strlen(LOGIN_HTML));
    request->send(response);
}

void handleLogin(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    // Whitelist check - block login from unknown IPs (proxy allowed)
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        LOG_WARNING("Login attempt from non-whitelisted IP: %s", clientIP.toString().c_str());
        request->send(403, "application/json", "{\"success\":false,\"error\":\"Access denied\"}");
        return;
    }

    if (isRateLimited(clientIP) || isIPBlocked(clientIP)) {
        request->send(429, "text/plain", "Too Many Requests");
        return;
    }

    // Check if FRAM credentials are loaded
    if (!areCredentialsLoaded()) {
        recordFailedAttempt(clientIP);
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "System not configured";
        error_response["message"] = "FRAM credentials required. Use Captive Portal to configure.";
        error_response["setup_instructions"] = "1. Hold button 5s during boot  2. Connect to ESP32-WATER-SETUP  3. Configure credentials in browser";
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(503, "application/json", response_str);  // Service Unavailable
        return;
    }
 
    if (!request->hasParam("password", true)) {
        recordFailedAttempt(clientIP);
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing password\"}");
        return;
    }
    
    String password = request->getParam("password", true)->value();
    
    if (verifyPassword(password)) {
        String token = createSession(clientIP);
        String cookie = "session_token=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" + String(SESSION_TIMEOUT_MS / 1000);
        
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
        response->addHeader("Set-Cookie", cookie);
        request->send(response);
    } else {
        recordFailedAttempt(clientIP);
                
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "Invalid password";
        
        if (!areCredentialsLoaded()) {
            error_response["message"] = "System requires FRAM credential programming";
        }
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(401, "application/json", response_str);
    }
}

void handleLogout(AsyncWebServerRequest* request) {
    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            
            String token = cookie.substring(tokenStart, tokenEnd);
            destroySession(token);
        }
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
    response->addHeader("Set-Cookie", "session_token=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    request->send(response);
}

void handleStatus(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    JsonDocument json;
    
    // ============================================
    // HARDWARE STATUS (for badges)
    // ============================================
    json["sensor_active"] = readWaterSensor();
    json["pump_active"] = isPumpActive();
    json["direct_pump_mode"] = isDirectPumpMode();
    json["system_error"] = (waterAlgorithm.getState() == STATE_ERROR);
    
    // ============================================
    // SYSTEM DISABLE STATUS (NEW)
    // ============================================
    json["system_disabled"] = isSystemDisabled();
    
    
    // ============================================
    // PROCESS STATUS (for description + remaining time)
    // ============================================
    json["state_description"] = waterAlgorithm.getStateDescription();
    json["remaining_seconds"] = waterAlgorithm.getRemainingSeconds();
    
    // ============================================
    // EXISTING STATUS FIELDS
    // ============================================
    json["water_status"] = getSensorPhaseString();
    json["pump_running"] = isPumpActive();  // kept for backwards compatibility
    json["pump_remaining"] = getPumpRemainingTime();  // kept for backwards compatibility
    json["wifi_status"] = getWiFiStatus();
    json["wifi_connected"] = isWiFiConnected();
    json["rtc_time"] = getCurrentTimestamp();
    json["rtc_working"] = isRTCWorking();
    json["rtc_info"] = getTimeSourceInfo();
    json["rtc_hardware"] = isRTCHardware(); 
    json["rtc_needs_sync"] = rtcNeedsSynchronization();
    json["rtc_battery_issue"] = isBatteryIssueDetected();
    json["free_heap"] = ESP.getFreeHeap();
    json["uptime"] = millis();

    // ============================================
    // KALKWASSER STATUS
    // ============================================
    json["low_reservoir_count"]   = waterAlgorithm.getLowReservoirCount();
    json["low_reservoir_warning"] = waterAlgorithm.isLowReservoirWarning();
    json["low_reservoir_error"]   = (waterAlgorithm.getLastError() == ERROR_LOW_RESERVOIR);

    json["kalk_state"]              = kalkwasserScheduler.getStateString();
    json["kalk_enabled"]            = kalkwasserScheduler.isEnabled();
    json["kalk_last_mix_ts"]        = kalkwasserScheduler.getLastMixTs();
    json["kalk_last_dose_ts"]       = kalkwasserScheduler.getLastDoseTs();
    json["kalk_mix_done_bits"]      = kalkwasserScheduler.getMixDoneBits();
    json["kalk_dose_done_bits"]     = kalkwasserScheduler.getDoseDoneBits();
    json["kalk_alarm"]              = kalkwasserScheduler.isNoTopoffAlarm();
    json["audio_muted"]             = audioPlayer.isMuted();
    json["audio_volume"]            = audioPlayer.getVolume();
    json["mixing_pump_active"]      = isMixingPumpActive();
    json["peristaltic_pump_active"] = isPeristalticPumpRunning();
    json["rtc_ts"]                  = (uint32_t)getUnixTimestamp();

    // ============================================
    // ALARM SCORE
    // ============================================
    {
        const EmaBlock&    ema = waterAlgorithm.getEma();
        const TopOffConfig& cfg = waterAlgorithm.getConfig();
        json["alarm_score"]   = ema.alarm_score;
        json["alarm_armed"]   = waterAlgorithm.isAlarmArmed();
        uint8_t zone = 0;
        if (ema.alarm_score >= cfg.zone_yellow)      zone = 2;
        else if (ema.alarm_score >= cfg.zone_green)  zone = 1;
        json["alarm_zone"]    = zone;  // 0=green 1=yellow 2=red
        json["ema_rate"]      = ema.ema_rate_ml_h;
        json["bootstrap"]     = ema.bootstrap_count;
    }
    
    // ============================================
    // DEVICE INFO
    // ============================================
    json["device_id"] = getDeviceID();
    json["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
    json["authentication_enabled"] = areCredentialsLoaded();
    
    if (!areCredentialsLoaded()) {
        json["setup_required"] = true;
        json["setup_message"] = "Use Captive Portal to configure FRAM credentials";
    }
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOn(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    uint16_t duration = currentPumpSettings.manualCycleSeconds;
    String mode = "bistable";
    if (request->hasParam("mode", true) && request->getParam("mode", true)->value() == "monostable") {
        duration = DIRECT_PUMP_SAFETY_TIMEOUT_S;
        mode = "monostable";
    }

    bool success = directPumpOn(duration);

    JsonDocument json;
    json["success"] = success;
    json["duration"] = duration;
    json["mode"] = mode;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOff(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    directPumpOff();

    JsonDocument json;
    json["success"] = true;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleCalibrationOn(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = directPumpOn(CALIBRATION_SAFETY_TIMEOUT_S);

    JsonDocument json;
    json["success"] = success;
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleCalibrationOff(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    directPumpOff();

    JsonDocument json;
    json["success"] = true;
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handlePumpStop(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    stopPump();
    
    JsonDocument json;
    json["success"] = true;
    json["message"] = "Pump stopped";
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
    
    LOG_INFO("Pump manually stopped via web");
}

void handlePumpSettings(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        // Return current settings
        JsonDocument json;
        json["success"] = true;
        json["volume_per_second"] = currentPumpSettings.volumePerSecond;
        json["normal_cycle"] = currentPumpSettings.manualCycleSeconds;
        json["extended_cycle"] = currentPumpSettings.calibrationCycleSeconds;
        json["auto_mode"] = currentPumpSettings.autoModeEnabled;
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
        
    } else if (request->method() == HTTP_POST) {
        if (!request->hasParam("volume_per_second", true)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"No volume_per_second parameter\"}");
            return;
        }
        
        String volumeStr = request->getParam("volume_per_second", true)->value();
        float newVolume = volumeStr.toFloat();
        
        if (newVolume < 0.003 || newVolume > 100) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be between 0.1-3000 ml/30s\"}");
            return;
        }
        
        currentPumpSettings.volumePerSecond = newVolume;

        // Save to NVS (non-volatile storage)
        saveVolumeToNVS();
        
        LOG_INFO("Volume per second updated to %.1f ml/s", newVolume);
        
        JsonDocument response;
        response["success"] = true;
        response["volume_per_second"] = newVolume;
        response["message"] = "Volume per second updated successfully";
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    }
}

// ============================================
// SYSTEM TOGGLE HANDLER (NEW)
// Replaces old pump toggle with full system control
// ============================================
void handleSystemToggle(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = !isSystemDisabled();

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) {
        bool currentlyDisabled = isSystemDisabled();
        setSystemState(currentlyDisabled);  // toggle

        JsonDocument json;
        json["success"] = true;
        json["enabled"] = currentlyDisabled;  // was disabled → now enabled, and vice versa

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

        LOG_INFO("System %s via web interface", currentlyDisabled ? "ENABLED" : "DISABLED");
    }
}

void handleResetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = resetErrorStatsInFRAM();

    JsonDocument json;
    json["success"] = success;
    json["message"] = success ? "Statistics reset successfully" : "Failed to reset statistics";

    if (success) {
        ErrorStats stats;
        if (loadErrorStatsFromFRAM(stats)) {
            json["reset_timestamp"] = stats.last_reset_timestamp;
        }
    }

    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);

    LOG_INFO("Statistics reset requested via web interface - success: %s", success ? "true" : "false");
}

void handleGetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    ErrorStats stats;
    bool success = loadErrorStatsFromFRAM(stats);

    JsonDocument json;
    json["success"] = success;

    if (success) {
        json["gap1_fail_sum"] = stats.gap1_fail_sum;
        json["gap2_fail_sum"] = stats.gap2_fail_sum;
        json["water_fail_sum"] = stats.water_fail_sum;
        json["last_reset_timestamp"] = stats.last_reset_timestamp;

        time_t resetTime = (time_t)stats.last_reset_timestamp;
        struct tm* timeinfo = localtime(&resetTime);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M", timeinfo);
        json["last_reset_formatted"] = String(timeStr);
    } else {
        json["error"] = "Failed to load statistics";
    }

    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);
}

// ========================================
// ROLLING 24H VOLUME HANDLERS
// ========================================

void handleGetDailyVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    JsonDocument json;
    json["success"] = true;
    json["rolling_24h_ml"] = waterAlgorithm.getRolling24hVolume();
    json["history_window_s"] = waterAlgorithm.getConfig().history_window_s;
    json["dose_ml"]       = waterAlgorithm.getConfig().dose_ml;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleResetDailyVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    waterAlgorithm.resetRolling24h();

    JsonDocument json;
    json["success"] = true;
    json["rolling_24h_ml"] = waterAlgorithm.getRolling24hVolume();
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// MAX DOSE (per-cycle safety limit) HANDLERS
// ===============================

void handleGetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    JsonDocument json;
    json["success"]       = true;
    json["dose_ml"]       = waterAlgorithm.getConfig().dose_ml;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleSetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }

    int value = request->getParam("value", true)->value().toInt();

    if (value < 100 || value > 10000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 100-10000 ml\"}");
        return;
    }

    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.daily_limit_ml = (uint16_t)value;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"]        = true;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleSetDose(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }

    int value = request->getParam("value", true)->value().toInt();

    if (value < 1 || value > 2000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 1-2000 ml\"}");
        return;
    }

    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.dose_ml = (uint16_t)value;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"] = true;
    json["dose_ml"] = waterAlgorithm.getConfig().dose_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// TOP-OFF HISTORY HANDLER
// ===============================

extern volatile bool framBusy;

void handleGetCycleHistory(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (framBusy) {
        request->send(503, "application/json", "{\"success\":false,\"error\":\"System busy\"}");
        return;
    }

    static TopOffRecord buf[TOPOFF_HISTORY_SIZE];
    uint16_t count = loadTopOffHistory(buf, TOPOFF_HISTORY_SIZE);

    JsonDocument doc;
    doc["success"] = true;
    doc["total"]   = count;

    // EMA + alarm config context — needed by frontend chart
    const EmaBlock&    ema = waterAlgorithm.getEma();
    const TopOffConfig& cfg = waterAlgorithm.getConfig();
    doc["ema_rate"]     = ema.ema_rate_ml_h;
    doc["alarm_score"]  = ema.alarm_score;
    doc["bootstrap"]    = ema.bootstrap_count;
    doc["zone_green"]   = cfg.zone_green;
    doc["zone_yellow"]  = cfg.zone_yellow;
    doc["alarm_p1"]     = cfg.alarm_p1;
    doc["alarm_p2"]     = cfg.alarm_p2;
    doc["ema_clamp"]    = cfg.ema_clamp;

    JsonArray arr = doc["cycles"].to<JsonArray>();

    // Iterate newest-first (ring buffer returns oldest-first)
    for (int i = (int)count - 1; i >= 0; i--) {
        const TopOffRecord& r = buf[i];
        JsonObject obj = arr.add<JsonObject>();

        obj["ts"]           = r.timestamp;
        obj["interval_s"]   = r.interval_s;
        obj["rate_ml_h"]    = r.rate_ml_h;
        obj["dev_sigma"]    = r.dev_rate_sigma;
        obj["alert"]        = r.alert_level;
        obj["hour"]         = r.hour_of_day;
        obj["cycle"]        = r.cycle_num;
        obj["red_alert"]    = (bool)(r.flags & TopOffRecord::FLAG_RED_ALERT);
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// ===============================
// CLEAR CYCLE HISTORY HANDLER
// ===============================

void handleClearCycleHistory(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    if (waterAlgorithm.isInCycle()) {
        request->send(409, "application/json", "{\"success\":false,\"error\":\"Cycle in progress\"}");
        return;
    }
    bool ok = waterAlgorithm.resetCycleHistory();
    if (ok) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"FRAM write failed\"}");
    }
}

// ===============================
// KALKWASSER HANDLERS
// ===============================

void handleKalkwasserConfig(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    if (request->method() == HTTP_GET) {
        JsonDocument doc;
        doc["success"]            = true;
        doc["enabled"]            = kalkwasserScheduler.isEnabled();
        doc["daily_dose_ml"]      = kalkwasserScheduler.getDailyDoseMl();
        doc["flow_rate_ul_per_s"] = kalkwasserScheduler.getFlowRateUlPerS();
        doc["flow_rate_ml_s"]     = (float)kalkwasserScheduler.getFlowRateUlPerS() / 1000.0f;
        doc["dose_duration_s"]    = kalkwasserScheduler.getDoseDurationS();
        doc["next_mix_ts"]        = kalkwasserScheduler.getNextMixTs();
        doc["next_dose_ts"]       = kalkwasserScheduler.getNextDoseTs();
        doc["kalk_state"]         = kalkwasserScheduler.getStateString();
        String resp; serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    } else {
        bool en = kalkwasserScheduler.isEnabled();
        uint16_t dose_ml = kalkwasserScheduler.getDailyDoseMl();
        if (request->hasParam("enabled", true))
            en = request->getParam("enabled", true)->value().toInt() != 0;
        if (request->hasParam("daily_dose_ml", true))
            dose_ml = (uint16_t)request->getParam("daily_dose_ml", true)->value().toInt();
        if (dose_ml > 5000) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"dose_ml max 5000\"}");
            return;
        }
        kalkwasserScheduler.setConfig(en, dose_ml);
        JsonDocument doc;
        doc["success"]       = true;
        doc["enabled"]       = en;
        doc["daily_dose_ml"] = dose_ml;
        String resp; serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    }
}

void handleKalkwasserCalibrate(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    bool ok = kalkwasserScheduler.startCalibration();
    if (ok) {
        request->send(200, "application/json", "{\"success\":true,\"duration_s\":30}");
    } else {
        const char* reason = waterAlgorithm.isInCycle()
            ? "Top-off cycle in progress"
            : "Scheduler not idle";
        JsonDocument doc;
        doc["success"] = false;
        doc["error"]   = reason;
        String resp; serializeJson(doc, resp);
        request->send(409, "application/json", resp);
    }
}

void handleKalkwasserFlowRate(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    if (!request->hasParam("measured_ml", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing measured_ml\"}");
        return;
    }
    float ml = request->getParam("measured_ml", true)->value().toFloat();
    if (!kalkwasserScheduler.setFlowRate(ml)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value out of range (0.1-500 ml)\"}");
        return;
    }
    JsonDocument doc;
    doc["success"]         = true;
    doc["measured_ml"]     = ml;
    doc["flow_rate_ul_s"]  = kalkwasserScheduler.getFlowRateUlPerS();
    doc["flow_rate_ml_s"]  = kalkwasserScheduler.getFlowRateMlPerS();
    doc["dose_duration_s"] = kalkwasserScheduler.getDoseDurationS();
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}

// ===============================
// SYSTEM RESET HANDLER
// ===============================

void handleSystemReset(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = waterAlgorithm.resetSystem();

    JsonDocument json;
    json["success"] = success;
    json["state"] = waterAlgorithm.getStateString();
    json["message"] = success ? "System reset to IDLE" : "Reset blocked - logging in progress";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// DIRECT MIXING PUMP CONTROL
// ===============================

void handleMixingPumpDirect(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    if (!request->hasParam("action", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing action\"}");
        return;
    }
    String action = request->getParam("action", true)->value();
    bool active;
    if (action == "on") {
        bool ok = kalkwasserScheduler.directMixOn();
        if (!ok) {
            request->send(409, "application/json", "{\"success\":false,\"error\":\"Scheduler not idle\"}");
            return;
        }
        active = true;
    } else {
        kalkwasserScheduler.directMixOff();
        active = false;
    }
    JsonDocument doc;
    doc["success"] = true;
    doc["active"]  = active;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}

// ===============================
// DIRECT PERISTALTIC PUMP CONTROL
// ===============================

void handlePeristalticPumpDirect(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    if (!request->hasParam("action", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing action\"}");
        return;
    }
    String action = request->getParam("action", true)->value();
    bool active;
    if (action == "on") {
        bool ok = kalkwasserScheduler.directDoseOn();
        if (!ok) {
            request->send(409, "application/json", "{\"success\":false,\"error\":\"Scheduler not idle\"}");
            return;
        }
        active = true;
    } else {
        kalkwasserScheduler.directDoseOff();
        active = false;
    }
    JsonDocument doc;
    doc["success"] = true;
    doc["active"]  = active;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}

// ===============================
// HEALTH CHECK HANDLER
// ===============================

void handleHealth(AsyncWebServerRequest *request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
        request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
        return;
    }

    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"device_name\":\"" + String(getDeviceID()) + "\",";
    json += "\"uptime\":" + String(millis());
    json += "}";

    request->send(200, "application/json", json);
}

// ===============================
// ALARM AUDIO MUTE TOGGLE
// GET  → zwraca stan wyciszenia
// POST → przełącza mute (toggle)
// ===============================

void handleAudioVolume(AsyncWebServerRequest *request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (request->method() == HTTP_GET) {
        JsonDocument json;
        json["success"] = true;
        json["volume"]  = audioPlayer.getVolume();
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) {
        if (!request->hasParam("volume", true)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing volume\"}");
            return;
        }
        int v = request->getParam("volume", true)->value().toInt();
        if (v < AUDIO_VOLUME_MIN || v > AUDIO_VOLUME_MAX || v % 5 != 0) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Volume must be 5/10/15/20/25/30\"}");
            return;
        }
        audioPlayer.setVolume((uint8_t)v);

        JsonDocument json;
        json["success"] = true;
        json["volume"]  = audioPlayer.getVolume();
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

        LOG_INFO("Audio volume set to %d via web interface", audioPlayer.getVolume());
    }
}

// ===============================
// ALGORITHM CONFIG HANDLER
// GET  — zwraca bieżące 7 parametrów algorytmu
// POST — zapisuje nowe wartości (form-encoded)
// ===============================

void handleAlgConfig(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (request->method() == HTTP_GET) {
        const TopOffConfig& cfg = waterAlgorithm.getConfig();
        JsonDocument doc;
        doc["success"]     = true;
        doc["ema_alpha"]   = cfg.ema_alpha;
        doc["ema_clamp"]   = cfg.ema_clamp;
        doc["alarm_p1"]    = cfg.alarm_p1;
        doc["alarm_p2"]    = cfg.alarm_p2;
        doc["zone_green"]  = cfg.zone_green;
        doc["zone_yellow"] = cfg.zone_yellow;
        doc["initial_ema"] = cfg.initial_ema;
        String resp; serializeJson(doc, resp);
        request->send(200, "application/json", resp);
        return;
    }

    // POST — walidacja i zapis
    auto getF = [&](const char* name, float def) -> float {
        return request->hasParam(name, true)
            ? request->getParam(name, true)->value().toFloat()
            : def;
    };

    const TopOffConfig& cur = waterAlgorithm.getConfig();
    float alpha      = getF("ema_alpha",   cur.ema_alpha);
    float clamp      = getF("ema_clamp",   cur.ema_clamp);
    float p1         = getF("alarm_p1",    cur.alarm_p1);
    float p2         = getF("alarm_p2",    cur.alarm_p2);
    float zGreen     = getF("zone_green",  cur.zone_green);
    float zYellow    = getF("zone_yellow", cur.zone_yellow);
    float initEma    = getF("initial_ema", cur.initial_ema);

    // Walidacja
    if (alpha < 0.05f || alpha > 0.50f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"ema_alpha: 0.05–0.50\"}");
        return;
    }
    if (clamp < 0.10f || clamp > 0.80f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"ema_clamp: 0.10–0.80\"}");
        return;
    }
    if (p1 < 0.01f || p1 > 2.0f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"alarm_p1: 0.01–2.0\"}");
        return;
    }
    if (p2 < 0.05f || p2 > 0.99f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"alarm_p2: 0.05–0.99\"}");
        return;
    }
    if (zGreen < 1.0f || zGreen > 200.0f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"zone_green: 1.0–200.0\"}");
        return;
    }
    if (zYellow <= zGreen || zYellow > 500.0f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"zone_yellow must be > zone_green and ≤ 500\"}");
        return;
    }
    if (initEma < 1.0f || initEma > 500.0f) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"initial_ema: 1.0–500.0 ml/h\"}");
        return;
    }

    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.ema_alpha   = alpha;
    cfg.ema_clamp   = clamp;
    cfg.alarm_p1    = p1;
    cfg.alarm_p2    = p2;
    cfg.zone_green  = zGreen;
    cfg.zone_yellow = zYellow;
    cfg.initial_ema = initEma;
    waterAlgorithm.setConfig(cfg);

    JsonDocument doc;
    doc["success"]     = true;
    doc["ema_alpha"]   = cfg.ema_alpha;
    doc["ema_clamp"]   = cfg.ema_clamp;
    doc["alarm_p1"]    = cfg.alarm_p1;
    doc["alarm_p2"]    = cfg.alarm_p2;
    doc["zone_green"]  = cfg.zone_green;
    doc["zone_yellow"] = cfg.zone_yellow;
    doc["initial_ema"] = cfg.initial_ema;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}

void handleAlarmToggle(AsyncWebServerRequest *request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (request->method() == HTTP_GET) {
        JsonDocument json;
        json["success"] = true;
        json["muted"]   = audioPlayer.isMuted();
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) {
        bool newMuted = !audioPlayer.isMuted();
        audioPlayer.setMuted(newMuted);

        JsonDocument json;
        json["success"] = true;
        json["muted"]   = newMuted;
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

        LOG_INFO("Alarm audio %s via web interface", newMuted ? "MUTED" : "UNMUTED");
    }
}