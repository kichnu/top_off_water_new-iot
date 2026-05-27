# ATO Controller — Instrukcja użytkownika

## Czym jest to urządzenie?

Automatyczny system uzupełniania wody (ATO — Automatic Top-Off) dla akwarium słodko- lub słonowodnego. Czujnik pływakowy wykrywa ubytek wody spowodowany parowaniem, a sterownik uruchamia pompę dolewki, podając stałą dawkę wody RO. Opcjonalnie, zintegrowany scheduler kalkwassera dozuje roztwór wodorotlenku wapnia (kalk) i uruchamia mieszadło.

---

## Pierwsze uruchomienie

Urządzenie startuje zawsze w **Service Mode** — przez pierwsze 15 minut pompy i scheduler są zatrzymane. Jest to celowe: czas na sprawdzenie instalacji, podłączenie węży, weryfikację czujników przed pierwszą dolewką. Po 15 minutach system automatycznie przechodzi do **Auto Mode**.

Jeśli urządzenie nie ma jeszcze skonfigurowanej sieci WiFi, aktywuje portal konfiguracyjny:

- Sieć WiFi: **`ESP32-WATER-SETUP`**
- Hasło sieci: **`setup12345`**
- Po podłączeniu otwórz przeglądarkę — portal pojawi się automatycznie (captive portal) lub przejdź ręcznie pod adres `192.168.4.1`

---

## Dioda statusu (LED)

Dioda LED na obudowie informuje o stanie systemu bez konieczności otwierania panelu:

| Wzorzec | Stan |
|---------|------|
| 3× szybki błysk, 2s przerwa | **BŁĄD** — wymagana interwencja (patrz: sytuacje wymagające interwencji) |
| Świeci ciągle | **Pompa aktywna** — dolewka lub kalkwasser w toku |
| 2× błysk co 3s | **Service Mode** — system wstrzymany |
| Szybkie mruganie 200ms | **Łączenie z WiFi** (faza rozruchu) |
| Pojedynczy puls co 3s | **Auto Mode** — praca normalna, WiFi OK |
| Puls co 3s, co trzeci = 3× szybki błysk | **Auto Mode** — praca normalna, **brak WiFi** (ograniczona funkcjonalność) |

---

## Panel sterowania (Dashboard)

Dostęp przez przeglądarkę pod adresem IP urządzenia w sieci lokalnej (widoczny w Serial Monitor lub routerze). Domyślna sesja wygasa po 30 minutach bezczynności.

### Pasek statusu (górny)

Pokazuje aktualny stan w czasie rzeczywistym:

- **ATO** — stan pompy dolewki (IDLE / DEBOUNCING / PUMPING / LOGGING / ERROR)
- **Kalk** — stan schedulera kalkwassera
- **Mixing** / **Peristaltic** — aktywność pomp kalkwassera
- Badge alarmowy — pojawia się przy ostrzeżeniu (żółty) lub błędzie krytycznym (czerwony)

### Karta 1 — Status i wykresy

Wykresy historii dolewek (canvas, ostatnie 60 cykli):
- **Rate [ml/h]** — tempo odparowania wody. Linia EMA (średnia wykładnicza) + żółty/czerwony pas odchylenia. Punkt żółty = odchylenie powyżej progu ostrzeżenia; czerwony = przekroczenie progu alarmowego (RED ALERT).
- **Interval [min]** — czas między kolejnymi dolewkami. Długi interwał = mało paruje; krótki = dużo.

Wartości żółte w historii to **tylko wskazówka wizualna** — system nie zatrzymuje się. Czerwony alert → **STATE_ERROR**, wymagany Cycle Reset.

Rolling 24h — łączny wolumen dolewek z ostatnich 24 godzin (porównaj z limitami).

### Karta 2 — System Control

| Przycisk | Działanie |
|---|---|
| **Auto Mode** (zielony) / **Service Mode** (pomarańczowy) | Przełącza tryb pracy. W Service Mode scheduler i algorytm ATO są zatrzymane; manualne pompy działają normalnie. |
| **Kalkwasser OFF/ON** | Włącza/wyłącza scheduler kalkwassera. Stan przeżywa restart i Cycle Reset — celowo. |
| **Alert Sound ON/OFF** | Wycisza aktualnie grający alarm audio. Mute resetuje się automatycznie gdy alarm ustępuje — przycisk powraca do "ON". |
| **Pump OFF / Pump ON Xs** | Uruchamia pompę dolewki ręcznie na 60 sekund (z odliczaniem). Działa niezależnie od trybu Auto/Service. |
| **Mixing Pump OFF/ON** | Ręczne sterowanie pompą miksera kalkwassera. |
| **Peristaltic OFF/ON** | Ręczne sterowanie pompą perystaltyczną (kalkwasser). |
| **Cycle Reset** | Resetuje **automatyczne** cykle ATO (czyści STATE_ERROR, przerywa DEBOUNCING/PUMPING wywołane przez czujnik/scheduler). Nie przerywa ręcznie uruchomionych pomp. |
| **Vol −/+** | Regulacja głośności alertów audio (DFPlayer Mini). |

