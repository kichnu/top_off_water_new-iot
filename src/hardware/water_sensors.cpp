#include "water_sensors.h"
#include "hardware_pins.h"
#include "../core/logging.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"

// ============================================================
// Stan wewnętrzny
// ============================================================
static SensorPhase currentPhase  = PHASE_IDLE;
static uint32_t    phaseStartMs  = 0;   // millis() startu bieżącej fazy
static uint32_t    lastCheckMs   = 0;   // millis() ostatniego próbkowania
static uint8_t     debounceCount = 0;   // licznik kolejnych LOW

// ============================================================
// Inicjalizacja
// ============================================================
void initWaterSensor() {
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
// Surowy odczyt pinu
// ============================================================
bool readWaterSensor() {
    return digitalRead(WATER_SENSOR_PIN) == LOW;
}

// ============================================================
// Reset procesu — powrót do IDLE
// ============================================================
void resetSensorProcess() {
    currentPhase  = PHASE_IDLE;
    phaseStartMs  = 0;
    lastCheckMs   = 0;
    debounceCount = 0;
    LOG_INFO("Sensor: reset to IDLE");
}

// ============================================================
// Główna logika — wywoływana z loop()
// ============================================================
void updateWaterSensor() {
    // Nie przetwarzaj gdy algorytm pompuje, loguje lub jest w błędzie
    AlgorithmState algState = waterAlgorithm.getState();
    if (algState == STATE_PUMPING  ||
        algState == STATE_LOGGING  ||
        algState == STATE_ERROR) {
        return;
    }

    uint32_t now = millis();

    // Odstęp między próbkowaniami
    if (now - lastCheckMs < (uint32_t)DEBOUNCE_INTERVAL_MS) {
        return;
    }
    lastCheckMs = now;

    bool sensorLow = readWaterSensor();

    switch (currentPhase) {

        // ======================================================
        // PHASE_IDLE — czekamy na pierwsze LOW
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
        // PHASE_DEBOUNCING — liczymy kolejne LOW
        // ======================================================
        case PHASE_DEBOUNCING:
            if (sensorLow) {
                debounceCount++;
                LOG_INFO("SENSOR: LOW %d/%d", debounceCount, DEBOUNCE_COUNTER);

                if (debounceCount >= DEBOUNCE_COUNTER) {
                    LOG_INFO("");
                    LOG_INFO("SENSOR: Debounce complete — triggering algorithm");
                    waterAlgorithm.onDebounceComplete();
                    // Algorytm odpowiada za reset procesu przez resetSensorProcess()
                }
            } else {
                // HIGH — reset licznika, wróć do IDLE
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
// Gettery stanu
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
