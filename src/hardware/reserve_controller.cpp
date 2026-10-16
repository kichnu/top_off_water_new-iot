#include "reserve_controller.h"
#include <math.h>
#include "fram_controller.h"
#include "level_sensor.h"
#include "pump_controller.h"
#include "rtc_controller.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"
#include "../core/logging.h"

static ReserveState g_st       = {};
static bool         g_refilled = false;

// Refill measurement
static RefillStatus g_refill      = {};
static uint32_t     g_refillStart = 0;
static uint32_t     g_lastCount   = 0;
static uint32_t     g_lastSeries  = 0;   // millis() of the last completed / requested series
static bool         g_pending     = false;
static float        g_win[REFILL_WINDOW];
static uint8_t      g_winN        = 0;   // valid series collected (window holds the last ones)

static uint16_t currentDose() {
    uint16_t d = waterAlgorithm.getConfig().dose_ml;
    return d ? d : 1;
}

static void saveState() {
    g_st.magic = RESERVE_STATE_MAGIC;
    saveReserveStateToFRAM(g_st);
}

// Remaining ml clamped to the current full capacity (geometry or dose may have changed)
static uint32_t remainingMl(const ReservoirLevels& lv) {
    return g_st.remaining_ml > lv.fullMl ? lv.fullMl : g_st.remaining_ml;
}

static uint32_t usableMl(const ReservoirLevels& lv) {
    uint32_t r = remainingMl(lv);
    return r > lv.hiddenMl ? r - lv.hiddenMl : 0;
}

void initReserveController() {
    if (loadReserveStateFromFRAM(g_st)) {
        LOG_INFO("Reserve: %lu ml remaining, sensor reference %s",
                 g_st.remaining_ml, (g_st.flags & RESERVE_FLAG_REF_VALID) ? "set" : "none");
    } else {
        // No state yet (first boot with the portion counter) — assume full, no reference
        memset(&g_st, 0, sizeof(g_st));
        g_st.remaining_ml = reservoirLevels(currentDose()).fullMl;
        saveState();
        LOG_WARNING("Reserve state not in FRAM — counter set to full (%lu ml)", g_st.remaining_ml);
    }
}

bool isReserveEmpty() {
    uint16_t dose = currentDose();
    return usableMl(reservoirLevels(dose)) < dose;
}

bool isReserveLow() {
    const ReservoirLevels lv = reservoirLevels(currentDose());
    return !isReserveEmpty() && usableMl(lv) <= lv.lowMl;
}

void subtractReserveMl(uint16_t ml) {
    if (ml == 0) return;
    g_st.remaining_ml = ml >= g_st.remaining_ml ? 0 : g_st.remaining_ml - ml;
    saveState();
    LOG_INFO("Reserve: -%u ml → %lu ml remaining", ml, g_st.remaining_ml);
}

ReserveView getReserveView(uint16_t doseMl) {
    ReserveView v = {};
    uint32_t dose = doseMl ? doseMl : 1;
    const ReservoirLevels lv = reservoirLevels(dose);

    v.sensorCheck = useLevelSensor();
    v.capacityMl  = lv.usableMl;
    v.lowMl       = lv.lowMl;
    v.ml          = usableMl(lv);
    v.portions    = v.ml / dose;
    v.pct         = v.capacityMl ? v.ml * 100.0f / v.capacityMl : 0.0f;

    const LevelSeries& ls = getLastLevelSeries();
    if (getLevelSensorState() == LEVEL_SENSOR_OK && ls.valid && ls.finished_ms != 0) {
        float h = getReservoirConfig().sensor_to_bottom_mm - ls.distance_mm;
        v.waterMm = h > 0 ? h : 0;
    }

    if      (!isReservoirConfigSaved()) v.zone = RESERVE_ZONE_UNKNOWN;
    else if (v.ml < dose)               v.zone = RESERVE_ZONE_EMPTY;
    else if (v.ml <= v.lowMl)           v.zone = RESERVE_ZONE_LOW;
    else                                v.zone = RESERVE_ZONE_OK;
    return v;
}

