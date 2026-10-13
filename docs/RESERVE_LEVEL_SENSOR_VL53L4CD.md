# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Data: 2026-09-16. Kontekst: zastępuje/uzupełnia obecny mechanizm `reserve_controller.cpp`
(binarny pływak na `AVAILABLE_WATER_SENSOR_PIN`, GPIO 6, + software'owy licznik ml
odejmowany po każdej dolewce). Testy fizyczne czujnika: patrz
`~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md` i `data.md`.

## 1. Cel

Ciągły, liniowy pomiar objętości wody w zbiorniku rezerwy (pojemnik zasilający
pompę ATO) na podstawie odległości mierzonej z góry do lustra wody, zamiast
tylko binarnej informacji "pusty/niepusty".

## 2. Sprzęt / okablowanie

- VL53L4CD podłączony do wspólnej magistrali I2C razem z DS3231 (RTC) i FRAM
  (MB85RC256V) — GPIO 20 (SDA) / GPIO 21 (SCL), bez zmian w `hardware_pins.h`.
- Adresy I2C bez konfliktu: RTC 0x68, FRAM 0x50 (typowo), VL53L4CD domyślnie
  0x29. XSHUT/INT niepodłączony (jak w teście na S3-Pico) — jeden czujnik,
  adres domyślny wystarcza.
- `Wire.setClock(100000)` — bez zmian, VL53L4CD działa też w standard-mode
  (test w `data.md` używał 400kHz, ale nie jest to wymóg).
- **Mechaniczny pływak na GPIO 6 (`AVAILABLE_WATER_SENSOR_PIN`) NIE jest
  demontowany.** Zostaje jako niezależny checkpoint bezpieczeństwa — patrz
  sekcja 6.
- Montaż: czujnik ~100 mm nad lustrem wody przy pełnym zbiorniku, zbiornik
  ~300 mm wysokości → zakres pomiaru O (pusty) do FULL (pełny) to ok.
  100–400 mm. Mieści się z dużym zapasem w deklarowanym zakresie czujnika
  (5–1300 mm), sygnał powinien być mocny na całym zakresie (w testach na
  ~218 mm sygnał ~7600 kcps).

## 3. FRAM — nowa sekcja konfiguracji

Wolne miejsce: 0x05A2–0x05FF (94 B, między blokiem EMA a ring bufferem
dolewek). Nowa struktura `ReserveDistanceConfig` (wzorem `TopOffConfig`:
magic + checksum osobno):

```
0x05A4  uint16_t dist_empty_mm     // dystans przy punkcie "O" (pusty)
0x05A6  uint16_t dist_full_mm      // dystans przy punkcie "FULL" (pełny)
0x05A8  uint32_t capacity_ml       // pojemność zbiornika między O i FULL
0x05AC  uint8_t  sample_count      // N próbek na serię pomiarową
0x05AD  uint8_t  _pad
0x05AE  uint16_t interval_s        // interwał pomiaru cyklicznego (s)
0x05B0  uint8_t  magic             // 0xA7 (spójne z TopOffConfig)
0x05B1  uint8_t  _pad2
        -- 16 bajtów struktury --
0x05B2  uint16_t checksum
```

Funkcje w `fram_controller.*`, wzorem `saveTopOffConfigToFRAM`/
`loadTopOffConfigFromFRAM`: `saveReserveDistanceConfigToFRAM`,
`loadReserveDistanceConfigFromFRAM`.

Bieżąca objętość (`g_reserveMl`) — nadal trzymana jak dziś (pole w
`reserve_controller.cpp`, adresy `FRAM_ADDR_AVAIL_VOL_*` bez zmian), ale
**źródłem wartości staje się pomiar dystansu**, a nie odejmowanie dawki po
dolewce (patrz sekcja 5).

## 4. Algorytm pomiaru

Reżim niższy niż w testach laboratoryjnych (tam N=30 dla powtarzalności
~0.5 mm) — przy wymaganej dokładności ±5 mm i praktycznym braku falowania
(<1 mm nawet podczas pracy pompy dolewki) wystarczy dużo mniejsze N.
Uproszczenia względem kodu testowego z `data.md`:

- **Brak logiki restart-on-motion** (próg 5×sigma) — niepotrzebna przy tak
  niskim falowaniu w tej aplikacji; zwykłe uśrednienie serii wystarcza.
- Odrzucanie próbek ze `status != 0`.
- Budżet per próbka: można zostawić uproszczoną wersję wzoru z testów
  (`budget = clamp(5000/N, 10, 200)ms`) albo ustawić stały krótki budget
  (np. 50 ms/próbka — tryb "balanced"), do potwierdzenia dopiero po testach
  na rzeczywistym zbiorniku (patrz sekcja 8, punkt 1).
- Mapowanie liniowe: zbiornik ma odpowiedni (stały) przekrój na całej
  wysokości — potwierdzone, więc:

  ```
  ml = capacity_ml * clamp((dist_empty_mm - dist_raw_mm), 0, range_mm) / range_mm
  range_mm = dist_empty_mm - dist_full_mm
  ```

## 5. Integracja z `reserve_controller.cpp`

- `g_reserveMl` przestaje być odejmowany po dawce (`subtractReserveMl`) —
  zamiast tego odświeżany bezpośrednio z pomiaru dystansu po każdej serii.
  `subtractReserveMl()` można zostawić jako fallback (patrz sekcja 6), ale
  główną ścieżką staje się `updateReserveVolumeFromDistance(ml)`.
