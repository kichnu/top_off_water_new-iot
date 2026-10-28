# Dolewka — audyt logiki i plan: rezerwa z poziomem alarmowym na ToF

Wersja 1.0: 2026-10-25 · plik roboczy do realizacji w tym repo (commit bazowy `b13197d`).
Powstał przy planowaniu panelu dotykowego (`~/Dokumenty/My_apps/IOT/home_display-iot`,
`docs/nodes/dolewka.md`). Po zakończeniu prac wypełnić sekcję 7 i przenieść raport do tamtego projektu.

## 1. Kontekst i decyzje

- Pływak rezerwy (`RESERVE_FLOAT_ENABLED`, GPIO 8) zostaje usunięty z kodu (decyzja 2026-10-25).
- W ustawieniach zbiornika dochodzi definiowany poziom alarmowy, leżący poniżej poziomu rezerwy.
  Poziom rezerwy zostaje zwiększony. Stop pompy przy pustym zbiorniku opiera się na czujniku VL53L4CD.
- Uwaga: wg `RESERVE_LEVEL_SENSOR_VL53L4CD.md` §3 czujnik ma dokładność ±2 cm, a skoki między seriami
  wynoszą do 30 mm (~1,7 l przy 56,96 ml/mm). Z tego wynikają dwa warunki:
  - poziom alarmowy musi mieć zapas ≥ maksymalnego skoku ponad ukrytym dnem;
  - potrzebna jest polityka na wypadek awarii czujnika (patrz sekcja 3).
- Równolegle trwa zbieranie logów problemu z padającym czujnikiem. Punkt W4 to hipoteza do sprawdzenia
  przy tej okazji.

## 2. Ustalenia z przeglądu

Waga: K — krytyczne (możliwa szkoda: przelanie, praca na sucho, cichy brak dolewek), W — wysokie,
S — średnie, N — niskie.

