#include "peristaltic_pump.h"
#include "hardware_pins.h"
#include "../core/logging.h"

// ============================================================
// Stan wewnętrzny
// ============================================================
static bool     pumpRunning     = false;
static uint32_t startMs         = 0;
static uint32_t durationMs      = 0;   // 0 = nieskończony (ręczny stop)
static uint32_t currentSps      = 0;

// ============================================================
// Inicjalizacja
// ============================================================
void initPeristalticPump() {
    // LEDC: podepnij pin STEP, ustaw częstotliwość domyślną, duty=0 (stop)
    ledcAttach(PERYSTALTIC_PUMP_DRIVER_PIN, PERISTALTIC_DEFAULT_SPS, PERISTALTIC_LEDC_RESOLUTION);
    ledcWrite(PERYSTALTIC_PUMP_DRIVER_PIN, 0);

    pumpRunning = false;
    startMs     = 0;
    durationMs  = 0;
    currentSps  = 0;

    LOG_INFO("Peristaltic pump initialized on GPIO %d (LEDC STEP)", PERYSTALTIC_PUMP_DRIVER_PIN);
}

// ============================================================
// Wywołaj z loop() — auto-stop po upłynięciu czasu
// ============================================================
void updatePeristalticPump() {
    if (!pumpRunning) return;
    if (durationMs == 0) return;   // tryb nieskończony — bez auto-stop

    if (millis() - startMs >= durationMs) {
        ledcWrite(PERYSTALTIC_PUMP_DRIVER_PIN, 0);
        pumpRunning = false;
        LOG_INFO("Peristaltic pump: auto-stop after %lu s", durationMs / 1000UL);
    }
}

// ============================================================
// Start
// ============================================================
void startPeristalticPump(uint32_t durationSeconds, uint32_t stepsPerSec) {
    if (pumpRunning) {
        LOG_WARNING("Peristaltic pump already running — ignoring start");
        return;
    }

    currentSps = stepsPerSec;
    durationMs = durationSeconds * 1000UL;

    // Zmień częstotliwość LEDC na żądaną prędkość (pin już podpięty przez initPeristalticPump)
    ledcChangeFrequency(PERYSTALTIC_PUMP_DRIVER_PIN, stepsPerSec, PERISTALTIC_LEDC_RESOLUTION);
    ledcWrite(PERYSTALTIC_PUMP_DRIVER_PIN, PERISTALTIC_LEDC_DUTY_50);

    pumpRunning = true;
    startMs     = millis();

    if (durationSeconds == 0) {
        LOG_INFO("Peristaltic pump: START continuous @ %lu SPS", stepsPerSec);
    } else {
        LOG_INFO("Peristaltic pump: START %lu s @ %lu SPS", durationSeconds, stepsPerSec);
    }
}

// ============================================================
// Stop
// ============================================================
void stopPeristalticPump() {
    if (!pumpRunning) return;

    ledcWrite(PERYSTALTIC_PUMP_DRIVER_PIN, 0);
    pumpRunning = false;

    uint32_t elapsed = (millis() - startMs) / 1000UL;
    LOG_INFO("Peristaltic pump: STOP (ran %lu s)", elapsed);
}

// ============================================================
// Gettery
// ============================================================
bool isPeristalticPumpRunning() {
    return pumpRunning;
}

uint32_t getPeristalticRemainingSeconds() {
    if (!pumpRunning || durationMs == 0) return 0;
    uint32_t elapsed = millis() - startMs;
    if (elapsed >= durationMs) return 0;
    return (durationMs - elapsed) / 1000UL;
}

uint32_t getPeristalticElapsedMs() {
    if (!pumpRunning) return 0;
    return millis() - startMs;
}
