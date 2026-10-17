#include "pump_calibration.h"
#include <math.h>
#include "pump_controller.h"
#include "reserve_controller.h"
#include "rtc_controller.h"
#include "../config/config.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"
#include "../core/logging.h"

static CalStatus     g_cal       = {};
static uint32_t      g_runStart  = 0;
static uint32_t      g_lastEnd   = 0;      // millis() when the last run stopped / countdown began
static volatile bool g_stopping  = false;  // web "stop" in progress — not an external stop

static void resetStatus(uint16_t dose) {
    memset(&g_cal, 0, sizeof(g_cal));
    g_cal.doseMl = dose;
}

// Ml of an unfinished run — estimate from the portion pace so far (or ml/s)
static uint16_t estimateRunMl(uint32_t runMs) {
    float mlPerMs = 0;
    if (g_cal.runs > 0) {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < g_cal.runs; i++) sum += g_cal.runMs[i];
        mlPerMs = (float)g_cal.doseMl * g_cal.runs / sum;
    } else {
        mlPerMs = currentPumpSettings.volumePerSecond / 1000.0f;
    }
    return (uint16_t)lroundf(runMs * mlPerMs);
}

static void abortCalibration(const char* note, uint32_t unfinishedRunMs) {
    if (unfinishedRunMs) subtractReserveMl(estimateRunMl(unfinishedRunMs));
    g_cal.phase = CAL_IDLE;
    g_cal.note  = note;
    LOG_WARNING("Calibration aborted — %s (portion and time unchanged)", note);
}

static const char* startRun() {
    if (!triggerPumpMs(CAL_RUN_MAX_MS, "CALIBRATION")) return "Pump start failed";
    g_runStart  = millis();
    g_cal.phase = CAL_RUNNING;
    return nullptr;
}

static void computeResult() {
    uint32_t mn = g_cal.runMs[0], mx = g_cal.runMs[0];
    double mean = 0, var = 0;
    for (uint8_t i = 0; i < CAL_RUNS; i++) {
        uint32_t t = g_cal.runMs[i];
        if (t < mn) mn = t;
        if (t > mx) mx = t;
        mean += t;
    }
    mean /= CAL_RUNS;
    for (uint8_t i = 0; i < CAL_RUNS; i++) var += (g_cal.runMs[i] - mean) * (g_cal.runMs[i] - mean);
    g_cal.minMs  = mn;
    g_cal.maxMs  = mx;
    g_cal.meanMs = (uint32_t)lround(mean);
    g_cal.sdMs   = (float)sqrt(var / (CAL_RUNS - 1));
    g_cal.note   = ((mx - mn) * 100.0 > CAL_SPREAD_WARN_PCT * mean)
                 ? "runs differ by more than 20% of the mean — consider repeating" : nullptr;

    g_cal.phase = CAL_RESULT;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("PUMP CALIBRATION RESULT — portion %u ml", g_cal.doseMl);
    for (uint8_t i = 0; i < CAL_RUNS; i++) LOG_INFO("  run %2u : %lu ms", i + 1, g_cal.runMs[i]);
    LOG_INFO("  mean   : %lu ms (min %lu, max %lu, sd %.0f ms)",
             g_cal.meanMs, g_cal.minMs, g_cal.maxMs, g_cal.sdMs);
    if (g_cal.note) LOG_WARNING("  %s", g_cal.note);
    LOG_INFO("====================================");
}

const char* startPumpCalibration(uint16_t doseMl) {
    if (g_cal.phase != CAL_IDLE)             return "Calibration already in progress";
    if (doseMl < CAL_DOSE_MIN_ML || doseMl > CAL_DOSE_MAX_ML) return "Single Dose: 1–2000 ml";
    if (!isReservoirConfigSaved())           return "Save the reservoir dimensions first";
    if (isPumpActive())                      return "Pump running — wait until it stops";
    if (isReserveRefillActive())             return "Refill measurement in progress";
    if (getReserveUsableMl() < (uint32_t)doseMl * (CAL_RUNS + 1)) {
        return "Not enough water in the reservoir for 4 portions + 1 — refill first";
    }

    setSystemState(false);   // Service Mode without the boot auto-enable timer
    resetStatus(doseMl);
    g_cal.phase = CAL_COUNTDOWN;
    g_lastEnd   = millis();
    LOG_INFO("Calibration started — portion %u ml, run 1 in %lu s",
             doseMl, CAL_START_DELAY_MS / 1000);
    return nullptr;
}

