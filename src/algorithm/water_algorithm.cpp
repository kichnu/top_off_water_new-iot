#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../hardware/rtc_controller.h"
#include "../hardware/fram_controller.h"
#include "../hardware/reserve_controller.h"
#include "../hardware/pump_calibration.h"
#include "../hardware/buzzer_controller.h"
#include "../config/config.h"
#include "algorithm_config.h"
#include <math.h>
#include <time.h>

WaterAlgorithm waterAlgorithm;

// ============================================================
// Konstruktor
// ============================================================
WaterAlgorithm::WaterAlgorithm() {
    currentState     = STATE_IDLE;
    stateStartMs     = 0;
    pumpStartMs      = 0;

    lastTopOffTimestamp = 0;
    totalCycleCount     = 0;
    rolling24hVolumeMl  = 0;
    manualResetTs       = 0;
    manualResetMs       = 0;
    ramDoseCount        = 0;
    ramDoseNext         = 0;

    config.dose_ml           = DEFAULT_DOSE_ML;
    config.daily_limit_ml    = DEFAULT_DAILY_LIMIT_ML;
    memset(config._reserved, 0, sizeof(config._reserved));
    config.zone_green        = DEFAULT_ZONE_GREEN;
    config.zone_yellow       = DEFAULT_ZONE_YELLOW;
    config.initial_ema       = DEFAULT_INITIAL_EMA;
    config.history_window_s  = DEFAULT_HISTORY_WINDOW_S;
    config.is_configured     = 0x00;

    ema.ema_rate_ml_h  = 0.0f;
    ema.ema_interval_s = 0.0f;
    ema.alarm_score    = 0.0f;
    ema.bootstrap_count = 0;

    alarmArmed    = true;
    disarmUntilTs = 0;
    lastError     = ERROR_NONE;

    systemWasDisabled = false;
    cycleLogged       = false;

    pinMode(RESET_PIN, INPUT_PULLUP);

    LOG_INFO("WaterAlgorithm: constructor done");
}

// ============================================================
// initFromFRAM — wywołaj z setup() po initFRAM()
// ============================================================
void WaterAlgorithm::initFromFRAM() {
    TopOffConfig framCfg;
    bool configValid = loadTopOffConfigFromFRAM(framCfg) &&
                       framCfg.is_configured == TOPOFF_CONFIG_MAGIC;

    if (!isTopOffRingBufferValid()) {
        LOG_WARNING("WaterAlgorithm: ring buffer count out of range — clearing FRAM data");
        framBusy = true;
        clearTopOffRingBuffer();
        framBusy = false;
    }

    if (configValid) {
        config = framCfg;
        LOG_INFO("WaterAlgorithm: config loaded from FRAM");

        loadEmaFromFRAM();

        // Seed EMA z initial_ema jeśli brak danych historycznych
        if (ema.ema_rate_ml_h < 0.01f && config.initial_ema > 0.01f) {
            ema.ema_rate_ml_h = config.initial_ema;
            LOG_INFO("WaterAlgorithm: EMA seeded from initial_ema=%.2f", config.initial_ema);
        }

        TopOffRecord lastRecord;
        if (loadLastTopOffRecord(lastRecord)) {
            lastTopOffTimestamp = lastRecord.timestamp;
            totalCycleCount     = lastRecord.cycle_num + 1;
            LOG_INFO("WaterAlgorithm: last record restored: ts=%lu cycle=%d",
                     lastTopOffTimestamp, lastRecord.cycle_num);
        }

        rolling24hVolumeMl = scanRolling24h();

    } else {
        LOG_WARNING("WaterAlgorithm: FRAM config outdated or absent — hard reset");
        LOG_WARNING("  Clearing ring buffer and EMA (FRAM format change)");
        framBusy = true;
        clearTopOffRingBuffer();
        framBusy = false;

        // Seed EMA z default initial_ema
        if (config.initial_ema > 0.01f) {
            ema.ema_rate_ml_h = config.initial_ema;
        }
    }

    loadErrorState();

    LOG_INFO("====================================");
    LOG_INFO("WaterAlgorithm: initFromFRAM done");
    LOG_INFO("  dose_ml        : %d ml", config.dose_ml);
    LOG_INFO("  daily_limit_ml : %d ml", config.daily_limit_ml);
    LOG_INFO("  zone_green     : %.1f", config.zone_green);
    LOG_INFO("  zone_yellow    : %.1f", config.zone_yellow);
    LOG_INFO("  initial_ema    : %.2f", config.initial_ema);
    LOG_INFO("  bootstrap_count: %d", ema.bootstrap_count);
    LOG_INFO("  ema_rate       : %.2f ml/h", ema.ema_rate_ml_h);
    LOG_INFO("  alarm_score    : %.2f", ema.alarm_score);
    LOG_INFO("  rolling_24h    : %d ml", rolling24hVolumeMl);
    LOG_INFO("  next_cycle_num : %d", totalCycleCount);
    LOG_INFO("====================================");
}

