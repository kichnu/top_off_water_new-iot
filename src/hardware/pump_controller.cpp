#include "pump_controller.h"
#include "../hardware/hardware_pins.h"
#include "../config/config.h"
#include "../hardware/rtc_controller.h"
#include "../core/logging.h"
#include <math.h>


bool pumpRunning = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDuration = 0;
String currentActionType = "";

static bool pumpActive = false;
static bool directPumpMode = false;

void initPumpController() {

    pinMode(ATO_PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(ATO_PUMP_RELAY_PIN, HIGH);
    
    pumpRunning = false;
    LOG_INFO("");
    LOG_INFO("Pump controller initialized");
}

void updatePumpController() {
    // Re-assert pin ownership every cycle — ESP32-C3 WiFi/ADC may reclaim GPIO2 (A0) via periman
    if (pumpRunning) {
        pinMode(ATO_PUMP_RELAY_PIN, OUTPUT);
    }

        // Check global pump state - stop if disabled (but not in direct mode)
    if (!pumpGlobalEnabled && pumpRunning && !directPumpMode) {
        digitalWrite(ATO_PUMP_RELAY_PIN, HIGH);
        pumpRunning = false;
        LOG_INFO("");
        LOG_INFO("Pump stopped - globally disabled");
        return;
    }

    if (pumpRunning && (millis() - pumpStartTime >= pumpDuration)) {
        // Stop pump and log event
        digitalWrite(ATO_PUMP_RELAY_PIN, HIGH);
        pumpRunning = false;

        uint16_t actualDuration = (millis() - pumpStartTime) / 1000;
        uint16_t volumeML = (uint16_t)round(actualDuration * currentPumpSettings.volumePerSecond);

        LOG_INFO("");
        LOG_INFO("Pump stopped after %d seconds, estimated volume: %d ml",
                 actualDuration, volumeML);

        if (directPumpMode) {
            directPumpMode = false;
            LOG_INFO("");
            LOG_INFO("Direct pump safety timeout reached - pump stopped");
        }

        currentActionType = "";
    }
}

bool triggerPump(uint16_t durationSeconds, const String& actionType) {
    LOG_INFO("");
    LOG_INFO("triggerPump called: %s for %ds", actionType.c_str(), durationSeconds);
    
    if (pumpRunning) {
        LOG_WARNING("");
        LOG_WARNING("Pump already running, ignoring trigger");
        return false;
    }

    if (!pumpGlobalEnabled) {
        LOG_INFO("");
        LOG_INFO("Pump trigger blocked - globally disabled");
        return false;
    }
    
    digitalWrite(ATO_PUMP_RELAY_PIN, LOW);
    pumpRunning = true;
    pumpStartTime = millis();
    pumpDuration = durationSeconds * 1000UL;
    currentActionType = actionType;
    
    LOG_INFO("");
    LOG_INFO("Pump started: %s for %d seconds", actionType.c_str(), durationSeconds);
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

void stopPump() {
    if (pumpRunning) {
        digitalWrite(ATO_PUMP_RELAY_PIN, HIGH);
        pumpRunning = false;

        // Calculate actual duration and volume
        uint16_t actualDuration = (millis() - pumpStartTime) / 1000;
        uint16_t volumeML = (uint16_t)round(actualDuration * currentPumpSettings.volumePerSecond);

        LOG_INFO("");
        LOG_INFO("Pump manually stopped after %d seconds, estimated volume: %d ml",
                 actualDuration, volumeML);

        if (directPumpMode) {
            directPumpMode = false;
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
    digitalWrite(ATO_PUMP_RELAY_PIN, LOW);
    pumpRunning = true;
    pumpStartTime = millis();
    pumpDuration = durationSeconds * 1000UL;
    currentActionType = "DIRECT_MANUAL";

    LOG_INFO("");
    LOG_INFO("Direct pump ON for %d seconds", durationSeconds);
    return true;
}

void directPumpOff() {
    if (!pumpRunning || !directPumpMode) {
        return;
    }

    digitalWrite(ATO_PUMP_RELAY_PIN, HIGH);
    pumpRunning = false;
    directPumpMode = false;
    currentActionType = "";

    LOG_INFO("");
    LOG_INFO("Direct pump stopped");
}

bool isDirectPumpMode() {
    return directPumpMode;
}