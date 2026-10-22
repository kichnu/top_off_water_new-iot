#include "level_sensor.h"
#include <Wire.h>
#include <VL53L4CD.h>
#include <math.h>
#include <stdarg.h>
#include "../core/logging.h"
#include "pump_controller.h"
#include "hardware_pins.h"
#include "fram_controller.h"
#include "level_event_log.h"

static VL53L4CD g_tof;

static LevelSensorState g_state         = LEVEL_SENSOR_ABSENT;
static uint32_t         g_lastInitTry   = 0;
static uint8_t          g_badSeries     = 0;
static LevelSeries      g_last          = {};
static uint32_t         g_seriesCount   = 0;
static uint32_t         g_lastGoodMs    = 0;   // millis() of the last valid series (0 = none)
static uint32_t         g_lastPumpStop  = 0;   // millis() of the last pump stop (0 = none)
static bool             g_recoveryDone  = false;
static uint32_t         g_lastRecovery  = 0;
static uint8_t          g_retries       = 0;   // re-init attempts since the last valid series
static uint8_t          g_faultEvents   = 0;   // events stored in FRAM during the current FAULT

#define LEVEL_FAULT_EVENTS_MAX   8   // retry/recovery events stored per FAULT (FAULT + recovered always)

enum EventLevel : uint8_t { EV_INFO, EV_WARN, EV_ERROR };

// Log a line and optionally store it in the FRAM event log
static void logEvent(EventLevel lvl, bool store, const char* fmt, ...) {
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    switch (lvl) {
        case EV_INFO:  LOG_INFO("%s", buf);    break;
        case EV_WARN:  LOG_WARNING("%s", buf); break;
        default:       LOG_ERROR("%s", buf);   break;
    }
    if (store) addLevelEvent(buf);
}

// Retry/recovery events are capped per FAULT so a long outage cannot flush the log
static bool storeFaultEvent() {
    if (g_faultEvents >= LEVEL_FAULT_EVENTS_MAX) return false;
    g_faultEvents++;
    return true;
}

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
// Rejection breakdown for the bad-series log
static uint8_t  g_i2cErr         = 0;       // reads with an I2C error
static uint8_t  g_statusCnt[16]  = {};      // per range_status (0..14, 15 = other/255)
static VL53L4CD::RangingData g_lastRej = {};
// Data-ready polling — tells "sensor stopped ranging" from "sensor not answering"
static uint16_t g_polls          = 0;       // GPIO__TIO_HV_STATUS reads in the series
static uint16_t g_pollFail       = 0;       // of which failed on I2C
static uint8_t  g_pollErr        = 0;       // last failure: endTransmission() code, 0xFE = short read
static uint8_t  g_lastHv         = 0xFF;    // last GPIO__TIO_HV_STATUS read successfully

// 8-bit register read keeping the I2C result (the library returns 0xFF on a failed
// read, which dataReady() takes as "not ready"). err: endTransmission() code, 0xFE = short read
static bool readRegChecked(uint16_t reg, uint8_t& value, uint8_t& err) {
    Wire.beginTransmission(g_tof.getAddress());
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)reg);
    err = Wire.endTransmission();
    if (err != 0) return false;
    if (Wire.requestFrom(g_tof.getAddress(), (uint8_t)1) != 1) {
        err = 0xFE;
        return false;
    }
    value = Wire.read();
    return true;
}

// Register as 2 hex digits, "--" when the read failed
static const char* regHex(uint16_t reg, char* buf) {
    uint8_t v, err;
    if (readRegChecked(reg, v, err)) snprintf(buf, 3, "%02X", v);
    else strcpy(buf, "--");
    return buf;
}