// ============================================================
// FRAM — ładowanie i zapis
// ============================================================
void WaterAlgorithm::loadConfigFromFRAM() {
    // Logika przeniesiona do initFromFRAM()
}

void WaterAlgorithm::loadEmaFromFRAM() {
    EmaBlock framEma;
    if (loadEmaBlockFromFRAM(framEma)) {
        ema = framEma;
        LOG_INFO("WaterAlgorithm: EMA loaded — bootstrap=%d rate=%.2f score=%.2f",
                 ema.bootstrap_count, ema.ema_rate_ml_h, ema.alarm_score);
    } else {
        LOG_INFO("WaterAlgorithm: EMA absent in FRAM — starting from zero");
    }
}

void WaterAlgorithm::saveEmaToFRAM() {
    framBusy = true;
    saveEmaBlockToFRAM(ema);
    framBusy = false;
}

// ============================================================
// GŁÓWNA PĘTLA
// ============================================================
void WaterAlgorithm::update() {
    checkResetButton();
    checkPumpAutoEnable();
    checkServiceAutoEnable();

    if (isSystemDisabled()) {
        handleSystemDisable();
        return;
    }

    if (systemWasDisabled) {
        LOG_INFO("SYSTEM RE-ENABLED — sensor state will be picked up by water_sensors");
        systemWasDisabled = false;
        resetSensorProcess();
    }

    switch (currentState) {

        case STATE_IDLE:
            break;

        case STATE_DEBOUNCING:
            break;

        case STATE_PUMPING:
            if (!isPumpActive()) {
                finishPumpCycle();
            }
            break;

        case STATE_LOGGING:
            if (millis() - stateStartMs >= (uint32_t)(LOGGING_TIME * 1000)) {
                LOG_INFO("LOGGING complete — returning to IDLE");
                currentState = STATE_IDLE;
                stateStartMs = millis();
                cycleLogged  = false;
            }
            break;

        case STATE_ERROR:
            break;

    }
}

// ============================================================
// CALLBACKI Z water_sensors
// ============================================================

void WaterAlgorithm::onDebounceStart() {
    if (currentState != STATE_IDLE) return;
    currentState = STATE_DEBOUNCING;
    stateStartMs = millis();
    LOG_INFO("ALGORITHM: IDLE → DEBOUNCING");
}

void WaterAlgorithm::onDebounceComplete() {
    if (currentState != STATE_DEBOUNCING) return;
    resetSensorProcess();
    startAutoPump();
}

void WaterAlgorithm::onDebounceReset() {
    if (currentState == STATE_DEBOUNCING) {
        LOG_INFO("ALGORITHM: Debounce reset — back to IDLE");
        currentState = STATE_IDLE;
        stateStartMs = millis();
    }
}

// ============================================================
// PUMP CONTROL
// ============================================================

