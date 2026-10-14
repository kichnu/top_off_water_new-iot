#include "level_test.h"
#include <math.h>
#include "level_sensor.h"
#include "pump_controller.h"
#include "../config/config.h"
#include "../core/logging.h"

enum LevelTestState : uint8_t {
    LT_IDLE,
    LT_PUMPING,
    LT_SETTLING,
    LT_MEASURING,
};

static LevelTestState g_ltState = LT_IDLE;
static bool     g_wasDirect      = false;
static uint16_t g_run            = 0;

// "Before" — last valid series at pump start
static bool     g_beforeOk       = false;
static float    g_dBefore        = 0;
static float    g_semBefore      = 0;
static uint32_t g_beforeAgeMs    = 0;

static float    g_pumpS          = 0;
static float    g_pumpMl         = 0;
static uint32_t g_stopMs         = 0;

// "After" — mean of LEVEL_TEST_AFTER_SERIES series
static uint32_t g_seriesSeen     = 0;
static uint8_t  g_afterDone      = 0;
static uint8_t  g_afterValid     = 0;
static double   g_dAfterSum      = 0;
static double   g_semAfterSqSum  = 0;

static float levelMm(float distanceMm) { return LEVEL_SENSOR_TO_BOTTOM_MM - distanceMm; }

static void onPumpStart() {
    if (g_ltState == LT_SETTLING || g_ltState == LT_MEASURING) {
        LOG_WARNING("LVLTEST run %u aborted — pump restarted before measurement", g_run);
    }
    g_run++;
    g_ltState = LT_PUMPING;

    const LevelSeries& s = getLastLevelSeries();
    g_beforeOk    = s.valid;
    g_dBefore     = s.distance_mm;
    g_semBefore   = s.sem_mm;
    g_beforeAgeMs = millis() - s.finished_ms;
    if (!s.valid) {
        LOG_WARNING("LVLTEST run %u: no valid 'before' reading", g_run);
    } else {
        LOG_INFO("LVLTEST run %u: before %.1f mm (level %.1f mm, age %lu s)",
                 g_run, g_dBefore, levelMm(g_dBefore), g_beforeAgeMs / 1000);
    }
}

static void onPumpStop() {
    uint32_t runMs = getLastDirectPumpRunMs();
    g_pumpS  = runMs / 1000.0f;
    g_pumpMl = g_pumpS * currentPumpSettings.volumePerSecond;
    g_stopMs = millis();
    g_ltState = LT_SETTLING;
    LOG_INFO("LVLTEST run %u: pump %.2f s ~ %.0f ml, settling %lu s",
             g_run, g_pumpS, g_pumpMl, LEVEL_TEST_SETTLE_MS / 1000);
}

static void finishRun() {
    g_ltState = LT_IDLE;
    if (!g_beforeOk || g_afterValid == 0) {
        LOG_WARNING("LVLTEST run %u: no result (before %s, after valid %u/%u)",
                    g_run, g_beforeOk ? "ok" : "missing", g_afterValid, g_afterDone);
        return;
    }

    float dAfter   = (float)(g_dAfterSum / g_afterValid);
    float semAfter = (float)(sqrt(g_semAfterSqSum) / g_afterValid);   // SEM of the mean
    float sensorMl = (dAfter - g_dBefore) * LEVEL_ML_PER_MM;          // level drop → ml
    float diffMl   = sensorMl - g_pumpMl;
    float diffPct  = (g_pumpMl > 0) ? diffMl * 100.0f / g_pumpMl : 0.0f;
    float semMl    = sqrt(g_semBefore * g_semBefore + semAfter * semAfter) * LEVEL_ML_PER_MM;

    LOG_INFO("LVLTEST,run=%u,pump_s=%.2f,pump_ml=%.0f,d_before=%.1f,d_after=%.1f,"
             "level_after=%.1f,before_age_s=%lu,after_n=%u,sensor_ml=%.0f,sem_ml=%.0f,"
             "diff_ml=%.0f,diff_pct=%.1f",
             g_run, g_pumpS, g_pumpMl, g_dBefore, dAfter,
             levelMm(dAfter), g_beforeAgeMs / 1000, g_afterValid, sensorMl, semMl,
             diffMl, diffPct);
    if (g_beforeAgeMs > LEVEL_TEST_BEFORE_MAX_AGE_MS) {
        LOG_WARNING("LVLTEST run %u: 'before' reading is stale (%lu s)", g_run, g_beforeAgeMs / 1000);
    }
}

void updateLevelTest() {
    bool direct = isDirectPumpMode();
    if (direct && !g_wasDirect) onPumpStart();
    if (!direct && g_wasDirect && g_ltState == LT_PUMPING) onPumpStop();
    g_wasDirect = direct;

    uint32_t count = getLevelSeriesCount();

    switch (g_ltState) {
        case LT_SETTLING:
            if (millis() - g_stopMs >= LEVEL_TEST_SETTLE_MS) {
                g_afterDone = g_afterValid = 0;
                g_dAfterSum = g_semAfterSqSum = 0;
                g_ltState = LT_MEASURING;
                requestLevelSeries();
            }
            break;

        case LT_MEASURING:
            if (count != g_seriesSeen) {
                const LevelSeries& s = getLastLevelSeries();
                g_afterDone++;
                if (s.valid) {
                    g_afterValid++;
                    g_dAfterSum     += s.distance_mm;
                    g_semAfterSqSum += (double)s.sem_mm * s.sem_mm;
                }
                if (g_afterDone >= LEVEL_TEST_AFTER_SERIES) finishRun();
                else requestLevelSeries();
            }
            break;

        default:
            break;
    }

    g_seriesSeen = count;
}
