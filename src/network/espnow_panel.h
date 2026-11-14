#pragma once
// ESP-NOW link to the touch panel (home_display-iot). Stage D1: status broadcast only —
// every STATUS_INTERVAL_MS and right after a relevant change. Commands come in stage D4.

#include <stdint.h>

void initEspNowPanel();     // after initWiFi(): needs the Wi-Fi password (key) and STA mode
void updateEspNowPanel();   // loop() context

struct EspNowPanelStats {
    bool     enabled;
    uint32_t txOk;
    uint32_t txFail;
};
EspNowPanelStats getEspNowPanelStats();