void WaterAlgorithm::startAutoPump() {
    if (isDirectPumpMode()) {
        LOG_INFO("Auto pump deferred — calibration/direct mode active, returning to IDLE");
        currentState = STATE_IDLE;
        stateStartMs = millis();
        resetSensorProcess();
        return;
    }

    if (!reserveCheckDose()) {
        LOG_WARNING("Reserve: %s — auto pump blocked → STATE_ERROR", getReserveAlarmString());
        startErrorSignal(ERROR_LOW_RESERVOIR);
        return;
    }

    // Lookahead: the dose must fit in the limit, not only start below it
    rolling24hVolumeMl = rolling24h();
    if ((uint32_t)rolling24hVolumeMl + config.dose_ml > config.daily_limit_ml) {
        LOG_WARNING("DAILY LIMIT PRE-CHECK: %d + %d > %d ml — dose skipped → ERROR",
                    rolling24hVolumeMl, config.dose_ml, config.daily_limit_ml);
        startErrorSignal(ERROR_DAILY_LIMIT);
        return;
    }

    uint32_t doseTimeMs = getPortionMs();

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("AUTO PUMP START");
    LOG_INFO("  dose_ml      : %d ml", config.dose_ml);
    LOG_INFO("  flow_rate    : %.2f ml/s", currentPumpSettings.volumePerSecond);
    LOG_INFO("  dose_time    : %lu ms (%s)", doseTimeMs, isPumpCalibrated() ? "calibrated" : "NOT calibrated, default");
    LOG_INFO("  rolling_24h  : %d / %d ml", rolling24hVolumeMl, config.daily_limit_ml);
    LOG_INFO("  ema_rate     : %.1f ml/h", ema.ema_rate_ml_h);
    LOG_INFO("  alarm_score  : %.2f (green<%.1f yellow<%.1f)",
             ema.alarm_score, config.zone_green, config.zone_yellow);
    LOG_INFO("====================================");

    pumpStartMs = millis();
    triggerPumpMs(doseTimeMs, "AUTO_PUMP");

    currentState = STATE_PUMPING;
    stateStartMs = millis();
}

// ============================================================
// FINALIZACJA CYKLU
// ============================================================

