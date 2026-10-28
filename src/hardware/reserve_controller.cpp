#include "reserve_controller.h"
#include "level_sensor.h"
#include "pump_calibration.h"
#include "fram_controller.h"
#include "rtc_controller.h"
#include "../core/logging.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"

// g_track is changed from loop() and from web handlers (AsyncTCP task):
// changes under g_mux, FRAM writes only in updateReserveController()
static portMUX_TYPE  g_mux          = portMUX_INITIALIZER_UNLOCKED;
static ReserveTrack  g_track        = {};
static volatile bool g_dirty        = false;

static bool     g_measured       = false;   // measured mode (else estimated / no data)
static uint32_t g_ring[RESERVE_MIN_SERIES]; // last valid series, ml (minus water pumped after each)
static uint8_t  g_ringN          = 0;
static uint32_t g_seriesSeen     = 0;
static uint32_t g_lastValidMs    = 0;       // millis() of the last valid series, 0 = none since boot
static uint32_t g_lastSettlingMs = 0;       // millis() of the last pump run / quiet time
static bool     g_noReadingLogged = false;

static uint16_t currentDose() {
    uint16_t d = waterAlgorithm.getConfig().dose_ml;
    return d ? d : 1;
}

static const char* causeString(uint8_t c) {
    switch (c) {
        case RESERVE_ALARM_NONE:     return "none";
        case RESERVE_ALARM_LEVEL:    return "level at the alarm level";
        case RESERVE_ALARM_ESTIMATE: return "estimate at the alarm level (no level reading)";
        case RESERVE_ALARM_NO_DATA:  return "no level reading";
        case RESERVE_ALARM_DOSE:     return "next portion would go below the alarm level";
        default:                     return "?";
    }
}

// ml that may still be pumped above the alarm level (conservative, may be < 0)
static int32_t availableMl(const ReserveTrack& t, uint32_t alarmMl) {
    if (t.source == RESERVE_SRC_NONE) return 0;
    int32_t above = (int32_t)t.base_ml - (int32_t)alarmMl;
    if (!g_measured) above /= 2;   // estimated: every portion counts double
    return above - (int32_t)t.drawn_ml;
}

static ReserveTrack trackCopy() {
    portENTER_CRITICAL(&g_mux);
    ReserveTrack t = g_track;
    portEXIT_CRITICAL(&g_mux);
    return t;
}

static void latch(uint8_t cause) {
    bool changed = false;
    portENTER_CRITICAL(&g_mux);
    if (g_track.alarm == RESERVE_ALARM_NONE) {
        g_track.alarm = cause;
        g_dirty = changed = true;
    }
    portEXIT_CRITICAL(&g_mux);
    if (changed) LOG_WARNING("Reserve ALARM: %s — no new portion until refilled and reset", causeString(cause));
}

ReserveView getReserveView(uint16_t doseMl) {
    ReserveView v = {};
    uint32_t dose = doseMl ? doseMl : 1;
    const ReservoirLevels lv = reservoirLevels(dose);
    const ReserveTrack t = trackCopy();

    v.capacityMl       = lv.fullMl;
    v.reserveMl        = lv.reserveMl;
    v.alarmMl          = lv.alarmMl;
    v.capacityPortions = (lv.fullMl - lv.alarmMl) / dose;
    v.noReadingS       = (millis() - g_lastValidMs) / 1000;   // since boot when none yet

    if (!isReservoirConfigSaved() || t.source == RESERVE_SRC_NONE) {
        v.zone = RESERVE_ZONE_UNKNOWN;
        return v;
    }
    v.reading   = g_measured;
    v.estimated = !g_measured;
    v.availMl   = availableMl(t, lv.alarmMl);
    v.portions  = v.availMl > 0 ? v.availMl / dose : 0;
    if (g_measured) {
        v.ml = t.base_ml > t.drawn_ml ? t.base_ml - t.drawn_ml : 0;
    } else {
        v.ml = lv.alarmMl + (v.availMl > 0 ? v.availMl : 0);
    }
    float mlPerMm = reservoirMlPerMm();
    v.waterMm = mlPerMm > 0 ? v.ml / mlPerMm : 0;
    v.pct     = v.capacityMl ? v.ml * 100.0f / v.capacityMl : 0.0f;

    if      (v.availMl <= 0)       v.zone = RESERVE_ZONE_ALARM;
    else if (v.ml <= lv.reserveMl) v.zone = RESERVE_ZONE_RESERVE;
    else                           v.zone = RESERVE_ZONE_OK;
    return v;
}

