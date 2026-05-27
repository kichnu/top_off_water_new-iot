#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>

// ============================================================
// Czujniki pływakowe — para równoległa na jednym pinie (WATER_SENSOR_PIN).
// Logika: LOW = woda poniżej poziomu (dowolny czujnik reaguje).
//
// Debouncing: próbkowanie co DEBOUNCE_INTERVAL_MS,
//             wymagane DEBOUNCE_COUNTER kolejnych LOW.
// ============================================================

enum SensorPhase {
    PHASE_IDLE = 0,
    PHASE_DEBOUNCING
};

// ============== INICJALIZACJA I AKTUALIZACJA ==============
void initWaterSensor();
void updateWaterSensor();      // Wywołaj z loop()

// ============== ODCZYT ==============
bool readWaterSensor();        // Surowy odczyt GPIO (true = LOW = woda poniżej poziomu)
void resetSensorProcess();     // Powrót do PHASE_IDLE

// ============== GETTERY STANU (dla UI / diagnostyki) ==============
SensorPhase getSensorPhase();
const char* getSensorPhaseString();
uint8_t     getDebounceCounter();    // Bieżąca liczba potwierdzonych LOW
uint32_t    getPhaseElapsedMs();     // ms od startu bieżącej fazy

#endif // WATER_SENSORS_H
