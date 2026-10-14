# Wyświetlacz lokalny: OLED 0,91" 128×32 (SSD1306, I2C)

Ustalenia z 2026-10-14. Stan: decyzja sprzętowa podjęta, implementacja nie rozpoczęta.

## 1. Cel

Lokalna alternatywa dla GUI (dashboardu web) — obsługa lokalna jest ważna dla wygody
użytkownika. Ekran pokazuje, co dolewka aktualnie robi — od `IDLE` po błędy — i pozwala
lokalnie obsłużyć alarmy (sekcja 6). Później (po ustaleniu optymalizacji komunikacji, sekcja 5) także
informacje z pomiaru lustra wody w rezerwie (VL53L4CD).

Zasady:
- tylko aktualny status i potrzebne informacje — bez zbędnych animacji;
- ekran nie może zapychać `loop()` (sekcja 5 — do ustalenia przed implementacją).

## 2. Sprzęt

Waveshare 0.91inch OLED Module (SKU 14657),
https://www.waveshare.com/0.91inch-oled-module.htm,
schemat: https://files.waveshare.com/upload/2/2a/0.91inch_OLED_Module_Schematic.pdf

| Parametr | Wartość |
|---|---|
| Sterownik | SSD1306 (panel M96-16), 128×32, mono biały |
| Interfejs | tylko I2C, max 400 kHz (nota SSD1306: t_cycle ≥ 2,5 µs) |
| Adres | 0x3C (SA0 niewyprowadzony; potwierdzić skanerem I2C) |
| Zasilanie | VCC 3,3 V lub 5 V → LDO XC6206P332 → 3V3; **podłączamy z 3,3 V** |
| Pull-upy | 4,7 kΩ SDA/SCL do 3V3 (za LDO) — bezpieczne dla ESP32-C3 |
| Reset | RC + dioda na płytce, nie zajmuje GPIO |
| Złącze | PH2.0 4-pin (kabel w zestawie) + goldpin; kolejność pinów sprawdzić na nadruku |
| Obszar aktywny | 22,4 × 5,6 mm, piksel ~0,17 mm |
| Bufor ramki | 512 B (128 × 32 / 8) |

Magistrala I2C po dołączeniu (SDA GPIO 5, SCL GPIO 6):

| Urządzenie | Adres |
|---|---|
| VL53L4CD | 0x29 |
| OLED SSD1306 | 0x3C |
| FRAM MB85RC256V | 0x50 |
| DS3231 (+ EEPROM modułu, jeśli jest) | 0x68 (0x57) |

Do sprawdzenia przy montażu: suma równoległych pull-upów wszystkich modułów. Kolejne
4,7 kΩ może dać zastępczo ~1–1,5 kΩ (≈ 2,5–3 mA w stanie niskim — granica normy I2C).
W razie potrzeby zdjąć R1/R2 z OLED-a albo pull-upy z modułu RTC.

## 3. Treść ekranu

Odległość obsługi: ok. 30–50 cm. Forma prezentacji: ikony, 1 linia tekstu albo 2 linie
tekstu — zależnie od informacji. Font 16 px (~2,8 mm) → 2 linie, czytelne z tej
odległości; większy font / ikona 32 px → 1 linia (komunikaty błędów, alarmy). Font 8 px
(~1,4 mm, 4 linie) za mały na 30–50 cm — nie używamy do treści głównej. Ikony (WiFi, alarm itp.) rysuje firmware — SSD1306 nie ma fontów ani
ikon, to czysta mapa bitów (np. ikony-fonty U8g2 `open_iconic`, własne bitmapy XBM).

Status urządzenia do pokazania (wstępnie):
- tryb: Service Mode (z czasem do auto-włączenia) / Auto;
- stan algorytmu: `IDLE`, `DEBOUNCING`, `PUMPING`, `LOGGING`;
- błędy (`STATE_ERROR`): pusta rezerwa, limit dobowy, czerwony alarm EMA — komunikat
  na cały ekran;