bool isReserveLow() {
    ReserveZone z = getReserveView(currentDose()).zone;
    return z == RESERVE_ZONE_RESERVE || z == RESERVE_ZONE_ALARM;
}

bool isReserveEmpty()                    { return g_track.alarm != RESERVE_ALARM_NONE; }
ReserveAlarmCause getReserveAlarmCause() { return (ReserveAlarmCause)g_track.alarm; }
const char* getReserveAlarmString()      { return causeString(g_track.alarm); }

bool isReserveNoReadingAlert() {
    return !g_measured && millis() - g_lastValidMs >= RESERVE_NO_READING_ALERT_MS;
}

bool reserveCanDose() {
    const ReserveTrack t = trackCopy();
    if (t.alarm != RESERVE_ALARM_NONE || t.source == RESERVE_SRC_NONE) return false;
    return availableMl(t, reservoirLevels(currentDose()).alarmMl) >= (int32_t)currentDose();
}

bool reserveCheckDose() {
    if (reserveCanDose()) return true;
    const ReserveTrack t = trackCopy();
    if (t.alarm == RESERVE_ALARM_NONE) {
        latch(t.source == RESERVE_SRC_NONE ? RESERVE_ALARM_NO_DATA : RESERVE_ALARM_DOSE);
    }
    return false;
}

void reserveOnPumpRun(uint32_t runMs, bool calibration) {
    uint32_t dose = currentDose();
    uint32_t portionMs = getPortionMs();
    // A calibration run ends at the vessel mark = one portion; otherwise by run time
    uint32_t ml = calibration ? dose
                : (portionMs ? (uint32_t)((uint64_t)runMs * dose / portionMs) : 0);
    if (ml == 0) return;
    portENTER_CRITICAL(&g_mux);
    g_track.drawn_ml += ml;
    // Readings in the ring predate this run — otherwise the next base (their min)
    // would drop the run once drawn_ml is reset
    for (uint8_t i = 0; i < g_ringN; i++) g_ring[i] = g_ring[i] > ml ? g_ring[i] - ml : 0;
    g_dirty = true;
    portEXIT_CRITICAL(&g_mux);
    LOG_INFO("Reserve: -%lu ml pumped (%lu ml since the last reading)",
             (unsigned long)ml, (unsigned long)g_track.drawn_ml);
}

void clearReserveLatch() {
    const ReserveTrack t = trackCopy();
    if (t.alarm == RESERVE_ALARM_NONE) return;
    uint32_t dose = currentDose();
    if (g_measured && availableMl(t, reservoirLevels(dose).alarmMl) >= (int32_t)dose) {
        portENTER_CRITICAL(&g_mux);
        g_track.alarm = RESERVE_ALARM_NONE;
        g_dirty = true;
        portEXIT_CRITICAL(&g_mux);
        LOG_INFO("Reserve alarm cleared");
    } else {
        LOG_WARNING("Reserve alarm stays: %s",
                    g_measured ? "level below alarm + 1 portion — refill first"
                               : "no level reading — use \"reservoir refilled\" after refilling");
    }
}

const char* reserveRefilled() {
    if (!isReservoirConfigSaved()) return "Save the reservoir dimensions first";
    if (g_measured) return "The level sensor is reading — the level is measured, not needed";
    uint32_t full = reservoirLevels(currentDose()).fullMl;
    uint32_t ts   = getUnixTimestamp();
    portENTER_CRITICAL(&g_mux);
    g_track.base_ml  = full;
    g_track.base_ts  = ts;
    g_track.drawn_ml = 0;
    g_track.source   = RESERVE_SRC_REFILL;
    g_track.alarm    = RESERVE_ALARM_NONE;
    g_dirty = true;
    portEXIT_CRITICAL(&g_mux);
    LOG_WARNING("Reserve: refilled to the marker by the user (%lu ml) — estimate restarted",
                (unsigned long)full);
    return nullptr;
}