void WaterAlgorithm::finishPumpCycle() {
    uint32_t nowTs = getUnixTimestamp();
    bool timeValid = isUnixTimeValid();
    time_t   t     = (time_t)nowTs;
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    uint8_t hourOfDay = (uint8_t)tmLocal.tm_hour;

    // The dose counts for the 24 h limit regardless of the clock
    ramDoses[ramDoseNext] = { millis(), config.dose_ml };
    ramDoseNext = (ramDoseNext + 1) % TOPOFF_HISTORY_SIZE;
    if (ramDoseCount < TOPOFF_HISTORY_SIZE) ramDoseCount++;

    if (!timeValid) {
        // No interval, EMA or history record from a fake time; the next interval
        // starts over (a huge interval after the clock returns would drop the
        // score and mask an anomaly)
        lastTopOffTimestamp = 0;
        LOG_WARNING("CYCLE COMPLETE without a valid RTC time — EMA, interval and history skipped");
        rolling24hVolumeMl = rolling24h();
        if (rolling24hVolumeMl >= config.daily_limit_ml) {
            LOG_WARNING("DAILY LIMIT: %d ml >= %d ml — entering ERROR state",
                        rolling24hVolumeMl, config.daily_limit_ml);
            startErrorSignal(ERROR_DAILY_LIMIT);
            return;
        }
        currentState = STATE_LOGGING;
        stateStartMs = millis();
        cycleLogged  = true;
        return;
    }

    uint32_t intervalS = (lastTopOffTimestamp > 0 && nowTs > lastTopOffTimestamp)
                         ? (nowTs - lastTopOffTimestamp) : 0;
    lastTopOffTimestamp = nowTs;

    // The interval is the time the PREVIOUS dose took to evaporate, so the rate uses
    // that cycle's dose — otherwise the first cycle after a dose change is skewed
    uint16_t rateDoseMl = config.dose_ml;
    if (intervalS > 0) {
        TopOffRecord prev;
        framBusy = true;
        bool havePrev = loadLastTopOffRecord(prev);
        framBusy = false;
        if (havePrev && prev.dose_ml_record >= 50 && prev.dose_ml_record <= 5000) {
            rateDoseMl = prev.dose_ml_record;
        }
    }

    float rateMlH = (intervalS > 0)
                    ? ((float)rateDoseMl / intervalS * 3600.0f)
                    : 0.0f;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("CYCLE COMPLETE");
    LOG_INFO("  dose_ml   : %d ml (rate from the previous dose %d ml)", config.dose_ml, rateDoseMl);
    LOG_INFO("  interval  : %lu s", intervalS);
    LOG_INFO("  rate      : %.2f ml/h", rateMlH);
    LOG_INFO("  hour      : %d", hourOfDay);
    LOG_INFO("====================================");

    if (intervalS > 0) {
        updateEMA(intervalS, rateMlH);
    } else {
        ema.ema_interval_s = 0.0f;
        if (ema.bootstrap_count < 255) ema.bootstrap_count++;
    }

    // Re-arm: the score is back below the red zone, or the disarm time is over
    if (!alarmArmed && (ema.alarm_score < config.zone_yellow || nowTs >= disarmUntilTs)) {
        alarmArmed    = true;
        disarmUntilTs = 0;
        saveErrorState();
        LOG_INFO("ALARM re-armed (score=%.2f, zone_yellow=%.2f)", ema.alarm_score, config.zone_yellow);
    }

    uint8_t alertLevel = calculateAlertLevel();
    bool redAlertHit   = (alertLevel == 2) && alarmArmed;

    TopOffRecord record;
    record.timestamp      = nowTs;
    record.interval_s     = intervalS;
    record.rate_ml_h      = rateMlH;
    record.cycle_num      = totalCycleCount++;
    record.dev_rate_sigma = 0;  // nieużywane
    record.alert_level    = alertLevel;
    record.hour_of_day    = hourOfDay;
    record.flags          = 0;
    record.dose_ml_record = config.dose_ml;
    if (redAlertHit) record.flags |= TopOffRecord::FLAG_RED_ALERT;

    framBusy = true;
    saveTopOffRecord(record);
    saveEmaToFRAM();
    framBusy = false;

    rolling24hVolumeMl = rolling24h();

    bool dailyLimitHit = (rolling24hVolumeMl >= config.daily_limit_ml);

    LOG_INFO("METRICS: score=%.2f alert=%d armed=%d rolling=%d/%d ml",
             ema.alarm_score, alertLevel, (int)alarmArmed,
             rolling24hVolumeMl, config.daily_limit_ml);

    if (dailyLimitHit) {
        LOG_WARNING("DAILY LIMIT: %d ml >= %d ml — entering ERROR state",
                    rolling24hVolumeMl, config.daily_limit_ml);
        startErrorSignal(ERROR_DAILY_LIMIT);
        return;
    }

    if (redAlertHit) {
        LOG_WARNING("RED ALERT: alarm_score=%.2f >= zone_yellow=%.2f — entering ERROR state",
                    ema.alarm_score, config.zone_yellow);
        startErrorSignal(ERROR_RED_ALERT);
        return;
    }

    currentState = STATE_LOGGING;
    stateStartMs = millis();
    cycleLogged  = true;
}

// ============================================================
// EMA + ALARM SCORE
// ============================================================

void WaterAlgorithm::updateEMA(uint32_t intervalS, float rateMlH) {
    const float alpha = EMA_ALPHA;
    const float clamp = EMA_CLAMP;
    const float p1    = ALARM_P1;
    const float p2    = ALARM_P2;

    // Seed EMA przy pierwszej dolewce z rate
    if (ema.ema_rate_ml_h < 0.01f) {
        ema.ema_rate_ml_h  = rateMlH;
        ema.ema_interval_s = (float)intervalS;
        if (ema.bootstrap_count < 255) ema.bootstrap_count++;
        LOG_INFO("EMA: seeded from first rate=%.2f", rateMlH);
        return;
    }

    // Odchylenie względne
    float d = (ema.ema_rate_ml_h > 0.01f)
              ? ((rateMlH - ema.ema_rate_ml_h) / ema.ema_rate_ml_h)
              : 0.0f;

    // Alarm score
    if (d > 0.0f) {
        float instant = p1 * 100.0f * log2f(1.0f + d);
        float decay   = p2 / (1.0f + d);
        ema.alarm_score = ema.alarm_score * (1.0f - decay) + instant;
    } else {
        float factor = fminf(1.0f, p2 * (1.0f + fabsf(d)));
        ema.alarm_score = fmaxf(0.0f, ema.alarm_score * (1.0f - factor));
    }

    // Clamp rate przed aktualizacją EMA
    float lo      = ema.ema_rate_ml_h * (1.0f - clamp);
    float hi      = ema.ema_rate_ml_h * (1.0f + clamp);
    float clamped = fminf(fmaxf(rateMlH, lo), hi);

    ema.ema_rate_ml_h  = alpha * clamped    + (1.0f - alpha) * ema.ema_rate_ml_h;
    ema.ema_interval_s = alpha * intervalS  + (1.0f - alpha) * ema.ema_interval_s;

    if (ema.bootstrap_count < 255) ema.bootstrap_count++;

    LOG_INFO("EMA: rate=%.2f ml/h (clamped=%.2f) score=%.2f d=%.3f bootstrap=%d",
             ema.ema_rate_ml_h, clamped, ema.alarm_score, d, ema.bootstrap_count);
}

