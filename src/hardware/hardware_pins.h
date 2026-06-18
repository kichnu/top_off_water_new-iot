#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

  // ============================================================
  // SEEED STUDIO XIAO ESP32-C3 — Pin Mapping
  // ============================================================
  //
  //  Dostępne GPIO: 2,3,4,5,6,7,8,9,10,20,21 (tylko 11 pinów)
  //
  //  Piny specjalne (strap) — uwagi:
  //    GPIO 2  — strap VDD_SPI; INPUT_PULLUP=HIGH=OK (3.3V flash)
  //    GPIO 5  — strap MTDI;    INPUT_PULLUP=HIGH=OK → używamy jako RESET_PIN
  //    GPIO 8  — strap;         wolny, INPUT_PULLUP=HIGH=OK
  //    GPIO 9  — BOOT strap: LOW podczas boot → Download mode → NIE podłączać nic co może trzymać LOW
  //    GPIO 20 — UART0 RX;  wolny (USB-CDC na C3 nie używa UART0)
  //    GPIO 21 — UART0 TX;  wolny (USB-CDC na C3 nie używa UART0)
  //
  //  I2C → GPIO 20 (SDA) + GPIO 21 (SCL): brak strap, brak boot-risk, najczystszy wybór
  //
  //                                                      ┌─────────────────────┐
  //                              (strap VDD_SPI, wolny)  ├ GPIO  2             ├──── VBUS (5V)
  //                        WATER_SENSOR_PIN  GPIO  3 ────┤ ADC1_CH3            ├──── GND
  //                             (strap MTMS, wolny)      ├ GPIO  4             ├──── 3V3  (OUT)
  //                               RESET_PIN  GPIO  5 ────┤ strap MTDI          ├──── GPIO 10  ATO_PUMP_RELAY_PIN
  //            AVAILABLE_WATER_SENSOR_PIN    GPIO  6 ────┤ JTAG MTCK           ├──── GPIO  9  (strap BOOT, wolny!)
  //                             BUZZER_PIN   GPIO  7 ────┤ JTAG MTDO           ├──── GPIO  8  (strap, wolny)
  //                          I2C_SCL_PIN    GPIO 21 ────┤ UART TX             ├──── GPIO 20  I2C_SDA_PIN
  //                                                      └─────────────────────┘
  //
  // ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 10        // Pompa top-off (LOW = ON, active-LOW relay)
#define WATER_SENSOR_PIN 3           // Czujnik pływakowy (INPUT_PULLUP, active LOW)
#define AVAILABLE_WATER_SENSOR_PIN 6 // Czujnik zbiornika wody (INPUT_PULLUP, active LOW)

// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

// ============== SYSTEM ==============
#define RESET_PIN 5    // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)
#define BUZZER_PIN 7   // Buzzer

#endif // HARDWARE_PINS_H
