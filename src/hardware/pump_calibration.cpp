#include "pump_calibration.h"
#include <math.h>
#include "pump_controller.h"
#include "level_sensor.h"
#include "level_stable.h"
#include "reserve_controller.h"
#include "rtc_controller.h"
#include "../config/config.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"
#include "../core/logging.h"

static CalStatus     g_cal       = {};
static uint32_t      g_runStart  = 0;
static uint32_t      g_lastEnd   = 0;      // millis() when the last run stopped
static volatile bool g_stopping  = false;  // web "stop" in progress — not an external stop

static void resetStatus(uint16_t dose) {
    memset(&g_cal, 0, sizeof(g_cal));
    g_cal.doseMl   = dose;
    g_cal.beforeMm = NAN;
    g_cal.afterMm  = NAN;
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
    stableLevelCancel();
    g_cal.phase = CAL_IDLE;
    g_cal.note  = note;
    LOG_WARNING("Calibration aborted — %s (portion and time unchanged)", note);
}

static void computeResult() {
    uint32_t t[CAL_RUNS];
    memcpy(t, g_cal.runMs, sizeof(t));
    for (uint8_t i = 1; i < CAL_RUNS; i++) {          // insertion sort, N = 10
        uint32_t v = t[i];
        int8_t j = i - 1;
        while (j >= 0 && t[j] > v) { t[j + 1] = t[j]; j--; }
        t[j + 1] = v;
    }
    g_cal.minMs    = t[0];
    g_cal.maxMs    = t[CAL_RUNS - 1];
    g_cal.medianMs = (CAL_RUNS % 2) ? t[CAL_RUNS / 2]
                                    : (t[CAL_RUNS / 2 - 1] + t[CAL_RUNS / 2]) / 2;
    double mean = 0, var = 0;
    for (uint8_t i = 0; i < CAL_RUNS; i++) mean += t[i];
    mean /= CAL_RUNS;
    for (uint8_t i = 0; i < CAL_RUNS; i++) var += (t[i] - mean) * (t[i] - mean);
    g_cal.sdMs = (float)sqrt(var / (CAL_RUNS - 1));

    g_cal.hasSensorMl = !isnan(g_cal.beforeMm) && !isnan(g_cal.afterMm);
    if (g_cal.hasSensorMl) {
        g_cal.sensorMl = (int32_t)lroundf((g_cal.afterMm - g_cal.beforeMm) * reservoirMlPerMm());
    }

    g_cal.phase = CAL_RESULT;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("PUMP CALIBRATION RESULT — portion %u ml", g_cal.doseMl);
    for (uint8_t i = 0; i < CAL_RUNS; i++) LOG_INFO("  run %2u : %lu ms", i + 1, g_cal.runMs[i]);
    LOG_INFO("  median : %lu ms (min %lu, max %lu, sd %.0f ms)",
             g_cal.medianMs, g_cal.minMs, g_cal.maxMs, g_cal.sdMs);
    if (g_cal.hasSensorMl) {
        uint32_t expected = (uint32_t)g_cal.doseMl * CAL_RUNS;
        LOG_INFO("  sensor : %.1f → %.1f mm, %ld ml vs %lu ml (%.0f%%)",
                 g_cal.beforeMm, g_cal.afterMm, (long)g_cal.sensorMl, expected,
                 g_cal.sensorMl * 100.0f / expected);
    } else {
        LOG_INFO("  sensor : no data");
    }
    LOG_INFO("====================================");
}