uint8_t WaterAlgorithm::calculateAlertLevel() const {
    if (ema.alarm_score >= config.zone_yellow) return 2;
    if (ema.alarm_score >= config.zone_green)  return 1;
    return 0;
}

// ============================================================
// Rolling 24h
// ============================================================

uint16_t WaterAlgorithm::ramRolling24h() const {
    uint32_t now     = millis();
    uint32_t windowMs = config.history_window_s * 1000UL;
    uint32_t total   = 0;
    for (uint8_t i = 0; i < ramDoseCount; i++) {
        const RamDose& d = ramDoses[i];
        if (now - d.ms >= windowMs) continue;
        if (manualResetMs && (int32_t)(d.ms - manualResetMs) < 0) continue;
        total += d.ml;
    }
    return total > 65000u ? 65000u : (uint16_t)total;
}

uint16_t WaterAlgorithm::rolling24h() const {
    uint16_t fram = scanRolling24h();
    uint16_t ram  = ramRolling24h();
    if (ram > fram) LOG_WARNING("Rolling 24h: RAM count %u ml > FRAM history %u ml — using RAM", ram, fram);
    return ram > fram ? ram : fram;
}

uint16_t WaterAlgorithm::scanRolling24h() const {
    uint32_t nowTs    = getUnixTimestamp();
    uint32_t windowS  = config.history_window_s;
    uint32_t cutoffTs = (nowTs > windowS) ? (nowTs - windowS) : 0;
    if (manualResetTs > cutoffTs) cutoffTs = manualResetTs;

    uint32_t total = 0;   // uint32_t — chroni przed przepełnieniem uint16_t przy błędnych rekordach
    TopOffRecord records[TOPOFF_HISTORY_SIZE];
    uint16_t count = loadTopOffHistory(records, TOPOFF_HISTORY_SIZE);

    uint16_t inWindow = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (records[i].timestamp >= cutoffTs) {
            // Walidacja dose_ml_record: musi być w sensownym zakresie.
            // Stare rekordy (sprzed 02cefe4) miały tu _pad[2] — losowe bajty stosu.
            uint16_t d;
            if (records[i].dose_ml_record >= 50 && records[i].dose_ml_record <= 5000) {
                d = records[i].dose_ml_record;
            } else {
                d = config.dose_ml;   // fallback: 0 lub śmieć → użyj aktualnej konfiguracji
            }
            total += d;
            inWindow++;
        }
    }

    // Ogranicz do uint16_t — w razie masowej korupcji FRAM nie zwróci absurdalnej liczby
    uint16_t result = (total > 65000u) ? 65000u : (uint16_t)total;

    LOG_INFO("scanRolling24h: nowTs=%lu cutoff=%lu total=%d records, %d in window → %u ml",
             nowTs, cutoffTs, count, inWindow, result);

    return result;
}

// ============================================================
// Konfiguracja
// ============================================================

void WaterAlgorithm::setConfig(const TopOffConfig& cfg) {
    config = cfg;
    config.is_configured = TOPOFF_CONFIG_MAGIC;

    framBusy = true;
    saveTopOffConfigToFRAM(config);
    framBusy = false;

    LOG_INFO("Config updated: dose=%d ml, daily_limit=%d ml",
             config.dose_ml, config.daily_limit_ml);
    LOG_INFO("  zone_green=%.1f zone_yellow=%.1f initial_ema=%.2f",
             config.zone_green, config.zone_yellow, config.initial_ema);
}

