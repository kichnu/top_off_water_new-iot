#ifndef HW_HEALTH_H
#define HW_HEALTH_H

#include <Arduino.h>

// Critical hardware blocks (FRAM, DS3231 RTC). A failure at boot or during
// operation raises a latched critical alarm (buzzer, GUI, /api/status,
// /api/health) that stays until the block works again and the user resets
// (Cycle Reset). The latch survives software restarts (midnight, OTA).
// The ATO keeps running meanwhile: settings and counters stay in RAM and the
// daily limit still applies.

enum HwFaultBit : uint8_t {
    HW_FAULT_FRAM = 0x01,
    HW_FAULT_RTC  = 0x02,
};

void initHwHealth();      // end of setup(), after the FRAM and RTC init
void updateHwHealth();    // every loop() — periodic check
void noteFramIoError();   // failed FRAM transfer (any task) — check soon
void noteRtcIoError();    // failed DS3231 read — check soon

bool     isHwFaultAlarm();       // latched — buzzer, GUI
uint8_t  getHwFaultActive();     // HwFaultBit mask, failing now
uint8_t  getHwFaultLatched();    // HwFaultBit mask, latched
uint32_t getHwFaultSinceTs();    // unix time of the first fault, 0 = unknown
String   getHwFaultText();
bool     ackHwFaults();          // Cycle Reset: clears what works again; true if anything was latched

// FRAM missing at boot answers again — restart to re-init it (limited count)
bool isHwRecoveryRestartWanted();
void noteHwRecoveryRestart();

#endif // HW_HEALTH_H
