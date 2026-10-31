#include "water_sensors.h"
#include "hardware_pins.h"
#include "../core/logging.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"
#include "../config/config.h"
#include "driver/gpio.h"

// ============================================================
// Internal state
// ============================================================
static SensorPhase currentPhase  = PHASE_IDLE;
static uint32_t    phaseStartMs  = 0;   // millis() at the start of the current phase
static uint32_t    lastCheckMs   = 0;   // millis() of the last sample
static uint8_t     debounceCount = 0;   // consecutive LOW counter

// ============================================================
// Init
// ============================================================
void initWaterSensor() {
    // gpio_reset_pin() clears any peripheral allocation (ADC, IO matrix) before INPUT_PULLUP
    gpio_reset_pin((gpio_num_t)WATER_SENSOR_PIN);
    pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);

    currentPhase  = PHASE_IDLE;
    phaseStartMs  = 0;
    lastCheckMs   = 0;
    debounceCount = 0;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("Water sensor initialized on GPIO %d", WATER_SENSOR_PIN);
    LOG_INFO("Debounce: %d ms interval × %d LOWs = %d ms effective",
             DEBOUNCE_INTERVAL_MS, DEBOUNCE_COUNTER,
             DEBOUNCE_INTERVAL_MS * DEBOUNCE_COUNTER);
    LOG_INFO("====================================");
}

// ============================================================
// Raw pin read
// ============================================================
bool readWaterSensor() {
    return digitalRead(WATER_SENSOR_PIN) == LOW;
}

// ============================================================
// Process reset — back to IDLE
// ============================================================
void resetSensorProcess() {
    currentPhase  = PHASE_IDLE;
    phaseStartMs  = 0;
    lastCheckMs   = 0;
    debounceCount = 0;
    LOG_INFO("Sensor: reset to IDLE");
}

// ============================================================
// Main logic — called from loop()
// ============================================================
void updateWaterSensor() {
    if (isSystemDisabled()) return;

    // Do nothing while the algorithm pumps, logs or is in error
    AlgorithmState algState = waterAlgorithm.getState();
    if (algState == STATE_PUMPING  ||
        algState == STATE_LOGGING  ||
        algState == STATE_ERROR) {
        return;
    }

    uint32_t now = millis();

    // Interval between samples
    if (now - lastCheckMs < (uint32_t)DEBOUNCE_INTERVAL_MS) {
        return;
    }
    lastCheckMs = now;

    bool sensorLow = readWaterSensor();

    switch (currentPhase) {

        // ======================================================
        // PHASE_IDLE — waiting for the first LOW
        // ======================================================
        case PHASE_IDLE:
            if (sensorLow) {
                currentPhase  = PHASE_DEBOUNCING;
                phaseStartMs  = now;
                debounceCount = 1;
                LOG_INFO("");
                LOG_INFO("SENSOR: First LOW — debounce started (1/%d)", DEBOUNCE_COUNTER);
                waterAlgorithm.onDebounceStart();
            }
            break;

        // ======================================================
        // PHASE_DEBOUNCING — counting consecutive LOW samples
        // ======================================================
        case PHASE_DEBOUNCING:
            if (sensorLow) {
                debounceCount++;
                LOG_INFO("SENSOR: LOW %d/%d", debounceCount, DEBOUNCE_COUNTER);

                if (debounceCount >= DEBOUNCE_COUNTER) {
                    LOG_INFO("");
                    LOG_INFO("SENSOR: Debounce complete — triggering algorithm");
                    waterAlgorithm.onDebounceComplete();
                    // The algorithm resets the process via resetSensorProcess()
                }
            } else {
                // HIGH — reset the counter, back to IDLE
                if (debounceCount > 0) {
                    LOG_INFO("SENSOR: HIGH — counter reset (was %d/%d)",
                             debounceCount, DEBOUNCE_COUNTER);
                }
                debounceCount = 0;
                currentPhase  = PHASE_IDLE;
                waterAlgorithm.onDebounceReset();
            }
            break;
    }
}

// ============================================================
// State getters
// ============================================================
SensorPhase getSensorPhase() {
    return currentPhase;
}

const char* getSensorPhaseString() {
    switch (currentPhase) {
        case PHASE_IDLE:      return "IDLE";
        case PHASE_DEBOUNCING: return "DEBOUNCING";
        default:              return "UNKNOWN";
    }
}

uint8_t getDebounceCounter() {
    return debounceCount;
}

uint32_t getPhaseElapsedMs() {
    if (currentPhase == PHASE_IDLE) return 0;
    return millis() - phaseStartMs;
}