// ============================================================
// SYSTEM DISABLE
// ============================================================

void WaterAlgorithm::handleSystemDisable() {
    if (currentState == STATE_IDLE) {
        if (!systemWasDisabled) {
            LOG_INFO("System disabled while IDLE");
            systemWasDisabled = true;
        }
        return;
    }

    if (currentState == STATE_ERROR || currentState == STATE_LOGGING) {
        if (!systemWasDisabled) systemWasDisabled = true;
        return;
    }

    LOG_WARNING("SYSTEM DISABLE — interrupting cycle (state: %s)", getStateString());

    if (isPumpActive() && !isDirectPumpMode()) {
        stopPump();
        LOG_WARNING("Pump stopped by system disable");
    }

    resetSensorProcess();
    currentState      = STATE_IDLE;
    stateStartMs      = millis();
    systemWasDisabled = true;
}

// ============================================================
// ERROR SIGNAL
// ============================================================

void WaterAlgorithm::startErrorSignal(ErrorCode error) {
    lastError    = error;
    currentState = STATE_ERROR;
    stateStartMs = millis();
    saveErrorState();
    LOG_WARNING("Error: code=%d → STATE_ERROR", (int)error);
}

// Error latch + disarm time in FRAM — a restart (midnight, power loss) keeps them
void WaterAlgorithm::saveErrorState() {
    AlgErrorState st = {};
    st.last_error      = (uint8_t)lastError;
    st.magic           = ALG_ERROR_STATE_MAGIC;
    st.disarm_until_ts = alarmArmed ? 0 : disarmUntilTs;
    framBusy = true;
    saveAlgErrorStateToFRAM(st);
    framBusy = false;
}

void WaterAlgorithm::loadErrorState() {
    AlgErrorState st;
    framBusy = true;
    bool ok = loadAlgErrorStateFromFRAM(st);
    framBusy = false;
    if (!ok) return;
    disarmUntilTs = st.disarm_until_ts;
    alarmArmed    = (disarmUntilTs == 0);
    if (st.last_error != ERROR_NONE) {
        lastError    = (ErrorCode)st.last_error;
        currentState = STATE_ERROR;
        stateStartMs = millis();
        LOG_WARNING("WaterAlgorithm: error %d restored from FRAM → STATE_ERROR (reset required)", st.last_error);
    }
    if (!alarmArmed) {
        LOG_INFO("WaterAlgorithm: anomaly alarm disarmed until ts=%lu", (unsigned long)disarmUntilTs);
    }
}

// ============================================================
// RESET BUTTON
// ============================================================

// Held RESET_HOLD_MS without a break (that is also the debounce) — one reset
// per press, confirmed by two short beeps
void WaterAlgorithm::checkResetButton() {
    static uint32_t pressStart = 0;
    static bool     done       = false;

    if (digitalRead(RESET_PIN) != LOW) {
        pressStart = 0;
        done       = false;
        return;
    }
    if (pressStart == 0) pressStart = millis() | 1u;   // |1 — 0 means released
    if (done || millis() - pressStart < RESET_HOLD_MS) return;
    done = true;

    if (currentState != STATE_ERROR && !isReserveEmpty()) {
        LOG_INFO("Reset button: nothing to reset");
        return;
    }
    LOG_INFO("Reset button held %lu ms — reset", (unsigned long)RESET_HOLD_MS);
    if (resetSystem()) buzzerConfirm();
}

// ============================================================
// RESET / RECOVERY
// ============================================================

