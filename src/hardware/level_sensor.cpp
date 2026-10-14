#include "level_sensor.h"
#include <Wire.h>
#include <VL53L4CD.h>
#include <math.h>
#include "../core/logging.h"
#include "pump_controller.h"

static VL53L4CD g_tof;

static LevelSensorState g_state         = LEVEL_SENSOR_ABSENT;
static uint32_t         g_lastInitTry   = 0;
static uint8_t          g_badSeries     = 0;
static LevelSeries      g_last          = {};
static uint32_t         g_seriesCount   = 0;

// Running series accumulator
static bool     g_collecting     = false;
static bool     g_skipFirst      = false;   // first sample after idle may be stale
static uint32_t g_seriesStart    = 0;
static uint32_t g_lastSeriesEnd  = 0;
static bool     g_requested      = false;   // on-demand series — bypasses the quiet time
static bool     g_pumpWasActive  = false;
static bool     g_quiet          = false;   // inside LEVEL_PUMP_QUIET_MS after a pump stop
static uint32_t g_pumpStopMs     = 0;
static uint8_t  g_n              = 0;
static uint8_t  g_rej            = 0;
static double   g_sum            = 0;
static double   g_sumSq          = 0;
static uint32_t g_sumSignal      = 0;
static uint32_t g_sumAmbient     = 0;

static bool tryInit() {
    g_lastInitTry = millis();
    g_tof.setBus(&Wire);
    g_tof.setTimeout(200);
    if (!g_tof.init()) {
        return false;
    }
    if (!g_tof.setRangeTiming(LEVEL_TIMING_BUDGET_MS, 0)) {
        LOG_WARNING("Level sensor: setRangeTiming(%d) rejected", LEVEL_TIMING_BUDGET_MS);
    }
    g_tof.startContinuous();
    return true;
}

static void startSeries() {
    g_collecting  = true;
    g_skipFirst   = true;
    g_seriesStart = millis();
    g_n = g_rej = 0;
    g_sum = g_sumSq = 0;
    g_sumSignal = g_sumAmbient = 0;
}

static void finishSeries() {
    g_collecting     = false;
    g_requested      = false;
    g_lastSeriesEnd  = millis();

    LevelSeries s = {};
    s.samples     = g_n;
    s.rejected    = g_rej;
    s.finished_ms = g_lastSeriesEnd;
    s.valid       = (g_n >= (LEVEL_SAMPLES_PER_SERIES + 1) / 2);

    if (g_n > 0) {
        double mean = g_sum / g_n;
        double var  = (g_n > 1) ? (g_sumSq - g_n * mean * mean) / (g_n - 1) : 0.0;
        if (var < 0) var = 0;
        s.distance_mm  = (float)mean;
        s.stddev_mm    = (float)sqrt(var);
        s.sem_mm       = (float)(sqrt(var) / sqrt((double)g_n));
        s.signal_kcps  = (uint16_t)(g_sumSignal / g_n);
        s.ambient_kcps = (uint16_t)(g_sumAmbient / g_n);
    }
    g_last = s;
    g_seriesCount++;

    if (s.valid) {
        g_badSeries = 0;
        if (g_state == LEVEL_SENSOR_FAULT) LOG_INFO("Level sensor: recovered");
        g_state = LEVEL_SENSOR_OK;
        LOG_INFO("Level: %.1f mm (sd %.2f, sem %.2f) n=%u rej=%u signal=%u ambient=%u kcps",
                 s.distance_mm, s.stddev_mm, s.sem_mm, s.samples, s.rejected,
                 s.signal_kcps, s.ambient_kcps);
    } else {
        if (g_badSeries < 255) g_badSeries++;
        LOG_WARNING("Level: bad series (n=%u rej=%u, %u in a row)", s.samples, s.rejected, g_badSeries);
        if (g_badSeries >= LEVEL_FAULT_SERIES && g_state == LEVEL_SENSOR_OK) {
            g_state = LEVEL_SENSOR_FAULT;
            LOG_ERROR("Level sensor: FAULT after %u bad series", g_badSeries);
        }
        // Sensor may have lost its config (brown-out, loose wire) — re-init periodically
        if (g_state == LEVEL_SENSOR_FAULT && g_lastSeriesEnd - g_lastInitTry >= LEVEL_INIT_RETRY_MS) {
            LOG_WARNING("Level sensor: re-init %s", tryInit() ? "OK" : "failed");
        }
    }
}

