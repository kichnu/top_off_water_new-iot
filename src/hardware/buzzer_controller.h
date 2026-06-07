#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

// ============================================================
// Parametry czasowe — edytuj tutaj, obowiązują wszystkie tryby.
// ============================================================
#define BUZZER_TICK_MS        50    // rozdzielczość taska FreeRTOS [ms]
#define BUZZER_CYCLE_MS    12000    // okres cyklu warn/alarm [ms]
#define BUZZER_CYCLE_TICKS (BUZZER_CYCLE_MS / BUZZER_TICK_MS)  // 240 ticków

// Przerwa między dwoma tikami w BUZZER_ALARM [ms] — musi być wielokrotnością BUZZER_TICK_MS
#define BUZZER_ALARM_GAP_MS  100
#define BUZZER_ALARM_TICK2   (BUZZER_ALARM_GAP_MS / BUZZER_TICK_MS + 1)  // tick 3

// ============================================================
// Tryby buzzera
// Aby dodać nowy wzorzec: dodaj wpis poniżej i obsłuż go
// w switch() wewnątrz buzzerTask() w buzzer_controller.cpp.
// Wszystkie tryby dzielą BUZZER_TICK_MS i BUZZER_CYCLE_MS.
// ============================================================
enum BuzzerMode {
    BUZZER_OFF = 0,
    BUZZER_PROVISIONING,   // tick 0 ON — tryb konfiguracji
    BUZZER_WARNING,        // tick 0 ON — niski stan rezerwy
    BUZZER_ALARM,          // tick 0 i BUZZER_ALARM_TICK2 ON — pusta rezerwa
    // TODO: kolejne tryby (np. daily limit, red alert EMA)
};

void initBuzzer();          // inicjalizacja GPIO + start FreeRTOS task
void setBuzzerMode(BuzzerMode mode);

void buzzerPowerOn();       // 1000ms — wywołaj jako pierwsze w setup()
void buzzerOK();            // krótki + długi — wszystko gotowe
void buzzerCritical();      // 4x 250ms — błąd krytyczny startu

#endif // BUZZER_CONTROLLER_H
