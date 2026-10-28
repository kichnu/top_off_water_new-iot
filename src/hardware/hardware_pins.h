#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

  // ============================================================
  // SEEED STUDIO XIAO ESP32-C3 — Pin Mapping
  // ============================================================
  //
  //  Available GPIO: 2,3,4,5,6,7,8,9,10,20,21 (only 11 pins)
  //
  //  Special (strap) pins — notes:
  //    GPIO 2  — strap VDD_SPI; INPUT_PULLUP=HIGH=OK (3.3V flash)
  //    GPIO 5  — strap MTDI;    INPUT_PULLUP=HIGH=OK
  //    GPIO 8  — strap;         matters only in download mode (GPIO 9 LOW) → free, a switch to GND OK
  //    GPIO 9  — BOOT strap: LOW during boot → download mode → connect nothing that can hold it LOW
  //    GPIO 20 — UART0 RX;  input during boot (USB-CDC on the C3 does not use UART0) → VL53L4CD XSHUT
//              (module pull-up keeps the sensor on while the pin is an input)
  //    GPIO 21 — UART0 TX;  free — the ROM drives it HIGH with the boot log, so no switch to GND here
  //
  //                                                      ┌─────────────────────┐
  //                         (strap VDD_SPI, free)  GPIO 2├                     ├──── VBUS (5V)
  //                        WATER_SENSOR_PIN  GPIO  3 ────┤                     ├──── GND
  //                              RESET_PIN   GPIO  4 ----|                     |──── 3V3  (OUT)
  //                             I2C_SDA_PIN  GPIO  5 ────┤                     ├──── GPIO 10  ATO_PUMP_RELAY_PIN
  //                             I2C_SCL_PIN  GPIO  6 ────┤                     ├──── GPIO  9  (strap BOOT, free!)
  //                             BUZZER_PIN   GPIO  7 ────┤                     ├──── GPIO  8  FREE
  //                                    FREE  GPIO 21 ────┤                     ├──── GPIO 20  LEVEL_XSHUT_PIN
  //                                                      └─────────────────────┘
  //
  // ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 10        // Top-off pump relay (active HIGH)
#define PUMP_RELAY_ON  HIGH
#define PUMP_RELAY_OFF LOW
#define WATER_SENSOR_PIN 3           // ATO float sensor (INPUT_PULLUP, active LOW)


// ============== I2C (DS3231 RTC + FRAM MB85RC256V + VL53L4CD) ==============
#define I2C_SDA_PIN 5
#define I2C_SCL_PIN 6
#define LEVEL_XSHUT_PIN 20   // VL53L4CD XSHUT (active LOW, open-drain only: the module pulls it up to its 2.8 V VDD)

// ============== SYSTEM ==============
#define RESET_PIN 4    // Reset / provisioning button (INPUT_PULLUP, active LOW)
#define BUZZER_PIN 7   // Buzzer (active HIGH)
#define BUZZER_LEVEL_ON  HIGH
#define BUZZER_LEVEL_OFF LOW

#endif // HARDWARE_PINS_H
