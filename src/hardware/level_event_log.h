#ifndef LEVEL_EVENT_LOG_H
#define LEVEL_EVENT_LOG_H

#include <Arduino.h>

// Persistent log of level sensor events (boot, FAULT, resets, bus recovery,
// recovery) — diagnostics of sensor faults when no log client is connected.
// FRAM ring buffer at FRAM_ADDR_LEVEL_LOG, mirrored in RAM so reading it from the web
// handler never touches the I2C bus. Read: GET /api/level-events, clear: POST.

#define LEVEL_EVENT_COUNT      48
#define LEVEL_EVENT_TEXT_LEN  120

// Call after FRAM init
void initLevelEventLog();

// Store one event (text is truncated to LEVEL_EVENT_TEXT_LEN - 1 chars)
void addLevelEvent(const char* text);

// Oldest first, one line per event: "YYYY-MM-DD HH:MM:SS (up N s) text"
String getLevelEventsText();

void clearLevelEvents();

#endif // LEVEL_EVENT_LOG_H