### Karta 3 — Kalkwasser

Ustawienia schedulera:
- **Enabled / Daily dose [ml]** — dzienna łączna dawka kalkwassera
- **Kalkwasser Flow Rate** — kalibracja pompy perystaltycznej: uruchom test 30s, zmierz zebraną ciecz, wpisz wynik → system wylicza czas dawki

Harmonogram: mieszanie 4× dziennie (00:15, 06:15, 12:15, 18:15) przez 5 min + 1h odczekania. Dozowanie 16× dziennie (każda pełna godzina poza oknami mieszania).

**Priorytet:** kalkwasser czeka maks. 10 min na zakończenie cyklu ATO przed rozpoczęciem. Jeśli ATO nie skończy w tym czasie — slot przepada (nie jest powtarzany).

### Karta 4 — Ustawienia ATO

- **Dose [ml]** — stała dawka na jeden cykl dolewki (domyślnie 150 ml)
- **Daily limit [ml]** — dobowy limit bezpieczeństwa. Przekroczenie → STATE_ERROR (alarm)
- **EMA alpha** — czułość średniej wykładniczej (0.10–0.40). Wyższe = szybsza reakcja na zmiany, ale więcej szumów.
- **Yellow/Red sigma [%]** — progi alertów jako procent odchylenia standardowego EMA. Np. Yellow 150% = alarm przy odchyleniu > 1.5× EMA_dev.
- **Pump flow rate [ml/s]** — kalibracja pompy ATO (wpływa na czas trwania cyklu)

---

## Algorytm dolewki krok po kroku

1. Czujnik pływakowy opada (brak wody) → start debouncingu (1 sek., 5 próbek)
2. Debouncing OK → sprawdzenie czujnika zbiornika wody uzupełniającej
3. Start pompy na czas `dose_ml / flow_rate_ml_s` sekund
4. Po zakończeniu: aktualizacja EMA, zapis do historii (FRAM ring buffer, 60 rekordów)
5. Sprawdzenie limitów i alertów → IDLE lub STATE_ERROR

**EMA bootstrap:** pierwsze 3 cykle nie generują alertów (brak danych referencyjnych). Po bootstrapie każdy cykl jest porównywany ze wzorcem.

---

## Sytuacje wymagające interwencji

| Stan | Przyczyna | Co zrobić |
|---|---|---|
| **STATE_ERROR: Daily Limit** | Suma dolewek ≥ dobowy limit | Sprawdź szczelność/parowanie, zresetuj licznik 24h lub podnieś limit, naciśnij Cycle Reset |
| **STATE_ERROR: Red Alert** | Tempo odparowania drastycznie odbiega od normy EMA | Sprawdź czy czujnik jest sprawny, czy nie ma wycieku; Cycle Reset |
| **STATE_ERROR: Low Reservoir** | Czujnik zbiornika wody uzupełniającej sygnalizuje pusty zbiornik 4× z rzędu | Uzupełnij zbiornik RO, Cycle Reset |
| **Alert Sound** gra w kółko | Jeden z powyższych błędów jest aktywny | Rozwiąż przyczynę, wycisz przyciskiem, zresetuj cykl |

---

## Czujnik zbiornika wody uzupełniającej

Monitoruje poziom wody w zbiorniku RO/uzupełniającej. Ostrzeżenia narastają przy kolejnych dolewkach gdy zbiornik jest niski (licznik od 1 do 3 = żółty badge). Przy 4. kolejnym zdarzeniu → **STATE_ERROR** (czerwony), system zatrzymuje dolewkę. Licznik resetuje się przy restarcie urządzenia lub gdy zbiornik zostanie uzupełniony (czujnik wróci na HIGH).

---

## Restart i odporność na awarie

- Urządzenie restartuje się automatycznie **o północy (00:00)** czasu lokalnego — wszystkie pompy zatrzymywane bezpiecznie przed restartem
- Konfiguracja EMA, historia cykli i ustawienia są zapisane w FRAM (nieulotna pamięć, przeżywa wyłączenie zasilania)
- Stan kalkwassera (ON/OFF) przeżywa restart, resetuje się tylko ręcznie przez panel
- Po każdym restarcie: 15 min Service Mode → automatyczny powrót do Auto Mode