// ============================================================
// REFILL
// ============================================================

static void commitFull(bool withRef) {
    g_st.remaining_ml = reservoirLevels(currentDose()).fullMl;
    if (withRef) {
        g_st.ref_dist_x10 = (uint16_t)lroundf(g_refill.distanceMm * 10.0f);
        g_st.ref_ts       = getUnixTimestamp();
        g_st.flags       |= RESERVE_FLAG_REF_VALID;
    } else {
        g_st.ref_dist_x10 = 0;
        g_st.ref_ts       = 0;
        g_st.flags       &= ~RESERVE_FLAG_REF_VALID;
    }
    saveState();
    g_refilled = true;
    LOG_INFO("Reserve refilled — counter full (%lu ml), reference %s",
             g_st.remaining_ml, withRef ? "from the measurement" : "none");
}

static void beginMeasurement() {
    memset(&g_refill, 0, sizeof(g_refill));
    g_refill.phase      = REFILL_MEASURING;
    const ReservoirConfig& c = getReservoirConfig();
    g_refill.expectedMm = (float)(c.sensor_to_bottom_mm - c.marker_level_mm);
    g_refillStart = millis();
    g_lastCount   = getLevelSeriesCount();
    g_lastSeries  = 0;
    g_pending     = false;
    g_winN        = 0;
    LOG_INFO("Refill: measuring the level (expected %.0f mm at the marker)", g_refill.expectedMm);
}

static void finishMeasurement(RefillResult res, const char* note) {
    g_refill.phase  = REFILL_RESULT;
    g_refill.result = res;
    g_refill.note   = note;

    if (res != REFILL_RES_NO_READING) {
        const ReservoirConfig& c = getReservoirConfig();
        g_refill.deviationMm = g_refill.expectedMm - g_refill.distanceMm;
        g_refill.deviationMl = (int32_t)lroundf(g_refill.deviationMm * reservoirMlPerMm());

        if (fabsf(g_refill.deviationMm) <= c.marker_tol_mm) {
            g_refill.result = REFILL_RES_OK;
            g_refill.note   = nullptr;
        } else {
            g_refill.result = REFILL_RES_MISMATCH;
            // Physically impossible reading → the sensor is the suspect, else the water
            if (g_refill.distanceMm < LEVEL_MIN_CLEARANCE_MM / 2 ||
                g_refill.distanceMm > c.sensor_to_bottom_mm + 20) {
                g_refill.sensorSuspect = true;
                g_refill.note = "reading outside the tank — sensor suspect";
            } else {
                g_refill.note = g_refill.deviationMm < 0
                    ? "level below the marker — top up, or the sensor reads too far"
                    : "level above the marker — too much water, or the sensor reads too close";
            }
        }
        LOG_INFO("Refill result: %s — %.1f mm (expected %.1f, level %+.1f mm / %+ld ml, spread %.1f mm)",
                 g_refill.result == REFILL_RES_OK ? "OK" : "MISMATCH",
                 g_refill.distanceMm, g_refill.expectedMm, g_refill.deviationMm,
                 (long)g_refill.deviationMl, g_refill.spreadMm);
    } else {
        g_refill.sensorSuspect = true;
        LOG_WARNING("Refill result: NO READING — %s", note ? note : "?");
    }
}

// Window = last REFILL_WINDOW valid series; stable → result
static void evaluateWindow() {
    if (g_winN < REFILL_WINDOW) return;
    float mn = g_win[0], mx = g_win[0], sum = 0, s1 = 0, s2 = 0;
    const uint8_t half = REFILL_WINDOW / 2;
    for (uint8_t i = 0; i < REFILL_WINDOW; i++) {
        float d = g_win[i];
        if (d < mn) mn = d;
        if (d > mx) mx = d;
        sum += d;
        if (i < half) s1 += d; else s2 += d;
    }
    g_refill.spreadMm   = mx - mn;
    g_refill.distanceMm = sum / REFILL_WINDOW;
    float trend = s2 / (REFILL_WINDOW - half) - s1 / half;
    if (g_refill.spreadMm <= REFILL_STABLE_SPREAD_MM && fabsf(trend) <= REFILL_STABLE_TREND_MM) {
        finishMeasurement(REFILL_RES_OK, nullptr);   // OK / MISMATCH decided there
    }
}

