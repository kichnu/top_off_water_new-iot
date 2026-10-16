#include "level_stable.h"
#include <math.h>
#include "level_sensor.h"
#include "pump_controller.h"
#include "../core/logging.h"

static StableLevel g_sl         = {};
static uint32_t    g_start      = 0;
static uint32_t    g_lastCount  = 0;
static uint32_t    g_lastSeries = 0;   // millis() of the last completed / requested series
static bool        g_pending    = false;
static float       g_win[STABLE_WINDOW];
static uint8_t     g_winN       = 0;

static void fail(const char* note) {
    g_sl.state = STABLE_FAILED;
    g_sl.note  = note;
    LOG_WARNING("Stable level: failed — %s", note);
}

void stableLevelStart(bool waitQuiet) {
    memset(&g_sl, 0, sizeof(g_sl));
    g_sl.state   = waitQuiet ? STABLE_WAIT : STABLE_RUNNING;
    g_start      = millis();
    g_lastCount  = getLevelSeriesCount();
    g_lastSeries = 0;
    g_pending    = false;
    g_winN       = 0;
}

void stableLevelCancel() {
    g_sl.state = STABLE_IDLE;
}

// Window = last STABLE_WINDOW valid series; stable → done
static void evaluateWindow() {
    if (g_winN < STABLE_WINDOW) return;
    float mn = g_win[0], mx = g_win[0], sum = 0, s1 = 0, s2 = 0;
    const uint8_t half = STABLE_WINDOW / 2;
    for (uint8_t i = 0; i < STABLE_WINDOW; i++) {
        float d = g_win[i];
        if (d < mn) mn = d;
        if (d > mx) mx = d;
        sum += d;
        if (i < half) s1 += d; else s2 += d;
    }
    g_sl.spreadMm   = mx - mn;
    g_sl.distanceMm = sum / STABLE_WINDOW;
    float trend = s2 / (STABLE_WINDOW - half) - s1 / half;
    if (g_sl.spreadMm <= STABLE_SPREAD_MM && fabsf(trend) <= STABLE_TREND_MM) {
        g_sl.state = STABLE_DONE;
        LOG_INFO("Stable level: %.1f mm (spread %.1f mm, %lu s)",
                 g_sl.distanceMm, g_sl.spreadMm, g_sl.elapsedS);
    }
}

void updateStableLevel() {
    if (g_sl.state != STABLE_WAIT && g_sl.state != STABLE_RUNNING) return;
    uint32_t now = millis();
    g_sl.elapsedS = (now - g_start) / 1000;

    if (isPumpActive())                            { fail("pump started"); return; }
    if (getLevelSensorState() == LEVEL_SENSOR_ABSENT) { fail("sensor not detected"); return; }
    if (getLevelSensorState() == LEVEL_SENSOR_FAULT)  { fail("sensor fault (bad series)"); return; }

    if (g_sl.state == STABLE_WAIT) {
        if (isLevelSettling()) return;
        g_sl.state  = STABLE_RUNNING;
        g_start     = now;              // timeout counts from here
        g_lastCount = getLevelSeriesCount();
    }

    uint32_t cnt = getLevelSeriesCount();
    if (cnt != g_lastCount) {
        g_lastCount  = cnt;
        g_lastSeries = now;
        g_pending    = false;
        const LevelSeries& s = getLastLevelSeries();
        if (s.valid) {
            if (g_winN < STABLE_WINDOW) {
                g_win[g_winN++] = s.distance_mm;
            } else {
                memmove(g_win, g_win + 1, (STABLE_WINDOW - 1) * sizeof(float));
                g_win[STABLE_WINDOW - 1] = s.distance_mm;
            }
            g_sl.series = g_winN;
            evaluateWindow();
            if (g_sl.state != STABLE_RUNNING) return;
        }
    }

    if (now - g_start >= STABLE_TIMEOUT_MS) {
        fail(g_winN >= STABLE_WINDOW ? "level not stable within 5 min" : "too few valid series");
        return;
    }

    if (!g_pending && (g_lastSeries == 0 || now - g_lastSeries >= STABLE_SERIES_GAP_MS)) {
        requestLevelSeries();
        g_pending    = true;
        g_lastSeries = now;
    }
}

const StableLevel& getStableLevel() { return g_sl; }