bool WaterAlgorithm::resetSystem() {
    if (currentState == STATE_LOGGING) return false;

    if (isPumpActive()) stopPump();

    clearReserveLatch();   // reserve alarm cleared only with water back, else it stays
    // Resetting an error with the score in the red zone: disarm the anomaly alarm
    // for a while, otherwise the next cycle raises it again at once
    if (currentState == STATE_ERROR && ema.alarm_score >= config.zone_yellow) {
        alarmArmed    = false;
        disarmUntilTs = (uint32_t)getUnixTimestamp() + ALARM_DISARM_S;
        LOG_INFO("Anomaly alarm disarmed for %lu h or until score < %.1f",
                 (unsigned long)(ALARM_DISARM_S / 3600), config.zone_yellow);
    }
    lastError    = ERROR_NONE;
    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
    saveErrorState();
    LOG_INFO("System reset to IDLE");
    return true;
}

bool WaterAlgorithm::resetCycleHistory() {
    if (isInCycle()) return false;

    ema = EmaBlock{};
    if (config.initial_ema > 0.01f) {
        ema.ema_rate_ml_h = config.initial_ema;
        LOG_INFO("WaterAlgorithm: EMA re-seeded to initial_ema=%.2f after history clear",
                 config.initial_ema);
    }
    lastTopOffTimestamp = 0;
    rolling24hVolumeMl  = 0;
    alarmArmed          = true;
    disarmUntilTs       = 0;
    totalCycleCount     = 0;

    framBusy = true;
    bool ok = clearTopOffRingBuffer();
    if (ok) saveEmaToFRAM();  // persist reset EMA — without this, restart restores old EMA
    framBusy = false;
    saveErrorState();         // armed again

    LOG_INFO("WaterAlgorithm: cycle history and EMA reset");
    return ok;
}

void WaterAlgorithm::resetRolling24h() {
    manualResetTs      = getUnixTimestamp();
    manualResetMs      = millis() | 1u;   // |1 — 0 means none
    rolling24hVolumeMl = rolling24h();
    LOG_INFO("Rolling 24h manually reset — manualResetTs=%lu rolling=%d ml",
             manualResetTs, rolling24hVolumeMl);
}

// ============================================================
// STATUS GETTERS
// ============================================================

const char* WaterAlgorithm::getStateString() const {
    switch (currentState) {
        case STATE_IDLE:            return "IDLE";
        case STATE_DEBOUNCING:      return "DEBOUNCING";
        case STATE_PUMPING:         return "PUMPING";
        case STATE_LOGGING:         return "LOGGING";
        case STATE_ERROR:           return "ERROR";
        default:                    return "UNKNOWN";
    }
}

String WaterAlgorithm::getStateDescription() const {
    switch (currentState) {
        case STATE_IDLE:
            return "Waiting for sensor";
        case STATE_DEBOUNCING:
            return String("Debouncing sensor (") +
                   getDebounceCounter() + "/" + DEBOUNCE_COUNTER + ")";
        case STATE_PUMPING: {
            float elapsed = (millis() - pumpStartMs) / 1000.0f;
            float total   = getPortionMs() / 1000.0f;
            return String("Pumping ") + config.dose_ml + "ml (" + String(elapsed, 1) + "/" + String(total, 1) + "s)";
        }
        case STATE_LOGGING:
            return "Logging cycle data";
        case STATE_ERROR:
            if (lastError == ERROR_LOW_RESERVOIR) return "Reservoir alarm — refill and reset";
            if (lastError == ERROR_DAILY_LIMIT)   return "Daily limit reached — reset required";
            if (lastError == ERROR_RED_ALERT)     return "Rate anomaly — reset required";
            return String("Error: code ") + (int)lastError + " — reset required";
        default:
            return "Unknown";
    }
}

bool WaterAlgorithm::isInCycle() const {
    return currentState != STATE_IDLE && currentState != STATE_ERROR;
}

uint32_t WaterAlgorithm::getRemainingSeconds() const {
    switch (currentState) {
        case STATE_PUMPING:
            return getPumpRemainingTime();
        case STATE_LOGGING: {
            uint32_t elapsed = millis() - stateStartMs;
            uint32_t total   = (uint32_t)(LOGGING_TIME * 1000);
            if (elapsed >= total) return 0;
            return (total - elapsed) / 1000UL;
        }
        default:
            return 0;
    }
}

uint32_t WaterAlgorithm::getPumpElapsedMs() const {
    if (currentState != STATE_PUMPING) return 0;
    return millis() - pumpStartMs;
}