- ostrzeżenia: niska rezerwa;
- ręczna praca pompy (direct/kalibracja);
- WiFi (ikona).

Później, po sekcji 5: poziom/objętość rezerwy z VL53L4CD, dzienny wolumen / limit,
ostatnia dolewka.

Ryzyko wypalania OLED przy statycznym obrazie 24/7 — do ustalenia (obniżony kontrast,
wygaszanie, przesunięcie o 1–2 px). Nota SSD1306: kontrast 256 kroków (cmd 0x81).

## 4. Biblioteka

Propozycja: U8g2 (obsługuje SSD1306 128×32, fonty, ikony, tryb stron). Narzut flash
~30–50 kB przy kilku fontach; wolne ~550 kB w partycji aplikacji (`min_spiffs`).

## 5. Komunikacja I2C — DO USTALENIA przed implementacją

Fakty:
- magistrala chodzi dziś na 100 kHz (`main.cpp`, `Wire.setClock(100000)`);
- pełna ramka 512 B: ~46 ms przy 100 kHz, ~12 ms przy 400 kHz (blokujące);
- wszystkie urządzenia na magistrali wspierają 400 kHz; SSD1306 więcej nie przyjmie;
- z I2C korzysta też task AsyncTCP (handlery web → FRAM); `Wire` w rdzeniu ESP32 ma
  wewnętrzny mutex, więc transakcje się nie przeplatają, ale mogą się nawzajem blokować.

Opcje do omówienia: podniesienie zegara do 400 kHz, odświeżanie tylko przy zmianie
treści, wysyłanie ramki w częściach (tryb stron U8g2 / tylko zmienione strony),
osobny task FreeRTOS o niskim priorytecie (jak buzzer).

## 6. Przycisk jako nawigacja (ćwiczenie UI)

Przycisk `RESET_PIN` (GPIO 4, INPUT_PULLUP, active LOW) ma dziś dwie funkcje:
1. boot: przytrzymanie 5 s → tryb provisioning (`prov_detector.cpp`);
2. praca: każde naciśnięcie i puszczenie w `STATE_ERROR` → `resetFromError()`
   (+ `clearReserveAlarm()` przy pustej rezerwie) — `WaterAlgorithm::checkResetButton()`.

Plan (najprawdopodobniej):
- provisioning przy starcie (przytrzymanie 5 s) — bez zmian;
- w czasie pracy przycisk służy wyłącznie do obsługi ekranu: nawigacja (np. przewinięcie
  o 1 linię / następny ekran) i kasowanie alarmów przez menu na ekranie:
  zaznacz alarm → potwierdź skasowanie;
- obecne kasowanie błędu „każdym naciśnięciem” w `checkResetButton()` znika — zastępuje
  je świadome potwierdzenie na ekranie (nawigacja nie może przypadkiem skasować alarmu).

Do ustalenia przy implementacji: rozróżnienie zdarzeń krótkie / długie naciśnięcie
(np. krótkie = dalej, długie = wybierz/potwierdź), debouncing, powrót z menu do ekranu
statusu po czasie bezczynności; obsługa przycisku w jednym miejscu, które przekazuje
zdarzenia do ekranu, a ekran wywołuje akcje algorytmu (np. `resetFromError()`).

## 7. Kolejność prac

1. Montaż, skaner I2C (adres, pull-upy), test z przykładem U8g2.
2. Ustalenia z sekcji 5 (komunikacja) i zasady odświeżania.
3. Moduł `hardware/display_controller.*`: ekran statusu (sekcja 3, bez VL53L4CD).
4. Obsługa przycisku: krótkie/długie naciśnięcie, nawigacja, kasowanie alarmów z
   potwierdzeniem; usunięcie kasowania błędu z `checkResetButton()` (sekcja 6).
5. Dane z VL53L4CD i statystyki dobowe.
