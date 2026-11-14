#include "node_status.h"

#include "../algorithm/water_algorithm.h"
#include "../config/config.h"
#include "../hardware/hw_health.h"
#include "../hardware/level_sensor.h"
#include "../hardware/pump_calibration.h"
#include "../hardware/reserve_controller.h"
#include "../network/topoff_proto.h"

extern WaterAlgorithm waterAlgorithm;

using namespace topoff;

static constexpr uint16_t P1_ALARMS =
    AL_LOW_RESERVOIR | AL_DAILY_LIMIT | AL_RATE_ANOMALY | AL_NO_TOPOFF | AL_HW_FAULT;

uint8_t getErrorCode() {
    return waterAlgorithm.getState() == STATE_ERROR ? (uint8_t)waterAlgorithm.getLastError() : ERROR_NONE;
}

uint16_t getAlarmBits() {
    uint16_t a = 0;
    uint8_t err = getErrorCode();
    if (isReserveEmpty() || err == ERROR_LOW_RESERVOIR) a |= AL_LOW_RESERVOIR;
    if (err == ERROR_DAILY_LIMIT || err == ERROR_BOTH)  a |= AL_DAILY_LIMIT;
    if (err == ERROR_RED_ALERT || err == ERROR_BOTH)    a |= AL_RATE_ANOMALY;
    if (waterAlgorithm.isNoTopOffAlarm())               a |= AL_NO_TOPOFF;
    if (isHwFaultAlarm())                               a |= AL_HW_FAULT;
    if (isReserveLow() && !isReserveEmpty())            a |= AL_RESERVE_LOW;
    if (isReserveNoReadingAlert())                      a |= AL_LEVEL_NO_READING;
    if (getLevelSensorState() == LEVEL_SENSOR_FAULT)    a |= AL_LEVEL_SENSOR_FAULT;
    if (!isPumpCalibrated())                            a |= AL_UNCALIBRATED;
    return a;
}

uint8_t getNodeStatus() {
    uint16_t a = getAlarmBits();
    if (a & AL_HW_FAULT) return NS_FAULT;
    if (a & P1_ALARMS)   return NS_ALARM;
    if (isSystemDisabled()) return NS_SERVICE;
    if (a)               return NS_WARNING;
    return NS_OK;
}

const char* nodeStatusString(uint8_t status) {
    switch (status) {
        case NS_OK:      return "OK";
        case NS_WARNING: return "WARNING";
        case NS_ALARM:   return "ALARM";
        case NS_FAULT:   return "FAULT";
        case NS_SERVICE: return "SERVICE";
        default:         return "?";
    }
}
