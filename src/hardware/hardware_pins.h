#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

  // ============================================================
  // SEEED STUDIO ESP32 C6 — Pin Mapping
  // ============================================================
  //
  //  Top edge pads: GND ─ USB_D- ─ USB_D+ ─ GPIO 0 (BOOT, strapping) ─ GPIO 3 (strapping) ─ GPIO 21 (wolny)
  //
  //                                                      ┌─────────────────────┐
  //                          WATER_SENSOR_PIN GPIO 0 ────┤                     ├──── VBUS (5V)
  //                AVAILABLE_WATER_SENSOR_PIN GPIO 1 ────┤                     ├──── GND
  //                                 RESET_PIN GPIO 2 ────┤                     ├──── 3V3  (OUT)
  //                       ATO_PUMP_RELAY_PIN GPIO 21 ────┤                     ├──── GPIO 18  BUZZER_PIN
  //                              I2C_SDA_PIN GPIO 22 ────┤                     ├──── GPIO 20  STATUS_LED
  //                              I2C_SCL_PIN GPIO 23 ────┤                     ├──── GPIO 19                                      
  //                                          GPIO 16 ────┤                     ├──── GPIO 17 
  //                                                      └─────────────────────┘
  //
  // ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 21         // Pompa top-off (LOW = ON, active-LOW relay)
#define WATER_SENSOR_PIN 0            // Czujnik pływakowy (INPUT_PULLUP, active LOW)
#define AVAILABLE_WATER_SENSOR_PIN 1  // Czujnik zbiornika wody (INPUT_PULLUP, active LOW)

// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN 22
#define I2C_SCL_PIN 23

// ============== SYSTEM ==============
#define RESET_PIN 2    // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)
#define STATUS_LED 20  // dioda stanu systemu
#define BUZZER_PIN 18  // Buzzer (HIGH = ON)

#endif // HARDWARE_PINS_H
