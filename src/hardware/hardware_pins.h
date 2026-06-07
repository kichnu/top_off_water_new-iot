#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

  // ============================================================
  // SEEED STUDIO ESP32 C6 — Pin Mapping
  // ============================================================
  //
  //  Top edge pads: GND ─ USB_D- ─ USB_D+ ─ GPIO 0 (BOOT strap) ─ GPIO 3 (strap) ─ GPIO 21
  //
  //  Piny do unikania dla czujników:
  //    GPIO 0  — BOOT strap (LOW podczas boot = tryb Download)
  //    GPIO 2  — JTAG MTDO (wyjście z chipa) + połączenie PCB Seeed → stały LOW ~2.62V
  //    GPIO 21 — podłączony do VBAT na PCB Seeed (3.8V) → nie nadaje się jako wejście logiczne 3.3V
  //    GPIO 16 — UART0 TX  (chip aktywnie wystawia dane → fałszywe odczyty czujnika)
  //    GPIO 17 — UART0 RX  (input, mniej groźny, ale może zakłócać UART)
  //
  //                                                      ┌─────────────────────┐
  //                                           GPIO 0 ────┤ (BOOT strap, wolny) ├──── VBUS (5V)
  //                   WATER_SENSOR_PIN (nowy) GPIO 1 ────┤ (JTAG MTDI, input)  ├──── GND
  //                                 (UNIKAJ!) GPIO 2 ────┤ JTAG MTDO+PCB=2.62V ├──── 3V3  (OUT)
  //                      (UNIKAJ!) RESET_PIN GPIO 21 ────┤ PCB=VBAT=3.8V       ├──── GPIO 18  ATO_PUMP_RELAY_PIN
  //               AVAILABLE_WATER_SENSOR_PIN GPIO 22 ────┤                     ├──── GPIO 20  I2C_SCL_PIN
  //                               BUZZER_PIN GPIO 23 ────┤                     ├──── GPIO 19  I2C_SDA_PIN
  //                                          GPIO 17 ────┤  (UART RX, OK)      ├──── GPIO 16 
  //                                                      └─────────────────────┘
  //
  // ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 18         // Pompa top-off (LOW = ON, active-LOW relay)
#define WATER_SENSOR_PIN 1           // Czujnik pływakowy (INPUT_PULLUP, active LOW) — GPIO 1 (JTAG MTDI, input = bezpieczny)
#define AVAILABLE_WATER_SENSOR_PIN 22 // Czujnik zbiornika wody (INPUT_PULLUP, active LOW)

// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN 19
#define I2C_SCL_PIN 20

// ============== SYSTEM ==============
#define RESET_PIN 21    // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)
#define BUZZER_PIN 23  // Buzzer (LOW = ON, active LOW)

#endif // HARDWARE_PINS_H