| # | Waga | Problem | Miejsce | Propozycja |
|---|---|---|---|---|
| K1 | K | Ręczne porcje omijają blokady. Nie sprawdzają stanu ERROR, limitu dobowego ani `pumpGlobalEnabled` (`startManualPortion()` pisze wprost na pin). Nie trafiają do licznika 24 h. Kolejka 20 × 150 ml = 3 l > domyślny limit 2 l. Po ERROR z powodu podejrzenia wycieku można dalej dolewać ręcznie. | `pump_controller.cpp:50`, `:206`; `web_handlers.cpp:276`; `water_algorithm.cpp:424` | Porcje ręczne zapisywać do ring buffera z flagą `FLAG_MANUAL` (jeden rekord na sesję albo na porcję). Wliczać je do `scanRolling24h()`, ale pomijać w EMA i w `loadLastTopOffRecord()` (interwał i `rateDoseMl`). Przed każdą porcją: poziom alarmowy (sekcja 3) i `rolling + dose ≤ limit` (albo osobny limit sesji — pytanie 6.1). **Zmienione decyzją 2026-10-28 (odp. 6.1): tylko Service Mode, bez blokad i bez wliczania.** |
| K2 | K | Brak stopu przy pustym zbiorniku. Pływak jest wyłączony, więc `isReserveEmpty()` zawsze zwraca false, a `ERROR_LOW_RESERVOIR` nigdy nie wystąpi. | `reserve_controller.cpp`, `water_algorithm.cpp:230` | Sekcja 3. |
| K3 | K | Brak alarmu „za długo bez dolewki”. Pływak sumpa ma `INPUT_PULLUP`, LOW = brak wody. Przerwany przewód lub pływak zablokowany w górze daje HIGH, czyli „woda OK”: pompa nigdy nie rusza i nic nie alarmuje. Sump opada, zasolenie rośnie. | `water_sensors.cpp` | Watchdog: czas od ostatniej dolewki (auto) > `max(k × ema_interval_s, T_min)` (np. k = 3, T_min = 12 h) → alarm `NO_TOPOFF` (P2, buzzer WARNING, bez blokady). Kasuje się sam przy następnej dolewce. |
| W1 | W | Zatrzaśnięte błędy żyją tylko w RAM. Kasuje je nocny restart (codziennie o północy) i każdy zanik zasilania. Po restarcie `alarmArmed = true`, więc przy RED_ALERT wykona się jeszcze jedna dawka, zanim błąd wróci. Restart w trakcie pompowania: `stopPump()` bez `finishPumpCycle()`, więc dawka nie trafia do licznika 24 h. | `main.cpp:255–266`, `water_algorithm.cpp:512` | `lastError` (+ `alarmArmed`) zapisywać w FRAM, przywracać w `initFromFRAM()`. Restart nocny odkładać, gdy `isInCycle()`, trwa sesja ręczna lub kalibracja. |
| W2 | W | Przycisk reset kasuje błąd przy zwolnieniu, bez minimalnego czasu i debounce (`pressStart` nieużywany). Zakłócenie albo przypadkowy dotyk zdejmuje blokadę. | `water_algorithm.cpp:523–538` | Przytrzymanie ≥ 2–3 s z debounce, potwierdzenie buzzerem. |
| W3 | W | Dwa resety o różnej semantyce. `resetFromError()` (przycisk) ustawia `alarmArmed = false`, `resetSystem()` (web) nie. Po resecie przyciskiem przy score ≥ zone_yellow anomalia jest rozbrojona, dopóki score nie spadnie. Przy zablokowanym na dole pływaku chroni wtedy tylko limit dobowy. | `water_algorithm.cpp:544–566` | Jedna funkcja resetu. Rozbrojenie ograniczyć czasowo (np. re-arm po 24 h albo po N cyklach), zachowanie opisać w USER_GUIDE. |
| W4 | W | Brak muteksu I2C między taskiem AsyncTCP a `loop()`. `handleStatus` robi kilka odczytów RTC (`rtc.now()`, retry z `delay(10)`), handlery statystyk, konfiguracji i historii czytają lub piszą FRAM, a VL53 i FRAM pracują w `loop()`. `recoverBus()` robi `Wire.end()` / `Wire.begin()`, choć handler może być w trakcie transakcji. `framBusy` to zwykła flaga, sprawdzana tylko w historii cykli. | `level_sensor.cpp:165`, `web_handlers.cpp:127` (+ FRAM w handlerach), `rtc_controller.cpp:422` | Hipoteza dla padającego czujnika. Test: doba z zamkniętym dashboardem kontra doba z otwartym, porównać liczbę FAULT w `/api/level-events`. Poprawka: rekurencyjny mutex na I2C (wzorzec `core/lamp_lock` z `led_lamp_aquarium-iot`), a w handlerach tylko wartości z cache (czas odświeżany w `loop()` co 1 s). |
| W5 | W | Precheck limitu `rolling >= limit` pozwala przekroczyć limit o jedną dawkę. Walidacja `daily_limit` nie zna dawki: limit 100 ml przy dawce 150 ml daje ERROR w każdym cyklu. | `water_algorithm.cpp:237`, `web_handlers.cpp:519` | `rolling + dose > limit` → ERROR przed dawką. Walidacja `daily_limit ≥ 2 × dose_ml`. |
| S1 | S | OTA: `-DOTA_PASSWORD=${sysenv.OTA_PASSWORD}`. Pusty env przy buildzie oznacza OTA bez hasła. Dozownik przeszedł już na `OTA_PASSWORD_192_168_10_3`. | `platformio.ini:36, 51`, `main.cpp:198` | `OTA_PASSWORD_192_168_10_2` + `static_assert(sizeof(OTA_PASSWORD) > 1, ...)`. |
| S2 | S | Log TCP (port 23) bez whitelisty IP: każdy w sieci IoT czyta logi. | `core/logging.cpp:76` | `isIPAllowed()` przy `accept()`. Sprawdzić, czy żaden log nie zawiera sekretów. |
| S3 | S | Przy błędzie odczytu RTC `getUnixTimestamp()` zwraca stałą 2025-01-01. Rekordy dostają fałszywy czas, po powrocie RTC wypadają z okna 24 h (limit niedoszacowany). Pierwszy interwał po powrocie jest ogromny, więc score spada i maskuje anomalie. | `rtc_controller.cpp:424`, `:465` | Flaga `time_valid`. Przy złym czasie nie aktualizować EMA ani interwału. Limit dobowy wspierać licznikiem monotonicznym (`millis()`) w RAM. |
| S4 | S | `/api/system-toggle` przełącza stan (nieidempotentne), `direct-on` dokłada porcję przy każdym wywołaniu. Przy integracji z panelem (retry SET) stan się odwróci albo porcja zdubluje. | `web_handlers.cpp:367–389`, `:276` | Komenda z wartością docelową (`enabled=0\|1`). `cmd_id` dla porcji — przy wspólnym setterze z panelem. |
| S5 | S | Brak wyciszenia buzzera: ERROR dzwoni do resetu. Rezerwa nie ma żadnego sygnału. | `main.cpp:290` | `alarm_mute` (wzorzec z termostatu: wycisza aktywne, nowe wystąpienie dzwoni). WARNING dla rezerwy — pytanie 6.3. |
| N1 | N | `ERROR_BOTH` nieużywany. Przy jednoczesnym limicie i anomalii widać tylko DAILY_LIMIT. | `algorithm_config.h`, `water_algorithm.cpp:347` | Ustawiać BOTH albo usunąć kod. |
| N2 | N | Pompa niekalibrowana: biegnie 5 s (`DEFAULT_PORTION_MS`), a do licznika 24 h i EMA idzie `dose_ml` (150 ml) — oba fałszywe. | `pump_calibration.cpp:182` | Status „Uwaga” na dashboardzie albo blokada auto bez kalibracji (pytanie 6.4). |
| N3 | N | `manualResetTs` (reset licznika 24 h) tylko w RAM — znika po restarcie. Kierunek bezpieczny. | `water_algorithm.cpp:591` | Udokumentować. |

