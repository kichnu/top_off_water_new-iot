#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../hardware/rtc_controller.h"
#include "../hardware/fram_controller.h"
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

    config.dose_ml           = DEFAULT_DOSE_ML;
    config.daily_limit_ml    = DEFAULT_DAILY_LIMIT_ML;
    config.ema_alpha         = DEFAULT_EMA_ALPHA;
    config.ema_clamp         = DEFAULT_EMA_CLAMP;
    config.alarm_p1          = DEFAULT_ALARM_P1;
    config.alarm_p2          = DEFAULT_ALARM_P2;
    config.zone_green        = DEFAULT_ZONE_GREEN;
    config.zone_yellow       = DEFAULT_ZONE_YELLOW;
    config.initial_ema       = DEFAULT_INITIAL_EMA;
    config.history_window_s  = DEFAULT_HISTORY_WINDOW_S;
    config.is_configured     = 0x00;

    ema.ema_rate_ml_h  = 0.0f;
    ema.ema_interval_s = 0.0f;
    ema.alarm_score    = 0.0f;
    ema.bootstrap_count = 0;

    alarmArmed = true;
    lastError  = ERROR_NONE;

    lowReservoirCount = 0;

    systemWasDisabled = false;
    cycleLogged       = false;

    pinMode(RESET_PIN, INPUT_PULLUP);
    pinMode(AVAILABLE_WATER_SENSOR_PIN, INPUT_PULLUP);

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

    LOG_INFO("====================================");
    LOG_INFO("WaterAlgorithm: initFromFRAM done");
    LOG_INFO("  dose_ml        : %d ml", config.dose_ml);
    LOG_INFO("  daily_limit_ml : %d ml", config.daily_limit_ml);
    LOG_INFO("  ema_alpha      : %.2f", config.ema_alpha);
    LOG_INFO("  ema_clamp      : %.2f", config.ema_clamp);
    LOG_INFO("  alarm_p1       : %.3f", config.alarm_p1);
    LOG_INFO("  alarm_p2       : %.3f", config.alarm_p2);
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
    checkAvailableWaterSensor();
    if (currentState == STATE_ERROR) return;
    startAutoPump();
}

// ============================================================
// CZUJNIK DOSTĘPNOŚCI WODY
// ============================================================

void WaterAlgorithm::checkAvailableWaterSensor() {
    bool sensorLow = (digitalRead(AVAILABLE_WATER_SENSOR_PIN) == LOW);

    if (sensorLow) {
        if (lowReservoirCount < 255) lowReservoirCount++;

        if (lowReservoirCount >= LOW_RESERVOIR_CRITICAL_COUNT) {
            LOG_WARNING("LOW RESERVOIR CRITICAL: count=%d → STATE_ERROR", lowReservoirCount);
            startErrorSignal(ERROR_LOW_RESERVOIR);
        } else {
            LOG_WARNING("LOW RESERVOIR WARNING: count=%d/%d — system continues",
                        lowReservoirCount, LOW_RESERVOIR_CRITICAL_COUNT - 1);
        }
    } else {
        if (lowReservoirCount > 0) {
            LOG_INFO("Available water sensor OK — alarm counter reset (%d → 0)", lowReservoirCount);
        }
        lowReservoirCount = 0;
    }
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

    rolling24hVolumeMl = scanRolling24h();
    if (rolling24hVolumeMl >= config.daily_limit_ml) {
        LOG_WARNING("DAILY LIMIT PRE-CHECK: %d >= %d ml — dose skipped → ERROR",
                    rolling24hVolumeMl, config.daily_limit_ml);
        startErrorSignal(ERROR_DAILY_LIMIT);
        return;
    }

    uint16_t doseTimeS = calculateDoseTime(config.dose_ml, currentPumpSettings.volumePerSecond);

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("AUTO PUMP START");
    LOG_INFO("  dose_ml      : %d ml", config.dose_ml);
    LOG_INFO("  flow_rate    : %.2f ml/s", currentPumpSettings.volumePerSecond);
    LOG_INFO("  dose_time    : %d s", doseTimeS);
    LOG_INFO("  rolling_24h  : %d / %d ml", rolling24hVolumeMl, config.daily_limit_ml);
    LOG_INFO("  ema_rate     : %.1f ml/h", ema.ema_rate_ml_h);
    LOG_INFO("  alarm_score  : %.2f (green<%.1f yellow<%.1f)",
             ema.alarm_score, config.zone_green, config.zone_yellow);
    LOG_INFO("====================================");

    pumpStartMs = millis();
    triggerPump(doseTimeS, "AUTO_PUMP");

    currentState = STATE_PUMPING;
    stateStartMs = millis();
}

