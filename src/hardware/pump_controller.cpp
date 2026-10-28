#include "pump_controller.h"
#include "../hardware/hardware_pins.h"
#include "../config/config.h"
#include "../hardware/rtc_controller.h"
#include "../core/logging.h"
#include "pump_calibration.h"
#include "reserve_controller.h"
#include <esp_timer.h>
#include <math.h>


bool pumpRunning = false;
unsigned long pumpStartTime = 0;
unsigned long pumpDuration = 0;
String currentActionType = "";

static bool     directPumpMode          = false;
static uint32_t lastDirectPumpRunMs     = 0;
static uint32_t lastPumpRunMs           = 0;

// Manual portion queue (directPumpMode session)
static uint8_t  g_manualLeft     = 0;      // portions left, including the running one
static uint8_t  g_manualDone     = 0;
static bool     g_manualPause    = false;
static uint32_t g_pauseStartMs   = 0;
static uint32_t g_sessionRunMs   = 0;

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

// Every finished run takes water from the reservoir
static void onRunEnd() {
    reserveOnPumpRun(lastPumpRunMs, currentActionType == "CALIBRATION");
}

static uint16_t directVolumeMl(uint32_t runMs) {
    return (uint16_t)lroundf(runMs * currentPumpSettings.volumePerSecond / 1000.0f);
}

static void startManualPortion() {
    digitalWrite(ATO_PUMP_RELAY_PIN, PUMP_RELAY_ON);
    pumpRunning       = true;
    pumpStartTime     = millis();
    pumpDuration      = getPortionMs();
    currentActionType = "MANUAL_PORTION";
    armStopTimer(pumpDuration);

    LOG_INFO("");
    LOG_INFO("Manual portion %u started (%.1f s, %u left after it)",
             g_manualDone + 1, pumpDuration / 1000.0f, g_manualLeft - 1);
}

static void endManualSession(const char* reason) {
    LOG_INFO("Manual dosing finished (%s): %u portion(s), %.1f s, ~%u ml",
             reason, g_manualDone, g_sessionRunMs / 1000.0f, directVolumeMl(g_sessionRunMs));
    lastDirectPumpRunMs = g_sessionRunMs;
    directPumpMode = false;
    g_manualLeft   = 0;
    g_manualPause  = false;
}

// Bookkeeping after a manual portion ended (timer or forced stop)
static void onManualPortionEnd(bool forced) {
    g_sessionRunMs += lastPumpRunMs;
    g_manualDone++;
    if (g_manualLeft > 0) g_manualLeft--;
    if (forced) {
        endManualSession("stopped");
    } else if (g_manualLeft > 0) {
        g_manualPause  = true;
        g_pauseStartMs = millis();
    } else {
        endManualSession("queue done");
    }
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
        onRunEnd();
        return;
    }

    // Pause between manual portions — the next one starts only if it fits above the alarm level
    if (directPumpMode && g_manualPause && millis() - g_pauseStartMs >= MANUAL_PORTION_PAUSE_MS) {
        g_manualPause = false;
        if (!reserveCanDose()) {
            LOG_WARNING("Manual dosing: no water above the alarm level for one more portion — remaining portions dropped");
            endManualSession("reservoir at the alarm level");
        } else {
            startManualPortion();
        }
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

        onRunEnd();
        currentActionType = "";
        if (directPumpMode) onManualPortionEnd(false);
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

        onRunEnd();
        currentActionType = "";
        if (directPumpMode) onManualPortionEnd(true);
    } else if (directPumpMode) {
        endManualSession("stopped in pause");   // between portions — nothing running
    }
}

bool manualPortionAdd() {
    if (pumpRunning && !directPumpMode) {
        LOG_WARNING("Manual portion blocked — algorithm/calibration pump is running");
        return false;
    }
    if (!directPumpMode) {
        if (!reserveCanDose()) {
            LOG_WARNING("Manual portion blocked — no water above the alarm level for one portion");
            return false;
        }
        directPumpMode = true;
        g_manualLeft   = 1;
        g_manualDone   = 0;
        g_manualPause  = false;
        g_sessionRunMs = 0;
        startManualPortion();
        return true;
    }
    if (g_manualLeft >= MANUAL_QUEUE_MAX) return false;
    g_manualLeft++;
    LOG_INFO("Manual portion queued (%u left)", g_manualLeft);
    return true;
}

void manualPortionsStop() {
    if (!directPumpMode) return;
    if (pumpRunning) {
        g_manualLeft = 1;   // finish the running portion, drop the rest
        LOG_INFO("Manual dosing: OFF — finishing the running portion");
    } else {
        endManualSession("OFF in pause");
    }
}

bool    isManualPause()          { return directPumpMode && g_manualPause; }
uint8_t getManualPortionsLeft()  { return directPumpMode ? g_manualLeft : 0; }
uint8_t getManualPortionsDone()  { return g_manualDone; }

uint32_t getLastDirectPumpRunMs() {
    return lastDirectPumpRunMs;
}

bool isDirectPumpMode() {
    return directPumpMode;
}