## 3. Projekt: rezerwa i poziom alarmowy na ToF

### Ustawienia

`ReservoirConfig` ma wolne `_reserved[4]` (kopiowane w `setReservoirConfig`, `reservoir_config.cpp:61`).
W starym configu są tam zera, więc 0 = wartość domyślna — bez zmiany układu FRAM i bez bumpa magic.

| Pole | Znaczenie | Domyślnie | Walidacja |
|---|---|---|---|
| `reserve_pct` (u8) | rezerwa — żółty zakres, ostrzeżenie | ? (dziś stałe 30 %, ma wzrosnąć) | > `alarm_pct` + 2 porcje |
| `alarm_pct` (u8) | poziom alarmowy — stop pompy | ? | ≥ zapas na skok czujnika (30 mm × ml/mm ponad ukrytym dnem); ostrzeżenie w `reservoirConfigWarning()` |

Procenty liczone od pojemności użytkowej (jak dziś `RESERVE_LOW_PCT_FIXED`), zaokrąglane do pełnych
porcji. GUI pokazuje też ml, porcje i mm. Ukryte dno (`RESERVE_HIDDEN_PCT_FIXED`, 10 %, wlot pompy) zostaje
jako fizyczna granica pod poziomem alarmowym. Do decyzji: czy przy nowym poziomie alarmowym nie jest
zbędne (pytanie 6.2).

### Strefy

`OK` → `REZERWA` (≤ reserve) → `ALARM` (≤ alarm) → `NIEZNANY` (brak ważnego odczytu).
Zastępują obecne `OK / LOW / EMPTY / UNKNOWN` (EMPTY = mniej niż 1 porcja, dziś tylko informacyjnie).

### Stop pompy

- Przed każdą dawką (auto, ręczna, start kalibracji): jeśli `poziom − dose < alarm_ml`, to dawka nie
  startuje, a auto przechodzi w `ERROR_LOW_RESERVOIR` (zatrzask, zapis w FRAM — W1).
- Poziom do decyzji o stopie liczony zachowawczo: minimum z ostatnich 2–3 ważnych serii. Groźne są skoki
  w górę (odczyt więcej wody niż jest). Skok w dół daje fałszywy stop, czyli kierunek bezpieczny.
- Kasowanie: reset (GUI / przycisk) przechodzi tylko wtedy, gdy ważny odczyt jest ≥ `alarm_ml + 1 porcja`
  (histereza na szum czujnika). Tak jak dawny zatrzask pływaka.
- Seria ważna tylko gdy `valid`, stan `LEVEL_SENSOR_OK`, poza ciszą po pompie i wiek < `LEVEL_MAX_AGE`
  (np. 15 min). Inaczej strefa `NIEZNANY`.

### Awaria czujnika (polityka fail-safe)

Kluczowe przy obecnym problemie z padaniem czujnika:

- Przy strefie `NIEZNANY` węzeł liczy dawki od ostatniego ważnego odczytu:
  `est = ostatni_ważny_ml − Σ dawek od tego odczytu`. Dawki są dozwolone, dopóki `est − dose ≥ alarm_ml`.
  Potem `ERROR_LOW_RESERVOIR` (przyczyna: brak odczytu).
