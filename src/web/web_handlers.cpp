
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
#include "../hardware/reserve_controller.h"
#include "../hardware/level_stable.h"
#include "../hardware/pump_calibration.h"
#include "../hardware/level_sensor.h"
#include "../config/reservoir_config.h"

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
    // RESERVE STATUS
    // ============================================
    json["reserve_low"]         = isReserveLow();
    json["reserve_empty"]       = isReserveEmpty();

    // Reservoir level sensor (VL53L4CD) — diagnostics, not yet used by the algorithm
    {
        const LevelSeries& ls = getLastLevelSeries();
        json["level_sensor"]      = getLevelSensorStateString();
        json["level_valid"]       = ls.valid;
        json["level_mm"]          = ls.distance_mm;
        json["level_sd_mm"]       = ls.stddev_mm;
        json["level_sem_mm"]      = ls.sem_mm;
        json["level_n"]           = ls.samples;
        json["level_rej"]         = ls.rejected;
        json["level_signal_kcps"] = ls.signal_kcps;
        json["level_ambient_kcps"]= ls.ambient_kcps;
        json["level_age_s"]       = ls.finished_ms ? (long)((millis() - ls.finished_ms) / 1000) : -1L;
    }

    // Reserve (portion counter) for the monitor bargraph
    {
        uint16_t    dose = waterAlgorithm.getConfig().dose_ml;
        ReserveView rv   = getReserveView(dose);
        json["res_configured"]  = isReservoirConfigSaved();
        json["res_sensor"]      = rv.sensorCheck;
        json["res_ml"]          = rv.ml;
        json["res_portions"]    = rv.portions;
        json["res_capacity_ml"] = rv.capacityMl;
        json["res_pct"]         = rv.pct;
        json["res_water_mm"]    = rv.waterMm;
        json["res_zone"]        = (uint8_t)rv.zone;   // 0=ok 1=low 2=empty 3=unknown
        json["res_low_ml"]      = rv.lowMl;   // permanent yellow reserve range
        json["res_dose_ml"]     = dose;
        json["level_settling"]  = isLevelSettling();

        // Pump calibration
        {
            const PumpCalibration& pc = getReservoirConfig().cal;
            const CalStatus& cs = getCalStatus();
            json["cal_calibrated"] = isPumpCalibrated();
            json["cal_portion_ms"] = getPortionMs();
            json["cal_ts"]         = pc.ts;
            json["cal_phase"]      = (uint8_t)cs.phase;   // 0=idle 1=countdown 2=ready 3=running 4=result
            if (cs.note) json["cal_note"] = cs.note;
            if (cs.phase != CAL_IDLE) {
                json["cal_dose_ml"]    = cs.doseMl;
                json["cal_runs"]       = cs.runs;
                json["cal_runs_total"] = CAL_RUNS;
                json["cal_pause_left"] = cs.pauseLeftS;
                json["cal_run_ms"]     = cs.runElapsedMs;
                JsonArray arr = json["cal_times"].to<JsonArray>();
                for (uint8_t i = 0; i < cs.runs; i++) arr.add(cs.runMs[i]);
                if (cs.phase == CAL_RESULT) {
                    json["cal_mean_ms"] = cs.meanMs;
                    json["cal_min_ms"]  = cs.minMs;
                    json["cal_max_ms"]  = cs.maxMs;
                    json["cal_sd_ms"]   = cs.sdMs;
                }
            }
        }

        // Refill measurement ("Reserve refilled")
        const RefillStatus& rs = getRefillStatus();
        json["refill_phase"] = (uint8_t)rs.phase;     // 0=idle 1=measuring 2=result
        if (rs.phase != REFILL_IDLE) {
            json["refill_result"]    = (uint8_t)rs.result;   // 1=ok 2=mismatch 3=no reading
            json["refill_series"]    = rs.series;
            json["refill_window"]    = STABLE_WINDOW;
            json["refill_elapsed_s"] = rs.elapsedS;
            json["refill_dist_mm"]   = rs.distanceMm;
            json["refill_expect_mm"] = rs.expectedMm;
            json["refill_dev_mm"]    = rs.deviationMm;
            json["refill_dev_ml"]    = rs.deviationMl;
            json["refill_spread_mm"] = rs.spreadMm;
            json["refill_tol_mm"]    = getReservoirConfig().marker_tol_mm;
            json["refill_suspect"]   = rs.sensorSuspect;
            if (rs.note) json["refill_note"] = rs.note;
        }
    }
    json["rtc_ts"]            = (uint32_t)getUnixTimestamp();

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

    if (isReserveEmpty()) {
        request->send(400, "application/json",
            "{\"success\":false,\"error\":\"Reserve empty — refill the reservoir\"}");
        return;
    }
    if (isReserveRefillActive() || isPumpCalibrationActive()) {
        request->send(409, "application/json",
            "{\"success\":false,\"error\":\"Refill measurement or calibration in progress\"}");
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
        if (isPumpCalibrationActive()) {
            request->send(409, "application/json",
                "{\"success\":false,\"error\":\"Pump calibration in progress — Service Mode required\"}");
            return;
        }
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
    json["state"]   = waterAlgorithm.getStateString();
    json["message"] = success ? "System reset to IDLE" : "Reset blocked - logging in progress";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
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
        doc["success"]        = true;
        doc["ema_alpha"]      = cfg.ema_alpha;
        doc["ema_clamp"]      = cfg.ema_clamp;
        doc["alarm_p1"]       = cfg.alarm_p1;
        doc["alarm_p2"]       = cfg.alarm_p2;
        doc["zone_green"]     = cfg.zone_green;
        doc["zone_yellow"]    = cfg.zone_yellow;
        doc["initial_ema"]    = cfg.initial_ema;
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
    doc["success"]           = true;
    doc["ema_alpha"]         = cfg.ema_alpha;
    doc["ema_clamp"]         = cfg.ema_clamp;
    doc["alarm_p1"]          = cfg.alarm_p1;
    doc["alarm_p2"]          = cfg.alarm_p2;
    doc["zone_green"]        = cfg.zone_green;
    doc["zone_yellow"]       = cfg.zone_yellow;
    doc["initial_ema"]       = cfg.initial_ema;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}


