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
// Runtime retry while the DS3231 is not in use or needs a time sync; non-blocking,
// call periodically. true = DS3231 in use with a synced time
bool retryRTC();

bool isBatteryIssueDetected();

#endif