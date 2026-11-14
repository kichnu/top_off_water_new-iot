#pragma once
// Node status and alarm summary shared by /api/status and the panel status frame (ESP-NOW).
// Rule (home_display-iot/docs/nodes/dolewka.md, "Reguła node_status"), first match wins:
// Fault = HW_FAULT; Alarm = other P1 alarm; Service = Service Mode; Warning = P3 alarm or
// uncalibrated pump; OK otherwise.

#include <stdint.h>

uint16_t getAlarmBits();    // topoff::AL_* bits
uint8_t  getNodeStatus();   // topoff::NodeStatus
uint8_t  getErrorCode();    // ErrorCode, ERROR_NONE outside STATE_ERROR
const char* nodeStatusString(uint8_t status);
