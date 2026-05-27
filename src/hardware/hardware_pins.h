#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

  // ============================================================
  // Waveshare ESP32-S3-Pico — Pin Mapping
  // ============================================================
  //
  //  Top edge pads: GND ─ USB_D- ─ USB_D+ ─ GPIO 0 (BOOT, strapping) ─ GPIO 3 (strapping) ─ GPIO 21 (wolny)
  //
  //                                                      ┌─────────────────────┐
  //                                                      │       USB-C         │
  //                           ATO_PUMP_RELAY GPIO 11 ────┤                     ├──── VBUS (5V)
  //                        MIXING_PUMP_RELAY GPIO 12 ────┤                     ├──── VSYS (5V)
  //                                              GND ────┤                     ├──── GND
  //                       AVAILABLE_WATER_SENSOR  13 ────┤                     ├──── EN   (3V3_EN)
  //                                    RESET GPIO 14 ────┤                     ├──── 3V3  (OUT)
  //                             WATER_SENSOR GPIO 15 ────┤                     ├──── GPIO 10  
  //                              STATUS LED  GPIO 16 ────┤                     ├──── GPIO  9  
  //                                              GND ────┤                     ├──── GND
  //                                  RESERVE GPIO 17 ────┤                     ├──── GPIO  8  [RGB LED — nie używać!]
  //                  PERYSTALTIC_PUMP_DRIVER GPIO 18 ────┤                     ├──── GPIO  7  I2C_SCL_PIN
  //                                          GPIO 33 ────┤                     ├──── RUN  (hardware reset)
  //                                          GPIO 34 ────┤                     ├──── GPIO  6  I2C_SDA_PIN
  //                                              GND ────┤                     ├──── GND
  //                                          GPIO 35 ────┤                     ├──── GPIO  5  
  //                                          GPIO 36 ────┤                     ├──── GPIO  4   FPLAYER_TX_PIN (UART TX → DFPlayer RX)D
  //                                          GPIO 37 ────┤                     ├──── GPIO  2 
  //                                          GPIO 38 ────┤                     ├──── GPIO  1 
  //                                              GND ────┤                     ├──── GND
  //                            JTAG (MTCK)   GPIO 39 ────┤                     ├──── GPIO 41  JTAG (MTDI)
  //                            JTAG (MTDO)   GPIO 40 ────┤                     ├──── GPIO 42  JTAG (MTMS)
  //                                                      └─────────────────────┘
  //
  // ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 11  // Pompa top-off (LOW = ON, active-LOW relay) [D2]
#define WATER_SENSOR_PIN 15  // Czujnik pływakowy — para równoległa (INPUT_PULLUP, active LOW)
#define AVAILABLE_WATER_SENSOR_PIN 13   // Czujnik pływakowy  (INPUT_PULLUP, active LOW)

// ============== KALKWASSER SYSTEM (2 + 1 kanał) ==============
#define PERYSTALTIC_PUMP_DRIVER_PIN 18  // Pompa dozująca A (HIGH = ON)
#define MIXING_PUMP_RELAY_PIN 12  // Pompa miksująca (LOW = ON, active-LOW relay) [D1]

// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7

// ============== SYSTEM ==============
#define RESET_PIN 14  // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)
#define STATUS_LED 16  // dioda stanu systemu

// ============== DFPLAYER PRO (Fermion DFR0768) — UART1 TX-only ==============
// GPIO20 (UART0 RX) nie może być użyty jako UART1 RX — periman UART0 blokuje przejęcie.
// df.begin() timeoutouje bez ACK. Tryb TX-only: ESP32 wysyła komendy, nie odbiera statusu.
// audio_player.cpp: initialized=true bez begin(), repeat oparty na timerze (bez isPlaying()).
#define DFPLAYER_TX_PIN 4   // ESP32 TX → DFPlayer RX

#endif // HARDWARE_PINS_H
