#include "level_test.h"
#include <math.h>
#include "level_sensor.h"
#include "pump_controller.h"
#include "../config/config.h"
#include "../config/reservoir_config.h"
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

// Latest valid series taken between pump runs (cleared on every pump start)
struct BeforeSample { float d; float sem; uint32_t ms; };
static BeforeSample g_hist[LEVEL_TEST_BEFORE_SERIES];
static uint8_t  g_histCount      = 0;   // valid entries
static uint8_t  g_histNext       = 0;   // ring write index
static bool     g_wasPumping     = false;

// "Before" — mean of g_hist at direct pump start
static bool     g_beforeOk       = false;
static uint8_t  g_beforeN        = 0;
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

static float levelMm(float distanceMm) {
    return getReservoirConfig().sensor_to_bottom_mm - distanceMm;
}

static void onPumpStart() {
    if (g_ltState == LT_SETTLING || g_ltState == LT_MEASURING) {
        LOG_WARNING("LVLTEST run %u aborted — pump restarted before measurement", g_run);
    }
    g_run++;
    g_ltState = LT_PUMPING;

    g_beforeN  = g_histCount;
    g_beforeOk = g_histCount > 0;
    if (!g_beforeOk) {
        LOG_WARNING("LVLTEST run %u: no valid 'before' reading", g_run);
        return;
    }
    double sum = 0, semSq = 0;
    for (uint8_t i = 0; i < g_histCount; i++) {
        sum   += g_hist[i].d;
        semSq += (double)g_hist[i].sem * g_hist[i].sem;
    }
    uint8_t newest = (g_histNext + LEVEL_TEST_BEFORE_SERIES - 1) % LEVEL_TEST_BEFORE_SERIES;
    g_dBefore     = (float)(sum / g_histCount);
    g_semBefore   = (float)(sqrt(semSq) / g_histCount);   // SEM of the mean
    g_beforeAgeMs = millis() - g_hist[newest].ms;
    LOG_INFO("LVLTEST run %u: before %.1f mm (level %.1f mm, %u series, newest %lu s old)",
             g_run, g_dBefore, levelMm(g_dBefore), g_beforeN, g_beforeAgeMs / 1000);
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
    float sensorMl = (dAfter - g_dBefore) * reservoirMlPerMm();          // level drop → ml
    float diffMl   = sensorMl - g_pumpMl;
    float diffPct  = (g_pumpMl > 0) ? diffMl * 100.0f / g_pumpMl : 0.0f;
    float semMl    = sqrt(g_semBefore * g_semBefore + semAfter * semAfter) * reservoirMlPerMm();

    LOG_INFO("LVLTEST,run=%u,pump_s=%.2f,pump_ml=%.0f,d_before=%.1f,d_after=%.1f,"
             "level_after=%.1f,before_age_s=%lu,before_n=%u,after_n=%u,sensor_ml=%.0f,sem_ml=%.0f,"
             "diff_ml=%.0f,diff_pct=%.1f",
             g_run, g_pumpS, g_pumpMl, g_dBefore, dAfter,
             levelMm(dAfter), g_beforeAgeMs / 1000, g_beforeN, g_afterValid, sensorMl, semMl,
             diffMl, diffPct);
    if (g_beforeAgeMs > LEVEL_TEST_BEFORE_MAX_AGE_MS) {
        LOG_WARNING("LVLTEST run %u: 'before' reading is stale (%lu s)", g_run, g_beforeAgeMs / 1000);
    }
}

static void histAdd(const LevelSeries& s) {
    g_hist[g_histNext] = { s.distance_mm, s.sem_mm, s.finished_ms };
    g_histNext = (g_histNext + 1) % LEVEL_TEST_BEFORE_SERIES;
    if (g_histCount < LEVEL_TEST_BEFORE_SERIES) g_histCount++;
}

void updateLevelTest() {
    uint32_t count = getLevelSeriesCount();

    // Any pump run (manual or algorithm) invalidates the between-runs history.
    // Series aren't taken while pumping nor in the quiet time after, so every
    // series completed outside a pump run is a settled reading.
    bool pumping = isPumpActive();
    if (pumping && !g_wasPumping) {
        // Only at the start of a manual session (it may hold several portions)
        if (isDirectPumpMode() && !g_wasDirect) onPumpStart();   // uses the history before clearing it
        g_histCount = g_histNext = 0;
    }
    g_wasPumping = pumping;
    if (!pumping && count != g_seriesSeen && getLastLevelSeries().valid) {
        histAdd(getLastLevelSeries());
    }

    bool direct = isDirectPumpMode();
    if (!direct && g_wasDirect && g_ltState == LT_PUMPING) onPumpStop();
    g_wasDirect = direct;

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
