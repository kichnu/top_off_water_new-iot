#pragma once
// Node commands — one gate to the state for the web handlers and the panel (ESP-NOW TO_CMD).
// Results use the protocol codes (topoff::AckResult); the web handlers map them to HTTP answers.

#include <stdint.h>

#include "../network/topoff_proto.h"

// Auto (true) / Service Mode (false), idempotent; Service Mode from the user returns to Auto after
// SERVICE_MODE_MANUAL_MS. *changed: the mode was switched.
topoff::AckResult cmdSetMode(bool autoMode, bool* changed);

// Manual portions (Service Mode only, refused with LOW_RESERVOIR latched). *accepted: portions queued.
topoff::AckResult cmdPortions(uint8_t n, uint8_t* accepted);

// Finish the running manual portion, clear the queue (always OK).
topoff::AckResult cmdPumpStop();

// Error / latch reset (= reset button held 3 s); RESET_BLOCKED while a cycle is being logged.
topoff::AckResult cmdErrorReset();

// Daily limit DAILY_LIMIT_MIN..MAX and >= 2 × dose. *limit: the limit now, or the nearest allowed
// value with OUT_OF_RANGE.
topoff::AckResult cmdSetDailyLimit(uint16_t ml, uint16_t* limit);

const char* ackResultString(uint8_t result);
