#include "pump_controller.h"
#include "../hardware/hardware_pins.h"
#include "../config/config.h"
#include "../hardware/rtc_controller.h"
#include "../core/logging.h"
#include <esp_timer.h>
#include <math.h>


bool pumpRunning = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDuration = 0;
String currentActionType = "";

static bool     directPumpMode          = false;
static uint16_t lastDirectPumpVolumeMl  = 0;
static uint32_t lastDirectPumpRunMs     = 0;
static uint32_t lastPumpRunMs           = 0;

// One-shot timer that cuts the relay at the exact end of a timed run;
// bookkeeping follows in updatePumpController()
static esp_timer_handle_t g_stopTimer  = nullptr;
static volatile bool      g_timerFired = false;
static volatile uint32_t  g_timerRunMs = 0;

static void onStopTimer(void*) {   // esp_timer task context
    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);
    g_timerRunMs = pumpDuration;
    g_timerFired = true;
}

static void armStopTimer(uint32_t durationMs) {
    g_timerFired = false;
    esp_timer_stop(g_stopTimer);
    esp_timer_start_once(g_stopTimer, (uint64_t)durationMs * 1000ULL);
}

static uint16_t directVolumeMl(uint32_t runMs) {
    return (uint16_t)lroundf(runMs * currentPumpSettings.volumePerSecond / 1000.0f);
}

void initPumpController() {

    pinMode(ATO_PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);

    esp_timer_create_args_t args = {};
    args.callback = onStopTimer;
    args.name     = "pump_stop";
    esp_timer_create(&args, &g_stopTimer);

    pumpRunning = false;
    LOG_INFO("");
    LOG_INFO("Pump controller initialized");
}

void updatePumpController() {
    // Re-assert pin ownership every cycle — ESP32 WiFi/ADC may reclaim GPIO via periman
    if (pumpRunning && !g_timerFired) {
        pinMode(ATO_PUMP_RELAY_PIN, OUTPUT);
    }

        // Check global pump state - stop if disabled (but not in direct mode)
    if (!pumpGlobalEnabled && pumpRunning && !directPumpMode) {
        esp_timer_stop(g_stopTimer);
        digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);
        pumpRunning = false;
        lastPumpRunMs = millis() - pumpStartTime;
        LOG_INFO("");
        LOG_INFO("Pump stopped - globally disabled");
        return;
    }

    if (pumpRunning && (g_timerFired || millis() - pumpStartTime >= pumpDuration)) {
        // Relay already cut by the timer; the millis() check is only a fallback
        digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);
        pumpRunning   = false;
        lastPumpRunMs = g_timerFired ? g_timerRunMs : (uint32_t)pumpDuration;
        g_timerFired  = false;

        LOG_INFO("");
        LOG_INFO("Pump stopped after %.1f s (%s)", lastPumpRunMs / 1000.0f, currentActionType.c_str());

        if (directPumpMode) {
            directPumpMode = false;
            lastDirectPumpRunMs    = lastPumpRunMs;
            lastDirectPumpVolumeMl = directVolumeMl(lastPumpRunMs);
            LOG_INFO("");
            LOG_INFO("Direct pump safety timeout reached - pump stopped, ~%u ml", lastDirectPumpVolumeMl);
        }

        currentActionType = "";
    }
}

bool triggerPumpMs(uint32_t durationMs, const String& actionType) {
    if (pumpRunning) {
        LOG_WARNING("");
        LOG_WARNING("Pump already running, ignoring trigger (%s)", actionType.c_str());
        return false;
    }

    if (!pumpGlobalEnabled) {
        LOG_INFO("");
        LOG_INFO("Pump trigger blocked - globally disabled");
        return false;
    }

    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_ON);
    pumpRunning = true;
    pumpStartTime = millis();
    pumpDuration = durationMs;
    currentActionType = actionType;
    armStopTimer(durationMs);

    LOG_INFO("");
    LOG_INFO("Pump started: %s for %.1f s", actionType.c_str(), durationMs / 1000.0f);
    return true;
}

bool isPumpActive() {
    return pumpRunning;
}

uint32_t getPumpRemainingTime() {
    if (!pumpRunning) return 0;

    unsigned long elapsed = millis() - pumpStartTime;
    if (elapsed >= pumpDuration) return 0;

    return (pumpDuration - elapsed) / 1000;
}

uint32_t getLastPumpRunMs() {
    return lastPumpRunMs;
}

void stopPump() {
    if (pumpRunning) {
        esp_timer_stop(g_stopTimer);
        digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);
        pumpRunning   = false;
        lastPumpRunMs = g_timerFired ? g_timerRunMs : (uint32_t)(millis() - pumpStartTime);
        g_timerFired  = false;

        LOG_INFO("");
        LOG_INFO("Pump manually stopped after %.1f s (%s)", lastPumpRunMs / 1000.0f, currentActionType.c_str());

        if (directPumpMode) {
            directPumpMode = false;
            lastDirectPumpRunMs = lastPumpRunMs;
            LOG_INFO("Direct pump stopped via stopPump()");
        }

        currentActionType = "";
    }
}

bool directPumpOn(uint16_t durationSeconds) {
    if (pumpRunning && !directPumpMode) {
        LOG_WARNING("directPumpOn blocked — algorithm pump is running");
        return false;
    }
    if (pumpRunning && directPumpMode) {
        // Already running in direct mode — ignore
        return true;
    }

    directPumpMode = true;
    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_ON);
    pumpRunning = true;
    pumpStartTime = millis();
    pumpDuration = durationSeconds * 1000UL;
    currentActionType = "DIRECT_MANUAL";
    armStopTimer(pumpDuration);

    LOG_INFO("");
    LOG_INFO("Direct pump ON for %d seconds", durationSeconds);
    return true;
}

void directPumpOff() {
    if (!pumpRunning || !directPumpMode) {
        return;
    }

    esp_timer_stop(g_stopTimer);
    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_OFF);
    lastPumpRunMs           = g_timerFired ? g_timerRunMs : (uint32_t)(millis() - pumpStartTime);
    g_timerFired            = false;
    lastDirectPumpRunMs     = lastPumpRunMs;
    lastDirectPumpVolumeMl  = directVolumeMl(lastPumpRunMs);

    pumpRunning    = false;
    directPumpMode = false;
    currentActionType = "";

    LOG_INFO("");
    LOG_INFO("Direct pump stopped after %.1f s, ~%d ml", lastPumpRunMs / 1000.0f, lastDirectPumpVolumeMl);
}

uint16_t getLastDirectPumpVolumeMl() {
    uint16_t v = lastDirectPumpVolumeMl;
    lastDirectPumpVolumeMl = 0;
    return v;
}

uint32_t getLastDirectPumpRunMs() {
    return lastDirectPumpRunMs;
}

bool isDirectPumpMode() {
    return directPumpMode;
}
