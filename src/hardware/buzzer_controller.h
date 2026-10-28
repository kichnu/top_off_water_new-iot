#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

// ============================================================
// Timing — applies to all modes
// ============================================================
#define BUZZER_TICK_MS          50    // FreeRTOS task resolution [ms]
#define BUZZER_CYCLE_MS      12000    // warning / provisioning period [ms]
#define BUZZER_ALARM_CYCLE_MS 120000  // alarm period [ms]

// Gap between the two alarm beeps [ms] — must be a multiple of BUZZER_TICK_MS
#define BUZZER_ALARM_GAP_MS  100
#define BUZZER_ALARM_TICK2   (BUZZER_ALARM_GAP_MS / BUZZER_TICK_MS + 1)  // tick 3

// ============================================================
// Buzzer modes
// To add a pattern: add an entry below and handle it in the
// switch() inside buzzerTask() in buzzer_controller.cpp.
// ============================================================
enum BuzzerMode {
    BUZZER_OFF = 0,
    BUZZER_PROVISIONING,   // tick 0 ON every BUZZER_CYCLE_MS — provisioning mode
    BUZZER_WARNING,        // tick 0 ON every BUZZER_CYCLE_MS — low reserve (legacy counter)
    BUZZER_ALARM,          // ticks 0 and BUZZER_ALARM_TICK2 ON every BUZZER_ALARM_CYCLE_MS —
                           // the single "check the GUI" alarm
};

void initBuzzer();          // GPIO init + FreeRTOS task start
void setBuzzerMode(BuzzerMode mode);

void buzzerPowerOn();       // 1000 ms — call first in setup()
void buzzerOK();            // short + long — boot complete
void buzzerCritical();      // 4x 250 ms — critical boot error
void buzzerConfirm();       // 2x 80 ms — reset button accepted

#endif // BUZZER_CONTROLLER_H