// Snapshot of the ranging state, stored at boot as the healthy reference and in a
// no-data series (start: SYSTEM_START readback, written 21 = continuous, 80 = stop;
// fw: 03 = booted; mux: interrupt polarity; int: interrupt config; rs: range status)
static void regSnapshot(char* buf, size_t len) {
    char rStart[3], rFw[3], rMux[3], rInt[3], rRs[3];
    snprintf(buf, len, "start=%s fw=%s mux=%s int=%s rs=%s",
             regHex(VL53L4CD::SYSTEM_START, rStart), regHex(VL53L4CD::FIRMWARE__SYSTEM_STATUS, rFw),
             regHex(VL53L4CD::GPIO_HV_MUX__CTRL, rMux), regHex(VL53L4CD::SYSTEM__INTERRUPT, rInt),
             regHex(VL53L4CD::RESULT__RANGE_STATUS, rRs));
}

// Hardware reset: XSHUT low puts the sensor into HW standby, releasing it reboots
// its firmware (like a power cycle). No-op when XSHUT is not wired.
static void hardReset() {
    digitalWrite(LEVEL_XSHUT_PIN, LOW);
    delay(10);
    digitalWrite(LEVEL_XSHUT_PIN, HIGH);   // released — the module pull-up enables the sensor
    delay(5);                              // boot (tBOOT ~1.2 ms)
}

static bool tryInit() {
    g_lastInitTry = millis();
    g_tof.setBus(&Wire);
    g_tof.setTimeout(200);
    // init() neither stops ranging nor resets the sensor, so a re-init would only
    // rewrite the config of a running (possibly wedged) device. Stop ranging and
    // soft-reset first (SOFT_RESET 0x0000: 0 → 1, as in the ST ULD); harmless
    // when the sensor is absent (NACK)
    g_tof.stopContinuous();
    g_tof.writeReg(VL53L4CD::SOFT_RESET, 0x00);
    delayMicroseconds(100);
    g_tof.writeReg(VL53L4CD::SOFT_RESET, 0x01);
    delay(5);   // boot (tBOOT ~1.2 ms); init() then waits for FIRMWARE__SYSTEM_STATUS
    if (!g_tof.init()) {
        return false;
    }
    if (!g_tof.setRangeTiming(LEVEL_TIMING_BUDGET_MS, 0)) {
        LOG_WARNING("Level sensor: setRangeTiming(%d) rejected", LEVEL_TIMING_BUDGET_MS);
    }
    g_tof.startContinuous();
    return true;
}

// Seconds since a millis() timestamp, -1 when it never happened
static long secondsSince(uint32_t ts, uint32_t now) {
    return ts ? (long)((now - ts) / 1000) : -1;
}

// endTransmission() result: 0 = ACK, 2 = NACK on address, 3 = NACK on data, 4/5 = bus error/timeout
static uint8_t probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission();
}

