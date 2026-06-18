// ============================================================
//  SEEED XIAO ESP32-C3 — Hardware Diagnostic Test
//  Jeden plik, zero zależności od projektu.
//
//  Testuje: Serial, GPIO out, GPIO in, I2C scan,
//           DS3231 RTC, FRAM R/W, WiFi radio, live sensor loop.
//
//  Flash: pio run -e test_c3 -t upload
//  Monitor: pio device monitor
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "driver/gpio.h"

// ---- Piny (zgodne z hardware_pins.h dla C3) ----
#define PIN_RELAY       10   // ATO pump relay   (active LOW)
#define PIN_WATER        3   // Water sensor      (INPUT_PULLUP, active LOW)
#define PIN_RESERVE      6   // Reserve sensor    (INPUT_PULLUP, active LOW)
#define PIN_RESET        5   // Provisioning btn  (INPUT_PULLUP, active LOW)
#define PIN_BUZZER       7   // Buzzer            (active LOW)
#define PIN_I2C_SDA     20
#define PIN_I2C_SCL     21

// ---- I2C addresses ----
#define ADDR_DS3231   0x68
#define ADDR_FRAM     0x50

// ---- Wyniki ----
static int g_pass = 0, g_fail = 0;

static void tPASS(const char* name) {
    Serial.printf("  [tPASS] %s\n", name);
    g_pass++;
}
static void tFAIL(const char* name, const char* reason = "") {
    Serial.printf("  [tFAIL] %s  (%s)\n", name, reason);
    g_fail++;
}
static void HEADER(const char* title) {
    Serial.printf("\n──── %s ────\n", title);
}

// ============================================================
//  Buzzer helpers (active LOW)
// ============================================================
static void beep(uint32_t ms) {
    digitalWrite(PIN_BUZZER, LOW);
    delay(ms);
    digitalWrite(PIN_BUZZER, HIGH);
}
static void buzzerOK() {
    beep(100); delay(150); beep(500);
}
static void buzzerFail() {
    for (int i = 0; i < 4; i++) { beep(200); delay(80); }
}

// ============================================================
//  1. GPIO output — buzzer + relay
// ============================================================
static void testGPIOOutput() {
    HEADER("1. GPIO output — buzzer + relay");

    // Buzzer
    gpio_reset_pin((gpio_num_t)PIN_BUZZER);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, HIGH);
    beep(80); delay(80); beep(80);
    tPASS("BUZZER GPIO7 — słyszysz 2 krótkie piknięcia?");

    // Relay — 100ms impuls (krótki klik)
    gpio_reset_pin((gpio_num_t)PIN_RELAY);
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, HIGH);   // off
    delay(200);
    digitalWrite(PIN_RELAY, LOW);    // on — klik!
    delay(100);
    digitalWrite(PIN_RELAY, HIGH);   // off
    tPASS("RELAY GPIO10 — słyszysz klik przekaźnika?");
}

// ============================================================
//  2. GPIO input — czujniki + przycisk
// ============================================================
static void testGPIOInput() {
    HEADER("2. GPIO input — czujniki + przycisk RESET");

    struct { const char* name; int pin; } inputs[] = {
        { "WATER_SENSOR   GPIO3",  PIN_WATER   },
        { "RESERVE_SENSOR GPIO6",  PIN_RESERVE },
        { "RESET_BUTTON   GPIO5",  PIN_RESET   },
    };

    for (auto& inp : inputs) {
        gpio_reset_pin((gpio_num_t)inp.pin);
        pinMode(inp.pin, INPUT_PULLUP);
        delay(5);
        int v = digitalRead(inp.pin);
        Serial.printf("  %s  →  %s  %s\n",
            inp.name,
            v == HIGH ? "HIGH" : "LOW ",
            v == HIGH ? "(OK — czujnik nieaktywny)" : "(LOW — aktywny lub problem)");
    }
    tPASS("GPIO input — zweryfikuj odczyty powyżej");
}