- `setReserveConfigMl()` / `RESERVE_ML_DEFAULT` zastąpione kalibracją O/FULL
  + `capacity_ml` z nowej sekcji FRAM — `getReserveConfigMl()` zwraca po
  prostu `capacity_ml`.
- `isReserveEmpty()` — nadal `g_reserveMl == 0`, ale teraz to wynik pomiaru,
  nie licznika.

## 6. Pływak GPIO 6 jako checkpoint (mechanika zostaje bez zmian)

Mechaniczny pływak nie jest demontowany — wykorzystany w kodzie jako
niezależny punkt kontrolny:

- Jeśli ToF działa poprawnie: pływak LOW (potwierdzone przez istniejący
  debounce 3 s) powinien pokrywać się z obliczonym `ml` bliskim zera.
  Rozjazd > ustalonego marginesu → log ostrzeżenia (niekalibrowany punkt O,
  przesunięty czujnik, kondensacja na soczewce itp.) — diagnostyka, nie
  blokada.
- Jeśli ToF **nie działa** (status błędu, brak I2C ACK, itp.): pływak
  pozostaje jedynym twardym sygnałem "pusty" → blokada pompy niezależnie od
  ostatniego dobrego odczytu dystansu. To zachowuje dzisiejszą ścieżkę
  bezpieczeństwa (`isReserveSensorLow()` / debounce / alarm) bez zmian.

## 7. Harmonogram pomiaru w `loop()`

Pomiar cykliczny (docelowo ~1 min) jest **niekrytyczny** — niski priorytet,
nie może zaburzać timingu debounce'u głównego pływaka ATO ani (docelowo)
`ArduinoOTA.handle()`. Podejście:

- Nieblokujący state machine analogiczny do `PHASE_IDLE`/`PHASE_DEBOUNCING`
  w `water_sensors.cpp`: pojedyncza próbka per iteracja `loop()` (nie cała
  seria N próbek w jednym wywołaniu blokującym), rozłożona seria pomiarowa
  na kilka przebiegów pętli.
- Wyzwalacze: (a) timer cykliczny co `interval_s` (nie-blokujący, oparty na
  `millis()`, jak istniejący debounce rezerwy), (b) opcjonalnie po
  zakończeniu cyklu dolewki (feedback/walidacja pompy — do potwierdzenia
  czy wchodzi w v1, patrz sekcja 8), (c) on-demand z GUI (`settings`).
- Pomiar cykliczny ma niższy priorytet niż stan maszyny ATO — jeśli trwa
  aktywna dolewka/debounce głównego pływaka, seria pomiaru dystansu może
  poczekać do najbliższej wolnej iteracji (podobnie jak dziś `framBusy`
  chroni zapisy FRAM).

## 8. API / GUI — sekcja "settings"

Nowe endpointy (form-encoded POST, zgodnie z konwencją projektu — nigdy
JSON w body POST):

- `POST /api/reserve-distance/calibrate-empty` — zapisuje bieżący odczyt
  jako `dist_empty_mm`
- `POST /api/reserve-distance/calibrate-full` — zapisuje bieżący odczyt
  jako `dist_full_mm`
- `POST /api/reserve-distance/set-capacity` — `capacity_ml`
- `GET  /api/reserve-distance/status` — bieżący dystans, obliczone ml,
  status ostatniej serii, rozjazd względem pływaka (sekcja 6)

Wszystko w istniejącej ukrytej sekcji "settings" w dashboardzie.

## 9. Kolejność wdrożenia

1. Sterownik VL53L4CD (biblioteka Pololu/ST) + inicjalizacja na wspólnym
   I2C, test odczytu przez USB-serial (bez integracji z algorytmem).
2. Nowa sekcja FRAM (`ReserveDistanceConfig`) + save/load.
3. State machine pomiaru cyklicznego w `loop()` (nieblokujący).
4. Podłączenie do `reserve_controller.cpp` (mapowanie ml, checkpoint
   pływaka).
5. API + GUI (kalibracja, status).
6. Testy na rzeczywistym zbiorniku — dobór finalnego N i interwału
   (sekcja 10, punkt 1).
7. (Opcjonalnie, po OTA) iteracyjne dostrajanie progów przez WiFi zamiast
   USB — zależne od kolejności OTA vs sensor ustalonej wcześniej.

## 10. Otwarte punkty (do dalszego ustalenia)

1. **Finalne N i budżet próbki** — do wyboru dopiero po testach na
   realnym zbiorniku (obiecujące testy wstępne już są, ale liczba próbek
   nie jest jeszcze zamknięta).
2. **Czy pomiar po cyklu dolewki wchodzi do v1** — jako feedback/walidacja
   działania pompy (wykrycie np. zapowietrzenia, zatkania) — czy zostaje
   na etap 2, po ustabilizowaniu samego pomiaru cyklicznego.
3. **Próg rozjazdu ToF vs pływak** (sekcja 6) — jaka różnica ml uznawana
   za "podejrzaną" i wymagającą logu ostrzeżenia.
4. **Finalny interwał cykliczny** — 1 min jako punkt startowy, do
   potwierdzenia po obserwacji rzeczywistego zużycia (parowanie + dolewki)
   czy nie za rzadko/za często.
5. **Zachowanie przy błędzie sensora** (status ≠ 0 stale, brak ACK na I2C)
   — poza fallbackiem na pływak: czy generować osobny alarm w GUI
   ("czujnik dystansu niesprawny") niezależny od alarmu "rezerwa pusta".
6. **Jednostki/zaokrąglenia w GUI** — czy pokazywać ml, %, czy oba.
