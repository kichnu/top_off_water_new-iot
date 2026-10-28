#ifndef RTC_CONTROLLER_H
#define RTC_CONTROLLER_H

#include <Arduino.h>

void initializeRTC();
String getCurrentTimestamp();
bool isRTCWorking();
unsigned long getUnixTimestamp();
// Last getUnixTimestamp() gave a real time (DS3231 read OK). The fallbacks
// (no RTC, failed read, compile-time / frozen internal time) are not valid.
bool isUnixTimeValid();
String getRTCInfo();

String getTimeSourceInfo();
bool isRTCHardware();

bool rtcNeedsSynchronization();
void initInternalTimeFromCompileTime();
bool setRTCFromNTP();

bool isBatteryIssueDetected();

#endif