// ============================================================
//  3. I2C scan
// ============================================================
static bool g_ds3231_ok = false;
static bool g_fram_ok   = false;

static void testI2CScan() {
    HEADER("3. I2C scan  SDA=GPIO20  SCL=GPIO21");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    delay(50);

    int found = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char* lbl = "";
            if (addr == ADDR_DS3231) { lbl = " ← DS3231 RTC";  g_ds3231_ok = true; }
            if (addr == ADDR_FRAM)   { lbl = " ← FRAM MB85RC"; g_fram_ok   = true; }
            Serial.printf("  0x%02X%s\n", addr, lbl);
            found++;
        }
    }

    if (found == 0) {
        tFAIL("I2C scan", "brak urządzeń — sprawdź okablowanie i pull-upy 4.7k");
    } else {
        Serial.printf("  Znaleziono: %d urządzenie(a)\n", found);
        tPASS("I2C scan");
        if (!g_ds3231_ok) tFAIL("DS3231 brak", "oczekiwany na 0x68");
        if (!g_fram_ok)   tFAIL("FRAM brak",   "oczekiwany na 0x50");
    }
}

// ============================================================
//  4. DS3231 — odczyt czasu (surowe rejestry, bez biblioteki)
// ============================================================
static void testDS3231() {
    HEADER("4. DS3231 RTC");

    if (!g_ds3231_ok) { tFAIL("DS3231", "pominięty — brak w I2C scan"); return; }

    // Ustaw wskaźnik na rejestr 0x00 (sekundy)
    Wire.beginTransmission(ADDR_DS3231);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) { tFAIL("DS3231 write ptr", "brak ACK"); return; }

    Wire.requestFrom((uint8_t)ADDR_DS3231, (uint8_t)7);
    if (Wire.available() < 7) { tFAIL("DS3231 read", "za mało bajtów"); return; }

    auto bcd = [](byte b) -> int { return (b >> 4) * 10 + (b & 0x0F); };
    int sec = bcd(Wire.read() & 0x7F);
    int min = bcd(Wire.read() & 0x7F);
    int hr  = bcd(Wire.read() & 0x3F);
    Wire.read();                           // dzień tygodnia
    int day = bcd(Wire.read());
    int mon = bcd(Wire.read() & 0x1F);
    int yr  = bcd(Wire.read());

    Serial.printf("  Czas RTC: 20%02d-%02d-%02d  %02d:%02d:%02d\n",
                  yr, mon, day, hr, min, sec);

    // Odczyt rejestru stanu (0x0F) — bit OSF (bit7) = 1 jeśli bateria padła
    Wire.beginTransmission(ADDR_DS3231);
    Wire.write(0x0F);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)ADDR_DS3231, (uint8_t)1);
    byte status = Wire.available() ? Wire.read() : 0xFF;
    bool osf = (status & 0x80);
    Serial.printf("  Status reg: 0x%02X  OSF=%d %s\n",
                  status, osf ? 1 : 0,
                  osf ? "(bateria wyładowana — czas zresetowany)" : "(bateria OK)");

    if (yr < 24 || yr > 35) {
        tFAIL("DS3231 czas", "rok poza rozsądnym zakresem");
    } else if (osf) {
        tFAIL("DS3231 bateria", "OSF=1 — wymień baterię CR2032");
    } else {
        tPASS("DS3231 RTC — czas i bateria OK");
    }
}

