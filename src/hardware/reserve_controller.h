#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

#define RESERVE_ML_DEFAULT   2000u   // ml — wartość gdy brak danych w FRAM
#define RESERVE_ML_MIN          1u
#define RESERVE_ML_MAX      10000u

// Inicjalizacja — ładuje z FRAM, konfiguruje GPIO
void initReserveController();

// Odczyt czujnika — wywołuj w każdej iteracji loop()
void updateReserveSensor();

// Stan czujnika fizycznego (GPIO LOW = brak wody = warning żółte)
bool isReserveSensorLow();

// true gdy czujnik był LOW i wrócił do HIGH, ale użytkownik jeszcze nie nacisnął Cycle Reset
bool isReserveNeedsReset();

// Bieżący stan rezerwy
uint32_t getReserveMl();
uint32_t getReserveConfigMl();
bool     isReserveEmpty();   // currentMl == 0 → alarm czerwony, blokada pompy

// Odejmuje ml po cyklu dolewania — klampuje do 0, zapisuje FRAM
void subtractReserveMl(uint16_t ml);

// Ustawia skonfigurowaną wartość rezerwy (i resetuje bieżącą do tej wartości)
void setReserveConfigMl(uint32_t ml);

// Reset alarmu — resetuje bieżącą do configMl i zapisuje FRAM.
// Wywołaj po "Cycle Reset" (sprawdź wcześniej isReserveSensorLow() == false!).
void clearReserveAlarm();

#endif // RESERVE_CONTROLLER_H
