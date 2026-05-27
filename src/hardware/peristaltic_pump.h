#ifndef PERISTALTIC_PUMP_H
#define PERISTALTIC_PUMP_H

#include <Arduino.h>

// ============================================================
// Pompa perystaltyczna — sterownik TMC2209 (standalone)
//
// Połączenia PCB (hardwired):
//   EN   → GND              (sterownik zawsze aktywny)
//   DIR  → GND/3V3          (kierunek ustalony na stałe)
//   MS1  → GND              (1/8 mikrokroku: MS2=0 MS1=0)
//   MS2  → GND
//
// Sterowanie programowe:
//   GPIO PERYSTALTIC_PUMP_DRIVER_PIN → STEP
//   Sygnał STEP generowany przez LEDC (hardware PWM)
//   50% duty cycle, konfigurowana częstotliwość = kroki/sekundę
//
// Przelicznik SPS ↔ RPM:
//   SPS = RPM * STEPS_PER_REV * MICROSTEP_DIV / 60
//   Przykład: 100 RPM, 200 krok/obr, 1/8 → 100*200*8/60 ≈ 2667 SPS
// ============================================================

// ---- Domyślna prędkość ----
#define PERISTALTIC_DEFAULT_SPS    2667U   // ~100 RPM przy 200 krok/obr, 1/8 MS

// ---- Rozdzielczość LEDC ----
#define PERISTALTIC_LEDC_RESOLUTION  10    // 10-bit → duty 0-1023
#define PERISTALTIC_LEDC_DUTY_50     512   // 50% duty cycle


// ============== API ==============

void     initPeristalticPump();
void     updatePeristalticPump();   // Wywołaj z loop()

// Uruchamia pompę na określony czas (sekundy). 0 = do ręcznego stop.
void     startPeristalticPump(uint32_t durationSeconds, uint32_t stepsPerSec = PERISTALTIC_DEFAULT_SPS);
void     stopPeristalticPump();

bool     isPeristalticPumpRunning();
uint32_t getPeristalticRemainingSeconds();
uint32_t getPeristalticElapsedMs();

#endif // PERISTALTIC_PUMP_H
