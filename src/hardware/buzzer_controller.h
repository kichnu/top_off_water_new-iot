#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

void initBuzzer();
void buzzerPowerOn();           // 1000ms — wywołaj jako pierwsze w setup()
void buzzerOK();                // krótki + długi — wszystko gotowe
void buzzerCritical();          // 4× 250ms — błąd krytyczny startu
void updateBuzzerProvisioning(); // non-blocking tick 50ms/5s — wywołuj w pętli provisioning
void updateBuzzerWarning();      // non-blocking tick 50ms/5s — niski stan rezerwy, wywołuj w loop()
void updateBuzzerAlarm();        // non-blocking podwójny tick co 5s — pusta rezerwa, wywołuj w loop()

#endif // BUZZER_CONTROLLER_H