// ============================================================
// FINALIZACJA CYKLU
// ============================================================

void WaterAlgorithm::finishPumpCycle() {
    uint32_t nowTs = getUnixTimestamp();
    time_t   t     = (time_t)nowTs;
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    uint8_t hourOfDay = (uint8_t)tmLocal.tm_hour;

    uint32_t intervalS = (lastTopOffTimestamp > 0 && nowTs > lastTopOffTimestamp)
                         ? (nowTs - lastTopOffTimestamp) : 0;
    lastTopOffTimestamp = nowTs;

    float rateMlH = (intervalS > 0)
                    ? ((float)config.dose_ml / intervalS * 3600.0f)
                    : 0.0f;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("CYCLE COMPLETE");
    LOG_INFO("  dose_ml   : %d ml", config.dose_ml);
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

    // Re-arm: jeśli score wróciło do żółtej lub zielonej strefy
    if (!alarmArmed && ema.alarm_score < config.zone_yellow) {
        alarmArmed = true;
        LOG_INFO("ALARM re-armed (score=%.2f < zone_yellow=%.2f)", ema.alarm_score, config.zone_yellow);
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

    rolling24hVolumeMl = scanRolling24h();

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
    float alpha = config.ema_alpha;
    float clamp = config.ema_clamp;
    float p1    = config.alarm_p1;
    float p2    = config.alarm_p2;

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

    LOG_INFO("Config updated: dose=%d ml, daily_limit=%d ml, alpha=%.2f, clamp=%.2f",
             config.dose_ml, config.daily_limit_ml, config.ema_alpha, config.ema_clamp);
    LOG_INFO("  p1=%.3f p2=%.3f zone_green=%.1f zone_yellow=%.1f initial_ema=%.2f",
             config.alarm_p1, config.alarm_p2, config.zone_green, config.zone_yellow,
             config.initial_ema);
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
    LOG_WARNING("Error: code=%d → STATE_ERROR", (int)error);
}

// ============================================================
// RESET BUTTON
// ============================================================

void WaterAlgorithm::checkResetButton() {
    static uint32_t pressStart = 0;
    static bool     wasPressed = false;

    bool pressed = (digitalRead(RESET_PIN) == LOW);

    if (pressed && !wasPressed) {
        pressStart = millis();
        wasPressed = true;
    } else if (!pressed && wasPressed) {
        wasPressed = false;
        if (currentState == STATE_ERROR) {
            resetFromError();
        }
    }
}

// ============================================================
// RESET / RECOVERY
// ============================================================

void WaterAlgorithm::resetFromError() {
    lastError    = ERROR_NONE;
    currentState = STATE_IDLE;
    stateStartMs = millis();
    alarmArmed   = false;  // wymagany re-arm przez żółtą/zieloną strefę
    resetSensorProcess();
    LOG_INFO("Error cleared — returned to IDLE, alarmArmed=false");
}

bool WaterAlgorithm::resetSystem() {
    if (currentState == STATE_LOGGING) return false;

    if (isPumpActive()) stopPump();

    lastError    = ERROR_NONE;
    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
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
    totalCycleCount     = 0;

    framBusy = true;
    bool ok = clearTopOffRingBuffer();
    if (ok) saveEmaToFRAM();  // persist reset EMA — without this, restart restores old EMA
    framBusy = false;

    LOG_INFO("WaterAlgorithm: cycle history and EMA reset");
    return ok;
}

void WaterAlgorithm::resetRolling24h() {
    manualResetTs      = getUnixTimestamp();
    rolling24hVolumeMl = scanRolling24h();
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
            uint32_t elapsed = (millis() - pumpStartMs) / 1000UL;
            uint16_t total   = calculateDoseTime(config.dose_ml, currentPumpSettings.volumePerSecond);
            return String("Pumping ") + config.dose_ml + "ml (" + elapsed + "/" + total + "s)";
        }
        case STATE_LOGGING:
            return "Logging cycle data";
        case STATE_ERROR:
            if (lastError == ERROR_LOW_RESERVOIR) return "Reservoir empty — refill and reset";
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
