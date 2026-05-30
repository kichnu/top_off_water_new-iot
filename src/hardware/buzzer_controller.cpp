#include "buzzer_controller.h"
#include "hardware_pins.h"
#include <Arduino.h>

static void beep(uint32_t ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
}

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
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

// Subtelny tick 50ms/5000ms — sygnalizuje aktywny tryb provisioning.
// Wywołuj w każdej iteracji pętli provisioning.
void updateBuzzerProvisioning() {
    uint32_t phase = millis() % 5000;
    digitalWrite(BUZZER_PIN, phase < 50 ? HIGH : LOW);
}
