#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

void initBuzzer();
void buzzerPowerOn();           // 1000ms — wywołaj jako pierwsze w setup()
void buzzerOK();                // krótki + długi — wszystko gotowe
void buzzerCritical();          // 4× 250ms — błąd krytyczny startu
void updateBuzzerProvisioning(); // non-blocking tick — wywołuj w pętli provisioning
void updateBuzzerAlarm();        // non-blocking tick co 5s — alarm rezerwy, wywołuj w loop()

#endif // BUZZER_CONTROLLER_H