const char* startPumpCalibration(uint16_t doseMl) {
    if (g_cal.phase != CAL_IDLE)             return "Calibration already in progress";
    if (doseMl < CAL_DOSE_MIN_ML || doseMl > CAL_DOSE_MAX_ML) return "Single Dose: 1–2000 ml";
    if (!isReservoirConfigSaved())           return "Save the reservoir dimensions first";
    if (isPumpActive())                      return "Pump running — wait until it stops";
    if (isReserveRefillActive())             return "Refill measurement in progress";
    if (getReserveUsableMl() < (uint32_t)doseMl * (CAL_RUNS + 1)) {
        return "Not enough water in the reservoir for 10 portions + 1 — refill first";
    }

    setSystemState(false);   // Service Mode without the boot auto-enable timer
    resetStatus(doseMl);

    g_cal.sensor = useLevelSensor() && getLevelSensorState() == LEVEL_SENSOR_OK;
    if (g_cal.sensor) {
        stableLevelStart(true);
        g_cal.phase = CAL_BEFORE;
    } else {
        g_cal.phase = CAL_READY;
    }
    g_lastEnd = 0;
    LOG_INFO("Calibration started — portion %u ml, level sensor %s",
             doseMl, g_cal.sensor ? "measuring before" : "not used");
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
        if (!triggerPumpMs(CAL_RUN_MAX_MS, "CALIBRATION")) return "Pump start failed";
        g_runStart  = millis();
        g_cal.phase = CAL_RUNNING;
        return nullptr;
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
        } else if (g_cal.sensor) {
            stableLevelStart(true);                   // quiet time first
            g_cal.phase = CAL_AFTER;
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
        pc.portion_ms = g_cal.medianMs;
        pc.ts         = getUnixTimestamp();
        pc.dose_ml    = g_cal.doseMl;
        pc.spread_ms  = (uint16_t)min<uint32_t>(g_cal.maxMs - g_cal.minMs, 65535u);
        pc.sensor_ml  = g_cal.hasSensorMl ? (int16_t)constrain(g_cal.sensorMl, -32767L, 32767L)
                                          : CAL_SENSOR_NONE;
        pc.runs       = CAL_RUNS;
        if (!setPumpCalibration(pc)) return "FRAM write failed";

        // ml/s at the working point — only for estimating direct pump runs
        currentPumpSettings.volumePerSecond = g_cal.doseMl * 1000.0f / g_cal.medianMs;
        saveVolumeToNVS();

        if (!isnan(g_cal.afterMm)) setReserveReference(g_cal.afterMm);

        g_cal.phase = CAL_IDLE;
        g_cal.note  = "calibration saved";
        LOG_INFO("Calibration saved: %u ml = %lu ms (%.2f ml/s)",
                 g_cal.doseMl, g_cal.medianMs, currentPumpSettings.volumePerSecond);
        return nullptr;
    }

    return "Unknown action";
}

void updatePumpCalibration() {
    switch (g_cal.phase) {
        case CAL_BEFORE: {
            const StableLevel& sl = getStableLevel();
            if (sl.state == STABLE_DONE) {
                g_cal.beforeMm = sl.distanceMm;
                g_cal.phase    = CAL_READY;
            } else if (sl.state == STABLE_FAILED) {
                g_cal.sensor = false;
                g_cal.note   = "level before not measured — continuing without sensor data";
                g_cal.phase  = CAL_READY;
                LOG_WARNING("Calibration: %s (%s)", g_cal.note, sl.note);
            }
            break;
        }
        case CAL_READY: {
            uint32_t pauseMs = getReservoirConfig().portion_pause_s * 1000UL;
            uint32_t since   = millis() - g_lastEnd;
            g_cal.pauseLeftS = (g_lastEnd == 0 || since >= pauseMs) ? 0 : (pauseMs - since + 999) / 1000;
            break;
        }
        case CAL_RUNNING:
            g_cal.runElapsedMs = millis() - g_runStart;
            if (!isPumpActive() && !g_stopping) {
                uint32_t ms = getLastPumpRunMs();
                abortCalibration(ms >= CAL_RUN_MAX_MS ? "run exceeded 300 s" : "pump stopped outside the calibration", ms);
            }
            break;
        case CAL_AFTER: {
            const StableLevel& sl = getStableLevel();
            if (sl.state == STABLE_DONE) {
                g_cal.afterMm = sl.distanceMm;
                computeResult();
            } else if (sl.state == STABLE_FAILED) {
                g_cal.note = "level after not measured — result without sensor data";
                LOG_WARNING("Calibration: %s (%s)", g_cal.note, sl.note);
                computeResult();
            }
            break;
        }
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