void updateReserveController() {
    if (g_refill.phase != REFILL_MEASURING) return;
    uint32_t now = millis();
    g_refill.elapsedS = (now - g_refillStart) / 1000;

    if (isPumpActive()) {
        g_refill.phase = REFILL_IDLE;
        LOG_WARNING("Refill: measurement aborted — pump started");
        return;
    }
    if (getLevelSensorState() != LEVEL_SENSOR_OK) {
        finishMeasurement(REFILL_RES_NO_READING, getLevelSensorState() == LEVEL_SENSOR_ABSENT
                          ? "sensor not detected" : "sensor fault (bad series)");
        return;
    }

    uint32_t cnt = getLevelSeriesCount();
    if (cnt != g_lastCount) {
        g_lastCount  = cnt;
        g_lastSeries = now;
        g_pending    = false;
        const LevelSeries& s = getLastLevelSeries();
        if (s.valid) {
            if (g_winN < REFILL_WINDOW) {
                g_win[g_winN++] = s.distance_mm;
            } else {
                memmove(g_win, g_win + 1, (REFILL_WINDOW - 1) * sizeof(float));
                g_win[REFILL_WINDOW - 1] = s.distance_mm;
            }
            g_refill.series = g_winN;
            evaluateWindow();
            if (g_refill.phase != REFILL_MEASURING) return;
        }
    }

    if (now - g_refillStart >= REFILL_TIMEOUT_MS) {
        finishMeasurement(REFILL_RES_NO_READING, g_winN >= REFILL_WINDOW
                          ? "level not stable within 5 min" : "too few valid series");
        return;
    }

    if (!g_pending && (g_lastSeries == 0 || now - g_lastSeries >= REFILL_SERIES_GAP_MS)) {
        requestLevelSeries();
        g_pending    = true;
        g_lastSeries = now;
    }
}

bool startReserveRefill(const char** err) {
    *err = nullptr;
    if (isPumpActive()) {
        *err = "Pump running — wait until it stops";
        return false;
    }
    if (!useLevelSensor()) {
        g_refill = {};
        commitFull(false);
        return false;
    }
    beginMeasurement();
    return true;
}

const char* reserveRefillAction(const String& action) {
    if (action == "cancel") {
        if (g_refill.phase == REFILL_IDLE) return "No refill in progress";
        g_refill.phase = REFILL_IDLE;
        LOG_INFO("Refill: cancelled — counter unchanged");
        return nullptr;
    }
    if (g_refill.phase != REFILL_RESULT) return "No result to decide on";

    if (action == "remeasure") {
        beginMeasurement();
        return nullptr;
    }
    if (action == "confirm") {
        if (g_refill.result != REFILL_RES_OK && g_refill.result != REFILL_RES_MISMATCH) {
            return "No valid reading — remeasure or accept without the sensor";
        }
        if (g_refill.result == REFILL_RES_MISMATCH) {
            LOG_WARNING("Refill: accepted despite the deviation (%+.1f mm) — reference = measurement",
                        g_refill.deviationMm);
        }
        commitFull(true);
        g_refill.phase = REFILL_IDLE;
        return nullptr;
    }
    if (action == "no_sensor") {
        ReservoirConfig c = getReservoirConfig();
        c.flags &= ~RESERVOIR_FLAG_USE_SENSOR;
        const char* e = setReservoirConfig(c);
        if (e) return e;
        LOG_WARNING("Refill: accepted without the sensor — level sensor switched off");
        commitFull(false);
        g_refill.phase = REFILL_IDLE;
        return nullptr;
    }
    return "Unknown action";
}

const RefillStatus& getRefillStatus() { return g_refill; }
bool isReserveRefillActive()          { return g_refill.phase != REFILL_IDLE; }

bool takeReserveRefilled() {
    bool r = g_refilled;
    g_refilled = false;
    return r;
}
