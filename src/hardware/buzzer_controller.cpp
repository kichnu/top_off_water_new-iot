#include "buzzer_controller.h"
#include "hardware_pins.h"
#include <Arduino.h>

static void beep(uint32_t ms) {
    digitalWrite(BUZZER_PIN, LOW);
    delay(ms);
    digitalWrite(BUZZER_PIN, HIGH);
}

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);  // HIGH = off (active LOW)
}

void buzzerPowerOn() {
    beep(1000);
}

// Narastający: krótki + długi = "ta-DAAA" = wszystko gra
void buzzerOK() {
    beep(100);
    delay(150);
    beep(500);
}

// Natarczywa czwórka = "nie ignoruj tego" = błąd krytyczny
void buzzerCritical() {
    for (int i = 0; i < 4; i++) {
        beep(250);
        if (i < 3) delay(100);
    }
}

// Pojedynczy tick 50ms co 5s — sygnalizuje aktywny tryb provisioning.
// Wywołuj w każdej iteracji pętli provisioning.
void updateBuzzerProvisioning() {
    uint32_t phase = millis() % 5000;
    digitalWrite(BUZZER_PIN, phase < 50 ? LOW : HIGH);
}

// Pojedynczy tick 50ms co 5s — ostrzeżenie niskiego stanu rezerwy.
// Wywołuj w loop() gdy isReserveSensorLow() == true.
void updateBuzzerWarning() {
    uint32_t phase = millis() % 5000;
    digitalWrite(BUZZER_PIN, phase < 50 ? LOW : HIGH);
}

// Podwójny tick (50ms ON, 100ms OFF, 50ms ON) co 5s — alarm pustej rezerwy.
// Wywołuj w loop() gdy isReserveEmpty() == true.
void updateBuzzerAlarm() {
    uint32_t phase = millis() % 5000;
    bool on = (phase < 50) || (phase >= 150 && phase < 200);
    digitalWrite(BUZZER_PIN, on ? LOW : HIGH);
}