void initLevelSensor() {
    if (tryInit()) {
        g_state = LEVEL_SENSOR_OK;
        LOG_INFO("Level sensor: VL53L4CD initialized (budget %d ms, N=%d, every %lu s)",
                 LEVEL_TIMING_BUDGET_MS, LEVEL_SAMPLES_PER_SERIES, LEVEL_SERIES_INTERVAL_MS / 1000);
        // First series starts from loop() — setup() takes longer than the
        // series timeout, a series started here would close empty
    } else {
        g_state = LEVEL_SENSOR_ABSENT;
        LOG_ERROR("Level sensor: VL53L4CD not detected (retry every %lu s)", LEVEL_INIT_RETRY_MS / 1000);
    }
}

void updateLevelSensor() {
    uint32_t now = millis();

    if (g_state == LEVEL_SENSOR_ABSENT) {
        if (now - g_lastInitTry >= LEVEL_INIT_RETRY_MS) {
            if (tryInit()) {
                g_state = LEVEL_SENSOR_OK;
                g_badSeries = 0;
                LOG_INFO("Level sensor: VL53L4CD detected");
                startSeries();
            }
        }
        return;
    }

    // Pumping disturbs the surface and the partitioned tank needs time to
    // equalize — no series while the pump runs, no scheduled series during the
    // quiet time after it stops (partial series dropped without logging)
    if (isPumpActive()) {
        g_collecting    = false;
        g_requested     = false;
        g_pumpWasActive = true;
        g_lastSeriesEnd = now;
        return;
    }
    if (g_pumpWasActive) {
        g_pumpWasActive = false;
        g_quiet         = true;
        g_pumpStopMs    = now;
    }
    if (g_quiet) {
        if (now - g_pumpStopMs >= LEVEL_PUMP_QUIET_MS) {
            g_quiet = false;
        } else if (!g_requested) {
            g_collecting    = false;
            g_lastSeriesEnd = now;
            return;
        }
    }

    if (!g_collecting) {
        if (now - g_lastSeriesEnd >= LEVEL_SERIES_INTERVAL_MS) startSeries();
        return;
    }

    if (now - g_seriesStart >= LEVEL_SERIES_TIMEOUT_MS) {
        finishSeries();
        return;
    }

    if (!g_tof.dataReady()) return;
    g_tof.read(false);
    if (g_tof.last_status != 0) {
        g_rej++;
        return;
    }

    if (g_skipFirst) {
        g_skipFirst = false;
        return;
    }

    const VL53L4CD::RangingData& r = g_tof.ranging_data;
    if (r.range_status != 0) {
        g_rej++;
    } else {
        g_n++;
        g_sum        += r.range_mm;
        g_sumSq      += (double)r.range_mm * r.range_mm;
        g_sumSignal  += r.signal_rate_kcps;
        g_sumAmbient += r.ambient_rate_kcps;
    }

    if (g_n >= LEVEL_SAMPLES_PER_SERIES) finishSeries();
}

void requestLevelSeries() {
    if (g_state == LEVEL_SENSOR_ABSENT) return;
    g_requested = true;
    startSeries();
}

uint32_t getLevelSeriesCount() { return g_seriesCount; }

LevelSensorState   getLevelSensorState() { return g_state; }
const LevelSeries& getLastLevelSeries()  { return g_last; }

const char* getLevelSensorStateString() {
    switch (g_state) {
        case LEVEL_SENSOR_OK:    return "ok";
        case LEVEL_SENSOR_FAULT: return "fault";
        default:                 return "absent";
    }
}