- Brak jakiegokolwiek ważnego odczytu od startu: auto zablokowane albo dozwolone N porcji (pytanie 6.5).
- Alarm `LEVEL_SENSOR_FAULT` (P2, buzzer WARNING), gdy brak ważnego odczytu > np. 30 min.
- Opcjonalnie później: ten sam licznik dawek jako stałe sprawdzenie krzyżowe (stop przy
  `min(ToF, est)`). Nowa baza po wykryciu dolania (wzrost ≥ 3 porcji potwierdzony 2 seriami).

### Buzzer

`ALARM` przy poziomie alarmowym (jak dawniej pływak) i przy każdym błędzie ATO. Rezerwa: `WARNING` albo
nic (pytanie 6.3).

### Do usunięcia (pływak)

`RESERVE_FLOAT_ENABLED`, `RESERVE_FLOAT_DEBOUNCE_MS`, `RESERVOIR_FLOAT_PIN` (GPIO 8 zostaje wolny),
`g_floatLow` / `g_floatLatched`, `getReserveFloatString()`, pole `reserve_float` w `/api/status`, gałąź
pływaka w `clearReserveLatch()` (zastąpiona zatrzaskiem ToF), stan pływaka w GUI, przypomnienia i §8
w `RESERVE_LEVEL_SENSOR_VL53L4CD.md`.

## 4. Plan realizacji

Każdy krok musi się budować (`pio run -e seeed_xiao_esp32c3`) i kończyć osobnym commitem.

1. Usunięcie pływaka — sam refactor, bez zmiany zachowania (dziś i tak wyłączony).
2. Ustawienia `reserve_pct` / `alarm_pct` + nowe strefy w API i GUI (jeszcze bez stopu).
3. Stop na poziomie alarmowym + polityka awarii czujnika + `LEVEL_SENSOR_FAULT` (K2).
4. Persystencja błędów w FRAM, jeden reset, przycisk z przytrzymaniem, odroczony restart nocny (W1–W3).
5. Ręczne porcje (K1) — wg odp. 6.1: tylko w Service Mode, Service Mode ręczny 180 min.
6. Limit dobowy z lookahead i walidacją względem dawki (W5); flaga `time_valid` (S3).
7. Watchdog braku dolewki `NO_TOPOFF` (K3).
8. Mutex I2C / cache w handlerach (W4) — po teście z sekcji 2 albo wcześniej, jeśli logi wskażą
   kolizje magistrali.
9. OTA i log TCP (S1, S2).
10. Przygotowanie pod panel: idempotentny toggle, `alarm_mute` (S4, S5) — razem z tabelą `ParamDesc`.

Kolejność 1–3 jest sztywna. Od 4 dalej można przestawiać.

Postęp (2026-10-28): kroki 1–3 zrobione i sprawdzone na sprzęcie (pasek, progi). Krok 4 zrobiony
(nietestowany na HW): błąd ATO i czas rozbrojenia w FRAM (`AlgErrorState` 0x05D6), jeden
`resetSystem()` dla GUI i przycisku, przycisk — przytrzymanie 3 s + 2 krótkie piknięcia,
rozbrojenie alarmu anomalii po resecie w czerwonej strefie: 24 h albo do score < zone_yellow
(decyzja 2026-10-28), restart nocny odkładany przy pompie / cyklu / sesji ręcznej / kalibracji.
Krok 5 zrobiony w zmienionej postaci (odp. 6.1): dolewka ręczna tylko w Service Mode, bez blokad
i wliczania; Service Mode ręczny 180 min; restart nocny odkładany także w Service Mode.
Krok 6 zrobiony: limit sprawdzany przed dawką (`rolling + dose > limit`), walidacja limit ≥ 2 ×
dawka (także przy starcie kalibracji nowej dawki), licznik 24 h = max(historia FRAM, dawki od
startu w RAM wg `millis()`); cykl bez ważnego czasu RTC dolewa, ale bez EMA, interwału i rekordu
historii (decyzja 2026-10-28 — zamiast blokady dolewek).
Krok 7 zrobiony: watchdog `NO_TOPOFF` 24 h (alarm krytyczny, buzzer, bez blokady); okres bez
możliwości dolewki (ręczny Service Mode, ERROR, alarm rezerwy) zaczyna 24 h od nowa po swoim
końcu, Service Mode po starcie nie (restart nocny nie zeruje); punkt odniesienia w FRAM
(`AlgErrorState` rozszerzony do 12 B, magic 0xE8); kasowanie: następna dolewka albo reset.

## 5. Weryfikacja

