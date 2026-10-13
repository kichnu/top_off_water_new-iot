#include "reserve_controller.h"
#include "fram_controller.h"
#include "../core/logging.h"

static uint32_t g_reserveConfigMl  = RESERVE_ML_DEFAULT;
static uint32_t g_reserveMl        = RESERVE_ML_DEFAULT;

void initReserveController() {
    uint32_t cfg = 0, cur = 0;
    if (loadReserveFromFRAM(cfg, cur)) {
        g_reserveConfigMl = cfg;
        g_reserveMl       = cur;
    } else {
        g_reserveConfigMl = RESERVE_ML_DEFAULT;
        g_reserveMl       = RESERVE_ML_DEFAULT;
        saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    }

    LOG_INFO("Reserve controller init: config=%lu ml, current=%lu ml",
             g_reserveConfigMl, g_reserveMl);
}

uint32_t getReserveMl()       { return g_reserveMl; }
uint32_t getReserveConfigMl() { return g_reserveConfigMl; }
bool     isReserveEmpty()     { return g_reserveMl == 0; }

bool isReserveLow() {
    return g_reserveMl > 0 && g_reserveMl * 100u <= g_reserveConfigMl * RESERVE_WARN_PCT;
}

void subtractReserveMl(uint16_t ml) {
    if (ml == 0) return;
    if (ml >= g_reserveMl) {
        g_reserveMl = 0;
    } else {
        g_reserveMl -= ml;
    }
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve: -%d ml → %lu ml remaining", ml, g_reserveMl);
}

void setReserveConfigMl(uint32_t ml) {
    if (ml < RESERVE_ML_MIN) ml = RESERVE_ML_MIN;
    if (ml > RESERVE_ML_MAX) ml = RESERVE_ML_MAX;
    g_reserveConfigMl = ml;
    g_reserveMl       = ml;
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve config set: %lu ml", ml);
}

void clearReserveAlarm() {
    g_reserveMl = g_reserveConfigMl;
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve refilled — reset to %lu ml", g_reserveMl);
}