const char* pumpCalibrationAction(const String& action) {
    if (action == "cancel") {
        if (g_cal.phase == CAL_IDLE) return "No calibration in progress";
        uint32_t unfinished = 0;
        if (g_cal.phase == CAL_RUNNING) {
            g_stopping = true;
            stopPump();
            unfinished = getLastPumpRunMs();
        }
        abortCalibration("cancelled by the user", unfinished);
        g_stopping = false;
        return nullptr;
    }

    if (action == "run") {
        if (g_cal.phase != CAL_READY) return "Not ready for a run";
        if (g_cal.pauseLeftS > 0)     return "Pause between portions — wait";
        return startRun();
    }

    if (action == "stop") {
        if (g_cal.phase != CAL_RUNNING) return "No run in progress";
        g_stopping = true;
        stopPump();                                   // stop first — timing matters
        uint32_t ms = getLastPumpRunMs();
        g_cal.runMs[g_cal.runs++] = ms;
        g_lastEnd = millis();
        subtractReserveMl(g_cal.doseMl);
        LOG_INFO("Calibration run %u/%u: %lu ms", g_cal.runs, CAL_RUNS, ms);

        if (g_cal.runs < CAL_RUNS) {
            g_cal.phase = CAL_READY;
        } else {
            computeResult();
        }
        g_stopping = false;   // phase updated — loop() may look again
        return nullptr;
    }

    if (action == "confirm") {
        if (g_cal.phase != CAL_RESULT) return "No result to confirm";

        TopOffConfig cfg = waterAlgorithm.getConfig();
        cfg.dose_ml = g_cal.doseMl;
        waterAlgorithm.setConfig(cfg);

        PumpCalibration pc = {};
        pc.portion_ms = g_cal.meanMs;
        pc.ts         = getUnixTimestamp();
        pc.dose_ml    = g_cal.doseMl;
        pc.spread_ms  = (uint16_t)min<uint32_t>(g_cal.maxMs - g_cal.minMs, 65535u);
        pc.sensor_ml  = CAL_SENSOR_NONE;
        pc.runs       = CAL_RUNS;
        if (!setPumpCalibration(pc)) return "FRAM write failed";

        // ml/s at the working point — only for estimating direct pump runs
        currentPumpSettings.volumePerSecond = g_cal.doseMl * 1000.0f / g_cal.meanMs;
        saveVolumeToNVS();

        g_cal.phase = CAL_IDLE;
        g_cal.note  = "calibration saved";
        LOG_INFO("Calibration saved: %u ml = %lu ms (%.2f ml/s)",
                 g_cal.doseMl, g_cal.meanMs, currentPumpSettings.volumePerSecond);
        return nullptr;
    }

    return "Unknown action";
}

// Seconds left of a wait that started at g_lastEnd
static uint32_t secondsLeft(uint32_t waitMs) {
    uint32_t since = millis() - g_lastEnd;
    return since >= waitMs ? 0 : (waitMs - since + 999) / 1000;
}

void updatePumpCalibration() {
    switch (g_cal.phase) {
        case CAL_COUNTDOWN: {
            g_cal.pauseLeftS = secondsLeft(CAL_START_DELAY_MS);
            if (g_cal.pauseLeftS == 0) {
                const char* err = isPumpActive() ? "pump already running" : startRun();
                if (err) abortCalibration(err, 0);
            }
            break;
        }
        case CAL_READY:
            g_cal.pauseLeftS = secondsLeft(CAL_PAUSE_MS);
            break;
        case CAL_RUNNING:
            g_cal.runElapsedMs = millis() - g_runStart;
            if (!isPumpActive() && !g_stopping) {
                uint32_t ms = getLastPumpRunMs();
                abortCalibration(ms >= CAL_RUN_MAX_MS ? "run exceeded 300 s" : "pump stopped outside the calibration", ms);
            }
            break;
        default:
            break;
    }
}

const CalStatus& getCalStatus()   { return g_cal; }
bool isPumpCalibrationActive()    { return g_cal.phase != CAL_IDLE; }

bool isPumpCalibrated() {
    const PumpCalibration& pc = getReservoirConfig().cal;
    return pc.portion_ms > 0 && pc.dose_ml == waterAlgorithm.getConfig().dose_ml;
}

uint32_t getPortionMs() {
    if (isPumpCalibrated()) return getReservoirConfig().cal.portion_ms;
    // Not calibrated — ml/s model (inaccurate: start-up and hose drain not included)
    float vps = currentPumpSettings.volumePerSecond;
    if (vps <= 0.01f) return 60000;
    uint32_t ms = (uint32_t)lroundf(waterAlgorithm.getConfig().dose_ml * 1000.0f / vps);
    return ms < 100 ? 100 : ms;
}