// ===============================
// RESERVOIR CONFIG HANDLER
// GET  — geometry, level-sensor settings and derived values
// POST — validates and saves (form-encoded; missing fields keep current values)
// ===============================

static void sendReserveConfig(AsyncWebServerRequest* request) {
    const ReservoirConfig& c = getReservoirConfig();
    uint16_t dose = waterAlgorithm.getConfig().dose_ml;
    uint32_t cap  = reservoirCapacityMl();
    ReservoirLevels lv = reservoirLevels(dose);

    JsonDocument doc;
    doc["success"]             = true;
    doc["saved"]               = isReservoirConfigSaved();
    doc["length_mm"]           = c.length_mm;
    doc["width_mm"]            = c.width_mm;
    doc["sensor_to_bottom_mm"] = c.sensor_to_bottom_mm;
    doc["marker_level_mm"]     = c.marker_level_mm;
    doc["marker_tol_mm"]       = c.marker_tol_mm;
    doc["use_level_sensor"]    = useLevelSensor();
    doc["ml_per_mm"]           = reservoirMlPerMm();
    doc["capacity_ml"]         = cap;
    doc["dose_ml"]             = dose;
    doc["full_ml"]             = lv.fullMl;     // rounded down to whole portions
    doc["hidden_ml"]           = lv.hiddenMl;   // safety bottom, not shown in the monitor
    doc["usable_ml"]           = lv.usableMl;
    doc["low_ml"]              = lv.lowMl;
    doc["low_pct"]             = RESERVE_LOW_PCT_FIXED;
    doc["hidden_pct"]          = RESERVE_HIDDEN_PCT_FIXED;
    const char* warn = reservoirConfigWarning(c);
    if (warn) doc["warning"] = warn;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
}

void handleReserveConfig(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (request->method() == HTTP_POST) {
        ReservoirConfig c = getReservoirConfig();
        auto getI = [&](const char* name, long def) -> long {
            return request->hasParam(name, true)
                ? request->getParam(name, true)->value().toInt()
                : def;
        };
        // Clamp to the field width before validation so overflow can't wrap into range
        auto u16 = [](long v) -> uint16_t { return (uint16_t)constrain(v, 0L, 65535L); };
        auto u8  = [](long v) -> uint8_t  { return (uint8_t)constrain(v, 0L, 255L); };

        c.length_mm           = u16(getI("length_mm",           c.length_mm));
        c.width_mm            = u16(getI("width_mm",            c.width_mm));
        c.sensor_to_bottom_mm = u16(getI("sensor_to_bottom_mm", c.sensor_to_bottom_mm));
        c.marker_level_mm     = u16(getI("marker_level_mm",     c.marker_level_mm));
        c.marker_tol_mm       = u8(getI("marker_tol_mm",        c.marker_tol_mm));
        if (request->hasParam("use_level_sensor", true)) {
            bool on = getI("use_level_sensor", 1) != 0;
            c.flags = on ? (c.flags | RESERVOIR_FLAG_USE_SENSOR)
                         : (c.flags & ~RESERVOIR_FLAG_USE_SENSOR);
        }

        const char* err = setReservoirConfig(c);
        if (err) {
            JsonDocument doc;
            doc["success"] = false;
            doc["error"]   = err;
            String resp; serializeJson(doc, resp);
            request->send(400, "application/json", resp);
            return;
        }
    }

    sendReserveConfig(request);
}

// ===============================
// RESERVE REFILL HANDLER
// POST action=start     — sensor off: counter full; sensor on: start the measurement
//      action=confirm   — accept the measured result (counter full, reference = reading)
//      action=no_sensor — counter full, level sensor switched off
//      action=remeasure / cancel
// ===============================

void handleReserveRefill(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    String action = request->hasParam("action", true)
        ? request->getParam("action", true)->value() : String("start");

    JsonDocument doc;
    const char* err = nullptr;
    if (action == "start") {
        bool measuring = startReserveRefill(&err);
        doc["measuring"] = measuring;
    } else {
        err = reserveRefillAction(action);
    }
    doc["success"] = (err == nullptr);
    if (err) doc["error"] = err;
    String resp; serializeJson(doc, resp);
    request->send(err ? 409 : 200, "application/json", resp);
}

// ===============================
// PUMP CALIBRATION HANDLER
// POST action=start&dose_ml=N — first step: the portion, then the procedure
//      action=run / stop     — one run (pump on / user stop at the vessel mark)
//      action=confirm        — save the portion, its time and ml/s
//      action=cancel
// ===============================

void handlePumpCalibration(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    String action = request->hasParam("action", true)
        ? request->getParam("action", true)->value() : String("");

    const char* err;
    if (action == "start") {
        long dose = request->hasParam("dose_ml", true)
            ? request->getParam("dose_ml", true)->value().toInt() : 0;
        err = startPumpCalibration((uint16_t)constrain(dose, 0L, 65535L));
    } else {
        err = pumpCalibrationAction(action);
    }

    JsonDocument doc;
    doc["success"] = (err == nullptr);
    if (err) doc["error"] = err;
    String resp; serializeJson(doc, resp);
    request->send(err ? 409 : 200, "application/json", resp);
}
