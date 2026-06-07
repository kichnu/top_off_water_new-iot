# Sterowanie buzzerem na ESP32-C6 — metoda FreeRTOS task

## Problem z podejściem polling (millis() % N)

Pierwotna implementacja wywoływała `digitalWrite()` z `millis() % 5000`
wewnątrz głównej pętli (`loop()`) co ~100ms. Powodowało to:

- Okna ON krótsze niż interwał pollingu (50ms < 100ms) były pomijane
  lub trafiane losowo, co dawało niestabilną liczbę tyknięć na cykl.
- Czas trwania tykniecia zależał od wyrównania fazy millis() do
  interwału pollingu, a nie od kodu.
- Efekt u użytkownika: 3 tyki zamiast 1, lub brak podwójnego tiku
  w trybie alarmowym.

## Rozwiązanie: dedykowany FreeRTOS task

```
xTaskCreate(buzzerTask, "buzzer", 1024, nullptr, 1, nullptr);
```

Task budzony jest co `BUZZER_TICK_MS` (50ms) przez `vTaskDelayUntil()`.
Czas wznowienia jest korygowany o czas wykonania, więc drift nie
kumuluje się.

Wewnątrz taska licznik `tick` (0 .. BUZZER_CYCLE_TICKS-1) wyznacza
pozycję w cyklu. Wzorce definiowane są przez numery ticków:

```
BUZZER_CYCLE_MS   = 12000 ms  → BUZZER_CYCLE_TICKS = 240
BUZZER_TICK_MS    =    50 ms
BUZZER_ALARM_GAP  =   100 ms  → BUZZER_ALARM_TICK2 = 3

WARNING/PROV:  tick 0              → ON (50ms), reszta OFF
ALARM:         tick 0, tick 3      → ON (50ms), przerwa 100ms, ON (50ms), reszta OFF
```

## Komunikacja main loop → task

Jedna zmienna `volatile BuzzerMode g_mode`. Zapis 1-bajtowego enuma
jest atomiczny na ESP32 (architektura RISC-V/Xtensa), mutex zbędny.

`setBuzzerMode()` może być wywoływane z dowolnego miejsca kodu.

## Dodawanie nowych wzorców

1. Dodaj wpis do `enum BuzzerMode` w `buzzer_controller.h`.
2. Dodaj `case` w `switch(g_mode)` wewnątrz `buzzerTask()`.
3. Wzorzec wyraź jako zestaw numerów ticków: `on = (tick == X) || ...`
4. Wywołaj `setBuzzerMode(NOWY_TRYB)` z odpowiedniego miejsca w kodzie.

## Sygnały bootowe (blokujące)

`buzzerPowerOn()`, `buzzerOK()`, `buzzerCritical()` używają blokującego
`delay()` — wywoływane tylko podczas `setup()`, przed startem schedulera
FreeRTOS, więc konflikt z taskiem jest niemożliwy (task startuje po
`initBuzzer()` → po `xTaskCreate()`, a scheduler działa od początku
`setup()` w Arduino-ESP32, ale task nie zdąży wykonać żadnego tiku
przed powrotem z beep()).

Uwaga: jeśli `buzzerOK()` / `buzzerCritical()` będą wywoływane po
starcie pętli (np. w handlerach web), należy dodać mutex lub
tymczasowo ustawić `g_mode = BUZZER_OFF` i wyłączyć task na czas
sygnału.