// ============================================================
//  5. FRAM — zapis i odczyt 8 bajtów pod adresem 0x7FF0
// ============================================================
static void testFRAM() {
    HEADER("5. FRAM MB85RC256V");

    if (!g_fram_ok) { tFAIL("FRAM", "pominięty — brak w I2C scan"); return; }

    const uint16_t ADDR  = 0x7FF0;
    const uint8_t  WR[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
    const uint8_t  LEN   = sizeof(WR);

    // Zapis
    Wire.beginTransmission(ADDR_FRAM);
    Wire.write(ADDR >> 8);
    Wire.write(ADDR & 0xFF);
    for (uint8_t b : WR) Wire.write(b);
    if (Wire.endTransmission() != 0) { tFAIL("FRAM write", "brak ACK"); return; }
    delay(5);

    // Odczyt
    Wire.beginTransmission(ADDR_FRAM);
    Wire.write(ADDR >> 8);
    Wire.write(ADDR & 0xFF);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)ADDR_FRAM, LEN);

    bool ok = true;
    Serial.print("  Odczyt: ");
    for (uint8_t i = 0; i < LEN; i++) {
        if (!Wire.available()) { ok = false; break; }
        uint8_t b = Wire.read();
        Serial.printf("0x%02X ", b);
        if (b != WR[i]) ok = false;
    }
    Serial.println();

    if (ok) tPASS("FRAM R/W — 8 bajtów zapis/odczyt zgodny");
    else    tFAIL("FRAM dane", "rozbieżność — uszkodzony chip lub złe połączenie");
}

// ============================================================
//  6. WiFi — pasywny scan sieci
// ============================================================
static void testWiFi() {
    HEADER("6. WiFi radio — scan sieci");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("  Skanowanie...");
    int n = WiFi.scanNetworks(false, true);  // show hidden too

    if (n < 0) {
        tFAIL("WiFi scan", "błąd skanowania");
    } else {
        for (int i = 0; i < min(n, 5); i++) {
            Serial.printf("  [%d] RSSI=%d  %s\n", i+1, WiFi.RSSI(i), WiFi.SSID(i).c_str());
        }
        if (n > 5) Serial.printf("  ... i %d więcej\n", n - 5);
        Serial.printf("  Łącznie: %d sieci\n", n);
        tPASS("WiFi radio OK");
    }
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
}

// ============================================================
//  7. Chip info
// ============================================================
static void testChipInfo() {
    HEADER("7. Chip info");
    Serial.printf("  Model:      %s\n", ESP.getChipModel());
    Serial.printf("  Revision:   %d\n", ESP.getChipRevision());
    Serial.printf("  CPU freq:   %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  Flash size: %d KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("  Free heap:  %d B\n", ESP.getFreeHeap());
    Serial.printf("  SDK:        %s\n", ESP.getSdkVersion());
    tPASS("Chip info odczytany");
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);  // czas na podłączenie monitora

    Serial.println();
    Serial.println("════════════════════════════════════════════════════");
    Serial.println("   SEEED XIAO ESP32-C3  —  Hardware Diagnostic");
    Serial.println("════════════════════════════════════════════════════");

    testGPIOOutput();
    testGPIOInput();
    testI2CScan();
    testDS3231();
    testFRAM();
    testWiFi();
    testChipInfo();

    // ---- Podsumowanie ----
    Serial.println();
    Serial.println("════════════════════════════════════════════════════");
    Serial.printf("  WYNIK: %d tPASS  /  %d tFAIL\n", g_pass, g_fail);
    Serial.println("════════════════════════════════════════════════════");

    if (g_fail == 0) {
        Serial.println("  ✅ Wszystkie testy przeszły!");
        buzzerOK();
    } else {
        Serial.println("  ❌ Są błędy — sprawdź tFAIL powyżej");
        buzzerFail();
    }

    Serial.println();
    Serial.println("  Live sensor readout co 2s (CTRL+C żeby wyjść):");
    Serial.println("────────────────────────────────────────────────────");
}

// ============================================================
//  LOOP — live odczyt czujników
// ============================================================
void loop() {
    static uint32_t last = 0;
    if (millis() - last < 2000) return;
    last = millis();

    int water   = digitalRead(PIN_WATER);
    int reserve = digitalRead(PIN_RESERVE);
    int reset   = digitalRead(PIN_RESET);

    Serial.printf("[%6lus]  WATER=%-12s  RESERVE=%-12s  RESET=%s\n",
        millis() / 1000,
        water   == LOW ? "LOW (aktywny)" : "HIGH",
        reserve == LOW ? "LOW (aktywny)" : "HIGH",
        reset   == LOW ? "WCIŚNIĘTY"    : "wolny");
}