// Full I2C recovery as in setup(): release the bus (9 SCL pulses + STOP when a slave
// holds SDA low), restart the Wire driver, re-register FRAM, re-init the sensor
static bool recoverBus() {
    Wire.end();
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(10);
    bool sdaLow = digitalRead(I2C_SDA_PIN) == LOW;
    bool sclLow = digitalRead(I2C_SCL_PIN) == LOW;
    uint8_t pulses = 0;
    if (sdaLow && !sclLow) {
        pinMode(I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
        while (pulses < 9 && digitalRead(I2C_SDA_PIN) == LOW) {
            digitalWrite(I2C_SCL_PIN, LOW);  delayMicroseconds(5);
            digitalWrite(I2C_SCL_PIN, HIGH); delayMicroseconds(5);
            pulses++;
        }
        // STOP: SDA low → high while SCL is high
        pinMode(I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
        digitalWrite(I2C_SDA_PIN, LOW);  delayMicroseconds(5);
        digitalWrite(I2C_SDA_PIN, HIGH); delayMicroseconds(5);
        pinMode(I2C_SCL_PIN, INPUT_PULLUP);
        pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    }
    bool sdaFreed = digitalRead(I2C_SDA_PIN) == HIGH;

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);
    delay(10);
    framReconnect();
    bool ok = tryInit();
    logEvent(EV_WARN, storeFaultEvent(),
             "Level sensor: bus recovery %s (SDA %s, SCL %s, %u pulses, SDA now %s; ACK tof=%u fram=%u rtc=%u)",
             ok ? "OK" : "failed", sdaLow ? "LOW" : "high", sclLow ? "LOW" : "high", pulses,
             sdaFreed ? "high" : "LOW", probe(0x29), probe(0x50), probe(0x68));
    return ok;
}

// Called in FAULT every LEVEL_INIT_RETRY_MS: re-init with a soft reset first, with
// an XSHUT hardware reset on later attempts, then a full bus recovery (pump off,
// at most every LEVEL_BUS_RECOVERY_MS)
static void retryInit(uint32_t now) {
    bool hw = g_retries > 0;
    if (g_retries < 255) g_retries++;
    if (hw) hardReset();
    const char* how = hw ? "XSHUT" : "soft";
    if (tryInit()) {
        logEvent(EV_WARN, storeFaultEvent(), "Level sensor: re-init OK (%s reset, down %ld s)",
                 how, secondsSince(g_lastGoodMs, now));
        return;
    }
    logEvent(EV_WARN, storeFaultEvent(), "Level sensor: re-init failed (%s reset, down %ld s, ACK tof=%u fram=%u rtc=%u)",
             how, secondsSince(g_lastGoodMs, now), probe(0x29), probe(0x50), probe(0x68));
    if (isPumpActive()) return;
    if (g_recoveryDone && now - g_lastRecovery < LEVEL_BUS_RECOVERY_MS) return;
    g_recoveryDone = true;
    g_lastRecovery = now;
    recoverBus();
}

static void startSeries() {
    g_collecting  = true;
    g_skipFirst   = true;
    g_seriesStart = millis();
    g_n = g_rej = 0;
    g_sum = g_sumSq = 0;
    g_sumSignal = g_sumAmbient = 0;
    g_i2cErr = 0;
    memset(g_statusCnt, 0, sizeof(g_statusCnt));
    g_polls = g_pollFail = 0;
    g_pollErr = 0;
    g_lastHv  = 0xFF;
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
        g_recoveryDone = false;
        g_retries = 0;
        if (g_state == LEVEL_SENSOR_FAULT) {
            logEvent(EV_INFO, true, "Level sensor: recovered (down %ld s)", secondsSince(g_lastGoodMs, g_lastSeriesEnd));
        }
        g_lastGoodMs = g_lastSeriesEnd;
        g_state = LEVEL_SENSOR_OK;
        LOG_INFO("Level: %.1f mm (sd %.2f, sem %.2f) n=%u rej=%u signal=%u ambient=%u kcps",
                 s.distance_mm, s.stddev_mm, s.sem_mm, s.samples, s.rejected,
                 s.signal_kcps, s.ambient_kcps);
    } else {
        if (g_badSeries < 255) g_badSeries++;
        char st[64] = "";
        size_t len = 0;
        for (uint8_t i = 0; i < 16 && len < sizeof(st); i++) {
            if (g_statusCnt[i]) len += snprintf(st + len, sizeof(st) - len, " st%u=%u", i == 15 ? 255 : i, g_statusCnt[i]);
        }
        // The series that trips the FAULT is stored (rejection breakdown of the failure)
        bool tripsFault = g_badSeries >= LEVEL_FAULT_SERIES && g_state == LEVEL_SENSOR_OK;
        if (s.samples == 0 && s.rejected == 0) {
            // Data never ready: polling result + snapshot of the ranging state
            char snap[64];
            regSnapshot(snap, sizeof(snap));
            logEvent(EV_WARN, tripsFault,
                     "Level: no data, %u in a row (polls=%u fail=%u e%u hv=%02X; %s)",
                     g_badSeries, g_polls, g_pollFail, g_pollErr, g_lastHv, snap);
        } else {
            logEvent(EV_WARN, tripsFault,
                     "Level: bad series (n=%u rej=%u [i2c=%u%s], %u in a row; last rej %u mm sig=%u amb=%u sigma=%u spads=%u)",
                     s.samples, s.rejected, g_i2cErr, st, g_badSeries,
                     g_lastRej.range_mm, g_lastRej.signal_rate_kcps, g_lastRej.ambient_rate_kcps,
                     g_lastRej.sigma_mm, g_lastRej.number_of_spad);
        }
        if (tripsFault) {
            g_state = LEVEL_SENSOR_FAULT;
            g_faultEvents = 0;
            logEvent(EV_ERROR, true, "Level sensor: FAULT after %u bad series (last good %ld s ago, last pump stop %ld s ago, uptime %lu s)",
                     g_badSeries, secondsSince(g_lastGoodMs, g_lastSeriesEnd),
                     secondsSince(g_lastPumpStop, g_lastSeriesEnd), (unsigned long)(g_lastSeriesEnd / 1000));
        }
        // Sensor may have lost its config (brown-out, loose wire) or the bus may hang — retry periodically
        if (g_state == LEVEL_SENSOR_FAULT && g_lastSeriesEnd - g_lastInitTry >= LEVEL_INIT_RETRY_MS) {
            retryInit(g_lastSeriesEnd);
        }
    }
}