void initReserveController() {
    ReserveTrack t;
    if (loadReserveTrackFromFRAM(t) && t.magic == RESERVE_TRACK_MAGIC) {
        g_track = t;
        LOG_INFO("Reserve: last %s %lu ml at %lu, %lu ml pumped since, alarm: %s",
                 t.source == RESERVE_SRC_REFILL ? "refill" : "reading",
                 (unsigned long)t.base_ml, (unsigned long)t.base_ts,
                 (unsigned long)t.drawn_ml, causeString(t.alarm));
    } else {
        memset(&g_track, 0, sizeof(g_track));
        g_track.magic = RESERVE_TRACK_MAGIC;
        LOG_WARNING("Reserve: no saved level — auto blocked until the first reading");
    }
    g_measured = false;   // estimated until RESERVE_MIN_SERIES valid series
    g_ringN    = 0;
}

// New completed level series → ring, mode, base
static void onSeries(uint32_t now) {
    const LevelSeries& s = getLastLevelSeries();
    bool ok = isReservoirConfigSaved() && getLevelSensorState() == LEVEL_SENSOR_OK &&
              s.valid && !isLevelSettling();
    if (!ok) {
        if (!g_measured) g_ringN = 0;   // back to measured only after N valid in a row
        return;
    }
    float h = getReservoirConfig().sensor_to_bottom_mm - s.distance_mm;
    uint32_t ml = (uint32_t)((h > 0 ? h : 0) * reservoirMlPerMm());
    portENTER_CRITICAL(&g_mux);
    if (g_ringN < RESERVE_MIN_SERIES) g_ringN++;
    for (uint8_t i = RESERVE_MIN_SERIES - 1; i > 0; i--) g_ring[i] = g_ring[i - 1];
    g_ring[0] = ml;
    portEXIT_CRITICAL(&g_mux);
    g_lastValidMs = now;

    if (!g_measured) {
        if (g_ringN < RESERVE_MIN_SERIES) return;
        g_measured = true;
        g_noReadingLogged = false;
        LOG_INFO("Reserve: level readings back — measured mode");
    }
    uint32_t ts = getUnixTimestamp();
    portENTER_CRITICAL(&g_mux);
    uint32_t mn = g_ring[0];
    for (uint8_t i = 1; i < g_ringN; i++) if (g_ring[i] < mn) mn = g_ring[i];
    g_track.base_ml  = mn;
    g_track.base_ts  = ts;
    g_track.drawn_ml = 0;
    g_track.source   = RESERVE_SRC_SENSOR;
    g_dirty = true;
    portEXIT_CRITICAL(&g_mux);
}

void updateReserveController() {
    uint32_t now = millis();
    if (isLevelSettling()) g_lastSettlingMs = now;

    uint32_t cnt = getLevelSeriesCount();
    if (cnt != g_seriesSeen) {
        g_seriesSeen = cnt;
        onSeries(now);
    }

    // No valid reading for a while outside the pump quiet time → estimated mode
    uint32_t ref = g_lastValidMs > g_lastSettlingMs ? g_lastValidMs : g_lastSettlingMs;
    if (g_measured && now - ref >= RESERVE_STALE_MS) {
        g_measured = false;
        g_ringN    = 0;
        LOG_WARNING("Reserve: no level reading for %lu s — estimating from the last reading",
                    (unsigned long)((now - g_lastValidMs) / 1000));
    }
    if (!g_noReadingLogged && isReserveNoReadingAlert()) {
        g_noReadingLogged = true;
        LOG_WARNING("Reserve: no level reading for 30 min");
    }

    // Alarm latch
    const ReserveTrack t = trackCopy();
    if (t.alarm == RESERVE_ALARM_NONE) {
        if (t.source == RESERVE_SRC_NONE || !isReservoirConfigSaved()) {
            if (now >= RESERVE_NO_DATA_GRACE_MS) latch(RESERVE_ALARM_NO_DATA);
        } else if (availableMl(t, reservoirLevels(currentDose()).alarmMl) <= 0) {
            latch(g_measured ? RESERVE_ALARM_LEVEL : RESERVE_ALARM_ESTIMATE);
        }
    }

    if (g_dirty) {
        portENTER_CRITICAL(&g_mux);
        ReserveTrack w = g_track;
        g_dirty = false;
        portEXIT_CRITICAL(&g_mux);
        if (!saveReserveTrackToFRAM(w)) LOG_WARNING("Reserve: FRAM save failed");
    }
}