- Scenariusze (`docs/inject_scenario.py` / `ema_simulation.py`, tam gdzie da się zasymulować):
  - poziom spada przez rezerwę do alarmu: ostrzeżenie, potem stop przed dawką, która zeszłaby poniżej;
  - reset przy poziomie poniżej alarmu nie kasuje błędu, po dolaniu kasuje;
  - czujnik FAULT w połowie zbiornika: dawki liczone, stop przy `est`, alarm czujnika;
  - restart w ERROR (zasilanie, północ): błąd wraca z FRAM;
  - kolejka ręczna przy limicie: zablokowana / przycięta;
  - pływak sumpa odłączony: po T_min alarm `NO_TOPOFF`.
- Na sprzęcie: zbiornik testowy (282 × 202 mm), opróżnianie do poziomu alarmowego, log z `:23`.
- Po kroku 8: doba z otwartym dashboardem, zero FAULT związanych z magistralą.

## 6. Pytania otwarte

1. Porcje ręczne a limit dobowy: wliczać i blokować, czy wliczać (blokuje późniejsze auto), ale ręczne
   ograniczyć tylko kolejką? Czy porcje ręczne wolno uruchamiać w stanie ERROR?
   Odp. (2026-10-28): porcje ręczne to świadoma dolewka serwisowa operatora — nigdzie nie wliczane
   (poza odejmowaniem wody w śledzeniu rezerwy) i nieblokowane w żadnym stanie, także przy
   poziomie alarmowym; działają tylko w Service Mode, przejście do Auto je przerywa. Service Mode
   włączony ręcznie wraca do Auto po 180 min (serwis akwarium / podmiana wody), po starcie po 15 min.
   **Do zrobienia po wdrożeniu `home_display-iot`:** usunąć dolewkę ręczną z GUI WWW — ma być
   działaniem tylko na miejscu (panel), dla spójności logiki bezpieczeństwa akwarium.
2. Ukryte dno 10 % zostaje pod poziomem alarmowym, czy poziom alarmowy je zastępuje?
   Odp. (2026-10-28): zastępuje — procenty od pojemności do znacznika; domyślnie rezerwa 30 %,
   alarm 15 %.
3. Rezerwa: buzzer WARNING czy tylko GUI / panel?
   Odp. (2026-10-28): buzzer tylko przy błędach krytycznych; rezerwa — GUI / panel.
4. Pompa niekalibrowana: blokada auto czy tylko status?
5. Brak odczytu od startu: auto zablokowane czy N porcji?
   Odp. (2026-10-28): ostatni odczyt i pobrana od niego woda w FRAM — restart kontynuuje od nich.
   Bez niczego w FRAM: auto zablokowane do pierwszego odczytu, po 60 s bez odczytu alarm krytyczny.
   Szacunek bez odczytu: połowa zapasu nad alarmem minus dawki; „Reservoir refilled” przy
   niedziałającym czujniku; poziom alarmowy = alarm krytyczny z buzzerem; brak odczytu 30 min —
   żółty status. Szczegóły: `RESERVE_LEVEL_SENSOR_VL53L4CD.md` §7a.
6. Watchdog braku dolewki: k i T_min (propozycja 3 × typowy interwał, min. 12 h).
   Odp. (2026-10-28): stałe 24 h bez dolewki auto (bez wyliczania z EMA) → alarm krytyczny z
   buzzerem, bez blokady. Uwaga: domyślne 20 ml/h to tylko seed EMA; faktyczne zużycie wg
   użytkownika 2–4 l/dobę — domyślny `daily_limit_ml` 2000 ml jest za niski, sprawdzić ustawienie.

## 7. Raport zwrotny do `home_display-iot`

Po zakończeniu wkleić w sesji panelu (lub dopisać do `docs/nodes/dolewka.md`):

- commit(y) i wersja firmware;
- zrealizowane / odłożone punkty z sekcji 2 (numery);
- odpowiedzi na pytania z sekcji 6;
- zmiany parametrów dla listy punktów: nowe `reserve_pct`, `alarm_pct` (zakres, krok), strefy
  `reserve_zone`, usunięte pola (`reserve_float`), nowe / zmienione endpointy;
- zmiany alarmów: `LOW_RESERVOIR` (teraz ToF + przyczyna), `LEVEL_SENSOR_FAULT` (priorytet),
  `NO_TOPOFF`, persystencja zatrzasków, `alarm_mute`;
- wnioski z diagnostyki padającego czujnika (czy W4 się potwierdziło).