void initLevelSensor() {
    // ESP-only resets (midnight restart, OTA) do not power-cycle the sensor —
    // start it from a clean state
    pinMode(LEVEL_XSHUT_PIN, OUTPUT_OPEN_DRAIN);
    hardReset();
    if (tryInit()) {
        g_state = LEVEL_SENSOR_OK;
        char snap[64];
        regSnapshot(snap, sizeof(snap));
        logEvent(EV_INFO, true, "Boot: sensor initialized (%s)", snap);
        LOG_INFO("Level sensor: VL53L4CD initialized (budget %d ms, N=%d, every %lu s)",
                 LEVEL_TIMING_BUDGET_MS, LEVEL_SAMPLES_PER_SERIES, LEVEL_SERIES_INTERVAL_MS / 1000);
        // First series starts from loop() — setup() takes longer than the
        // series timeout, a series started here would close empty
    } else {
        g_state = LEVEL_SENSOR_ABSENT;
        addLevelEvent("Boot: sensor not detected");
        LOG_ERROR("Level sensor: VL53L4CD not detected (retry every %lu s)", LEVEL_INIT_RETRY_MS / 1000);
    }
}

void updateLevelSensor() {
    uint32_t now = millis();

    if (g_state == LEVEL_SENSOR_ABSENT) {
        if (now - g_lastInitTry >= LEVEL_INIT_RETRY_MS) {
            hardReset();
            if (tryInit()) {
                g_state = LEVEL_SENSOR_OK;
                g_badSeries = 0;
                logEvent(EV_INFO, true, "Level sensor: VL53L4CD detected");
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
        g_lastPumpStop  = now;
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

    // Same test as g_tof.dataReady(), but a failed read is counted, not taken as "not ready"
    uint8_t hv, err;
    if (g_polls < 0xFFFF) g_polls++;
    if (!readRegChecked(VL53L4CD::GPIO__TIO_HV_STATUS, hv, err)) {
        if (g_pollFail < 0xFFFF) g_pollFail++;
        g_pollErr = err;
        return;
    }
    g_lastHv = hv;
    if (hv & 0x01) return;
    g_tof.read(false);
    if (g_tof.last_status != 0) {
        g_rej++;
        if (g_i2cErr < 255) g_i2cErr++;
        return;
    }

    if (g_skipFirst) {
        g_skipFirst = false;
        return;
    }

    const VL53L4CD::RangingData& r = g_tof.ranging_data;
    if (r.range_status != 0) {
        g_rej++;
        uint8_t idx = r.range_status < 15 ? r.range_status : 15;
        if (g_statusCnt[idx] < 255) g_statusCnt[idx]++;
        g_lastRej = r;
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

bool isLevelSettling() { return g_quiet || isPumpActive(); }

LevelSensorState   getLevelSensorState() { return g_state; }
const LevelSeries& getLastLevelSeries()  { return g_last; }

const char* getLevelSensorStateString() {
    switch (g_state) {
        case LEVEL_SENSOR_OK:    return "ok";
        case LEVEL_SENSOR_FAULT: return "fault";
        default:                 return "absent";
    }
}
