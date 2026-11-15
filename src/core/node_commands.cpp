#include "node_commands.h"

#include "../algorithm/water_algorithm.h"
#include "../config/config.h"
#include "../core/logging.h"
#include "../hardware/pump_calibration.h"
#include "../hardware/pump_controller.h"
#include "../hardware/reserve_controller.h"

extern WaterAlgorithm waterAlgorithm;

using namespace topoff;

AckResult cmdSetMode(bool autoMode, bool* changed) {
    bool isAuto = !isSystemDisabled();
    *changed = autoMode != isAuto;
    if (!*changed) return ACK_OK;   // the same mode again keeps the running Service timer
    if (isPumpCalibrationActive()) return ACK_CALIBRATION;
    setSystemState(autoMode, SERVICE_MODE_MANUAL_MS);
    return ACK_OK;
}

AckResult cmdPortions(uint8_t n, uint8_t* accepted) {
    *accepted = 0;
    if (n == 0 || n > PORTIONS_MAX) return ACK_BAD_CMD;
    if (!isSystemDisabled()) return ACK_NOT_SERVICE;
    if (isPumpCalibrationActive()) return ACK_CALIBRATION;
    if (isPumpActive() && !isDirectPumpMode()) return ACK_PUMP_BUSY;
    if (isReserveEmpty()) return ACK_RESERVOIR_EMPTY;
    *accepted = manualPortionsAdd(n);
    return *accepted == n ? ACK_OK : ACK_QUEUE_FULL;
}

AckResult cmdPumpStop() {
    manualPortionsStop();
    return ACK_OK;
}

AckResult cmdErrorReset() {
    return waterAlgorithm.resetSystem() ? ACK_OK : ACK_RESET_BLOCKED;
}

AckResult cmdSetDailyLimit(uint16_t ml, uint16_t* limit) {
    // At least two doses a day — a smaller limit would stop every cycle
    uint32_t lo = 2u * waterAlgorithm.getConfig().dose_ml;
    if (lo < DAILY_LIMIT_MIN) lo = DAILY_LIMIT_MIN;
    if (ml < lo || ml > DAILY_LIMIT_MAX) {
        *limit = ml < lo ? (uint16_t)lo : DAILY_LIMIT_MAX;
        return ACK_OUT_OF_RANGE;
    }
    if (ml != waterAlgorithm.getConfig().daily_limit_ml) {
        TopOffConfig cfg = waterAlgorithm.getConfig();
        cfg.daily_limit_ml = ml;
        waterAlgorithm.setConfig(cfg);
    }
    *limit = waterAlgorithm.getConfig().daily_limit_ml;
    return ACK_OK;
}

const char* ackResultString(uint8_t result) {
    switch (result) {
        case ACK_OK:              return "OK";
        case ACK_NOT_SERVICE:     return "Service Mode only";
        case ACK_CALIBRATION:     return "Calibration in progress";
        case ACK_PUMP_BUSY:       return "Pump busy";
        case ACK_QUEUE_FULL:      return "Queue full";
        case ACK_RESET_BLOCKED:   return "Reset blocked - logging in progress";
        case ACK_RESERVOIR_EMPTY: return "Reservoir alarm - refill first";
        case ACK_STALE_NONCE:     return "Stale nonce";
        case ACK_OUT_OF_RANGE:    return "Out of range";
        case ACK_BAD_CMD:         return "Bad command";
        default:                  return "?";
    }
}
