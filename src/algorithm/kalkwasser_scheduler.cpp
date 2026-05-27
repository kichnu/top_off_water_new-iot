#include "kalkwasser_scheduler.h"
#include "../hardware/fram_controller.h"
#include "../hardware/peristaltic_pump.h"
#include "../hardware/mixing_pump.h"
#include "../hardware/rtc_controller.h"
#include "../algorithm/water_algorithm.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <time.h>

KalkwasserScheduler kalkwasserScheduler;

// Mixing base hours (local time): 00:15, 06:15, 12:15, 18:15
static const int MIX_BASE_HOURS[4] = {0, 6, 12, 18};
#define MIX_MINUTE 15

// ============================================================
// init
// ============================================================

void KalkwasserScheduler::init() {
    state        = KALK_IDLE;
    stateStartMs = 0;
    noTopoffAlarm = false;

    KalkwasserConfig cfg;
    if (loadKalkwasserConfigFromFRAM(cfg)) {
        enabled            = cfg.enabled;
        daily_dose_ml      = cfg.daily_dose_ml;
        flow_rate_ul_per_s = cfg.flow_rate_ul_per_s;
        last_dose_ts       = cfg.last_dose_ts;
        last_mix_ts        = cfg.last_mix_ts;
        dose_done_bits     = cfg.dose_done_bits;
        mix_done_bits      = cfg.mix_done_bits;
        done_day           = cfg.done_day;
        LOG_INFO("KalkwasserScheduler: loaded — enabled=%d dose=%dml rate=%lu ul/s done_day=%d",
                 enabled, daily_dose_ml, flow_rate_ul_per_s, done_day);
    } else {
        enabled            = false;
        daily_dose_ml      = 0;
        flow_rate_ul_per_s = 0;
        last_dose_ts       = 0;
        last_mix_ts        = 0;
        dose_done_bits     = 0;
        mix_done_bits      = 0;
        done_day           = 0;
        LOG_INFO("KalkwasserScheduler: no FRAM config — defaults (disabled)");
    }

    doseDurationS = calcDoseDurationS();
}

// ============================================================
// update — call from loop()
// ============================================================

void KalkwasserScheduler::update() {
    updatePeristalticPump();   // drive auto-stop timer — must run in all modes

    uint32_t nowMs = millis();

    // ── Manual / override states — handled first, independent of service mode ──
    // KALK_CALIBRATING, KALK_DIRECT_MIX, KALK_DIRECT_DOSE must time-out even when
    // the system is in Service mode; without this the pump runs indefinitely because
    // the early return below would skip the switch.
    switch (state) {
        case KALK_CALIBRATING:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_CALIBRATION_S * 1000UL)) {
                stopPeristalticPump();
                state = KALK_IDLE;
                LOG_INFO("KalkwasserScheduler: calibration run complete (30s) → IDLE");
            }
            return;

        case KALK_DIRECT_MIX:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_DIRECT_MIX_TIMEOUT_S * 1000UL)) {
                stopMixingPump();
                state = KALK_IDLE;
                LOG_WARNING("KalkwasserScheduler: mixing pump DIRECT timeout → IDLE");
            }
            return;

        case KALK_DIRECT_DOSE:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_DIRECT_DOSE_TIMEOUT_S * 1000UL)) {
                stopPeristalticPump();
                state = KALK_IDLE;
                LOG_WARNING("KalkwasserScheduler: peristaltic pump DIRECT timeout → IDLE");
            }
            return;

        default:
            break;
    }

    // ── Service mode: stop scheduled operations and bail out ──
    if (isSystemDisabled()) {
        if (state != KALK_IDLE) {
            stopPeristalticPump();
            stopMixingPump();
            state = KALK_IDLE;
            LOG_INFO("KalkwasserScheduler: system disabled → IDLE");
        }
        return;
    }

    uint32_t nowTs = (uint32_t)getUnixTimestamp();
    checkDayRollover(nowTs);   // always run — clears done_bits even when disabled

    if (!enabled) return;

    switch (state) {

        case KALK_IDLE:
            if (daily_dose_ml > 0 && !mixAlreadyDoneThisSlot(nowTs) && isMixTime(nowTs)) {
                if (waterAlgorithm.isInCycle()) {
                    state        = KALK_WAIT_TOPOFF_MIX;
                    stateStartMs = nowMs;
                    LOG_INFO("KalkwasserScheduler: mix time, waiting for top-off cycle");
                } else {
                    last_mix_ts = nowTs;
                    markMixSlotDone(nowTs);
                    saveToFRAM();
                    startMixingPump();
                    state        = KALK_MIXING;
                    stateStartMs = nowMs;
                    LOG_INFO("KalkwasserScheduler: MIXING start");
                }
            } else if (flow_rate_ul_per_s > 0 && daily_dose_ml > 0 &&
                       !doseAlreadyDoneThisHour(nowTs) && isDoseTime(nowTs)) {
                checkNoTopoffAlarm();
                if (waterAlgorithm.isInCycle()) {
                    state        = KALK_WAIT_TOPOFF_DOSE;
                    stateStartMs = nowMs;
                    LOG_INFO("KalkwasserScheduler: dose time, waiting for top-off cycle");
                } else {
                    last_dose_ts  = nowTs;
                    markDoseSlotDone(nowTs);
                    doseDurationS = calcDoseDurationS();
                    saveToFRAM();
                    startPeristalticPump(doseDurationS);
                    state        = KALK_DOSING;
                    stateStartMs = nowMs;
                    LOG_INFO("KalkwasserScheduler: DOSING start, duration=%lus", doseDurationS);
                }
            }
            break;

        case KALK_WAIT_TOPOFF_MIX:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_WAIT_TIMEOUT_S * 1000UL)) {
                LOG_WARNING("KalkwasserScheduler: timeout waiting for top-off (mix skip)");
                state = KALK_IDLE;
            } else if (!waterAlgorithm.isInCycle()) {
                last_mix_ts = nowTs;
                markMixSlotDone(nowTs);
                saveToFRAM();
                startMixingPump();
                state        = KALK_MIXING;
                stateStartMs = nowMs;
                LOG_INFO("KalkwasserScheduler: MIXING start (after wait)");
            }
            break;

        case KALK_WAIT_TOPOFF_DOSE:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_WAIT_TIMEOUT_S * 1000UL)) {
                LOG_WARNING("KalkwasserScheduler: timeout waiting for top-off (dose skip)");
                state = KALK_IDLE;
            } else if (!waterAlgorithm.isInCycle()) {
                last_dose_ts  = nowTs;
                markDoseSlotDone(nowTs);
                doseDurationS = calcDoseDurationS();
                saveToFRAM();
                startPeristalticPump(doseDurationS);
                state        = KALK_DOSING;
                stateStartMs = nowMs;
                LOG_INFO("KalkwasserScheduler: DOSING start (after wait), duration=%lus", doseDurationS);
            }
            break;

        case KALK_MIXING:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_MIX_DURATION_S * 1000UL)) {
                stopMixingPump();
                state        = KALK_SETTLING;
                stateStartMs = nowMs;
                LOG_INFO("KalkwasserScheduler: mixing done → SETTLING (1h)");
            }
            break;

        case KALK_SETTLING:
            if ((nowMs - stateStartMs) >= (uint32_t)(KALK_SETTLE_S * 1000UL)) {
                state = KALK_IDLE;
                LOG_INFO("KalkwasserScheduler: settling done → IDLE");
            }
            break;

        case KALK_DOSING:
            // Extra 500ms margin over peristalticPump auto-stop
            if ((nowMs - stateStartMs) >= (doseDurationS * 1000UL + 500UL)) {
                if (isPeristalticPumpRunning()) stopPeristalticPump();
                state = KALK_IDLE;
                LOG_INFO("KalkwasserScheduler: dosing complete → IDLE");
            }
            break;

        default:
            break;
    }
}

// ============================================================
// Config setters
// ============================================================

bool KalkwasserScheduler::setConfig(bool en, uint16_t dose_ml) {
    if (!en && state != KALK_IDLE && state != KALK_CALIBRATING) {
        stopPeristalticPump();
        stopMixingPump();
        state = KALK_IDLE;
    }
    enabled       = en;
    daily_dose_ml = dose_ml;
    doseDurationS = calcDoseDurationS();
    saveToFRAM();
    LOG_INFO("KalkwasserScheduler: config set enabled=%d dose=%dml dose_dur=%lus",
             enabled, daily_dose_ml, doseDurationS);
    return true;
}

bool KalkwasserScheduler::setFlowRate(float measured_ml_30s) {
    if (measured_ml_30s < 0.1f || measured_ml_30s > 500.0f) return false;
    // µl/s = ml_in_30s * 1000 / 30
    flow_rate_ul_per_s = (uint32_t)(measured_ml_30s * 1000.0f / 30.0f);
    doseDurationS      = calcDoseDurationS();
    saveToFRAM();
    LOG_INFO("KalkwasserScheduler: flow rate set %.1f ml/30s → %lu ul/s, dose=%lus",
             measured_ml_30s, flow_rate_ul_per_s, doseDurationS);
    return true;
}

bool KalkwasserScheduler::startCalibration() {
    if (state != KALK_IDLE)          return false;
    if (waterAlgorithm.isInCycle())  return false;
    startPeristalticPump(KALK_CALIBRATION_S);
    state        = KALK_CALIBRATING;
    stateStartMs = millis();
    LOG_INFO("KalkwasserScheduler: calibration started (%ds)", KALK_CALIBRATION_S);
    return true;
}

void KalkwasserScheduler::stopCalibration() {
    if (state == KALK_CALIBRATING) {
        stopPeristalticPump();
        state = KALK_IDLE;
        LOG_INFO("KalkwasserScheduler: calibration stopped manually");
    }
}

// ============================================================
// Direct manual control (GUI buttons)
// ============================================================

bool KalkwasserScheduler::directMixOn() {
    if (state != KALK_IDLE) return false;
    startMixingPump();
    state        = KALK_DIRECT_MIX;
    stateStartMs = millis();
    LOG_INFO("KalkwasserScheduler: mixing pump DIRECT ON (auto-off in %ds)", KALK_DIRECT_MIX_TIMEOUT_S);
    return true;
}

void KalkwasserScheduler::directMixOff() {
    if (state == KALK_DIRECT_MIX) {
        stopMixingPump();
        state = KALK_IDLE;
        LOG_INFO("KalkwasserScheduler: mixing pump DIRECT OFF");
    }
}

bool KalkwasserScheduler::directDoseOn() {
    if (state != KALK_IDLE) return false;
    startPeristalticPump(0);   // durationSeconds=0 → continuous, backend timeout handled by scheduler
    state        = KALK_DIRECT_DOSE;
    stateStartMs = millis();
    LOG_INFO("KalkwasserScheduler: peristaltic pump DIRECT ON (auto-off in %ds)", KALK_DIRECT_DOSE_TIMEOUT_S);
    return true;
}

void KalkwasserScheduler::directDoseOff() {
    if (state == KALK_DIRECT_DOSE) {
        stopPeristalticPump();
        state = KALK_IDLE;
        LOG_INFO("KalkwasserScheduler: peristaltic pump DIRECT OFF");
    }
}

// ============================================================
// Schedule logic — local time
// ============================================================

bool KalkwasserScheduler::isMixTime(uint32_t nowTs) const {
    struct tm t;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &t);
    for (int i = 0; i < 4; i++) {
        if (t.tm_hour == MIX_BASE_HOURS[i] && t.tm_min == MIX_MINUTE) return true;
    }
    return false;
}

bool KalkwasserScheduler::isDoseTime(uint32_t nowTs) const {
    struct tm t;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &t);
    if (t.tm_min != 0) return false;   // only at top of hour
    return !isHourBlocked(t.tm_hour);
}

bool KalkwasserScheduler::isHourBlocked(int localHour) const {
    // Blocked window per mix slot: [MIX_MINUTE, MIX_MINUTE + MIX_DURATION_MIN + 60)
    // In minutes from midnight.  MIX_DURATION_S/60 = 5min → window = [15, 80)
    // Also block the mix base hour itself: dosing fires at :00 but mix is at :15 —
    // the base hour (0,6,12,18) is absent from DOSE_HOURS_ARR so the done-bit
    // would never be set and the GUI has no tile for it.
    int h_min = localHour * 60;
    for (int i = 0; i < 4; i++) {
        if (localHour == MIX_BASE_HOURS[i]) return true;
        int block_start = MIX_BASE_HOURS[i] * 60 + MIX_MINUTE;
        int block_end   = block_start + KALK_MIX_DURATION_S / 60 + 60;  // 5 + 60 = 65 min
        if (h_min >= block_start && h_min < block_end) return true;
    }
    return false;
}

bool KalkwasserScheduler::mixAlreadyDoneThisSlot(uint32_t nowTs) const {
    if (last_mix_ts == 0) return false;
    struct tm now_t, last_t;
    time_t ts_now  = (time_t)nowTs;
    time_t ts_last = (time_t)last_mix_ts;
    localtime_r(&ts_now,  &now_t);
    localtime_r(&ts_last, &last_t);
    // Same calendar day + same mix base hour?
    for (int i = 0; i < 4; i++) {
        if (now_t.tm_hour == MIX_BASE_HOURS[i]) {
            if (last_t.tm_year == now_t.tm_year &&
                last_t.tm_yday == now_t.tm_yday &&
                last_t.tm_hour == MIX_BASE_HOURS[i]) return true;
        }
    }
    return false;
}

bool KalkwasserScheduler::doseAlreadyDoneThisHour(uint32_t nowTs) const {
    if (last_dose_ts == 0) return false;
    // Compare UTC-hour buckets: same hour = floor(ts/3600)
    return (last_dose_ts / 3600UL) >= (nowTs / 3600UL);
}

uint32_t KalkwasserScheduler::calcDoseDurationS() const {
    if (flow_rate_ul_per_s == 0 || daily_dose_ml == 0) return 0;
    uint32_t single_ul = (uint32_t)daily_dose_ml * 1000UL / KALK_DOSES_PER_DAY;
    uint32_t dur = single_ul / flow_rate_ul_per_s;
    return (dur < 1) ? 1 : dur;   // minimum 1 second
}

// ============================================================
// Next event helpers for GUI
// ============================================================

uint32_t KalkwasserScheduler::getNextMixTs() const {
    uint32_t nowTs = (uint32_t)getUnixTimestamp();
    struct tm t;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &t);

    // Seconds elapsed since local midnight
    int sec_of_day = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    // Local midnight as UTC
    uint32_t midnight = nowTs - (uint32_t)sec_of_day;

    for (int i = 0; i < 4; i++) {
        uint32_t slot_ts = midnight + MIX_BASE_HOURS[i] * 3600 + MIX_MINUTE * 60;
        if (slot_ts > nowTs) return slot_ts;
    }
    // All today's slots past — return first tomorrow
    return midnight + 86400 + MIX_BASE_HOURS[0] * 3600 + MIX_MINUTE * 60;
}

uint32_t KalkwasserScheduler::getNextDoseTs() const {
    uint32_t nowTs = (uint32_t)getUnixTimestamp();
    struct tm t;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &t);

    int sec_of_day = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    uint32_t midnight = nowTs - (uint32_t)sec_of_day;

    for (int h = t.tm_hour + 1; h < 24; h++) {
        if (!isHourBlocked(h)) return midnight + h * 3600;
    }
    for (int h = 0; h < 24; h++) {
        if (!isHourBlocked(h)) return midnight + 86400 + h * 3600;
    }
    return 0;
}

// ============================================================
// Internal helpers
// ============================================================

void KalkwasserScheduler::saveToFRAM() {
    if (framBusy) return;
    KalkwasserConfig cfg;
    cfg.magic              = KALKWASSER_CONFIG_MAGIC;
    cfg.enabled            = enabled ? 1 : 0;
    cfg.daily_dose_ml      = daily_dose_ml;
    cfg.flow_rate_ul_per_s = flow_rate_ul_per_s;
    cfg.last_dose_ts       = last_dose_ts;
    cfg.last_mix_ts        = last_mix_ts;
    cfg.dose_done_bits     = dose_done_bits;
    cfg.mix_done_bits      = mix_done_bits;
    cfg.done_day           = done_day;
    framBusy = true;
    saveKalkwasserConfigToFRAM(cfg);
    framBusy = false;
}

// ============================================================
// Daily rollover + slot done bitmask helpers
// ============================================================

static const int DOSE_HOURS_ARR[16] = {2,3,4,5,8,9,10,11,14,15,16,17,20,21,22,23};

static int getCurrentHour(uint32_t nowTs) {
    struct tm tm_local;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &tm_local);
    return tm_local.tm_hour;
}

static uint8_t getCurrentMday(uint32_t nowTs) {
    struct tm tm_local;
    time_t ts = (time_t)nowTs;
    localtime_r(&ts, &tm_local);
    return (uint8_t)tm_local.tm_mday;
}

void KalkwasserScheduler::checkDayRollover(uint32_t nowTs) {
    static uint32_t lastCheckedTs = 0;
    if (nowTs == lastCheckedTs) return;
    lastCheckedTs = nowTs;

    uint8_t today = getCurrentMday(nowTs);
    if (today != done_day) {
        dose_done_bits = 0;
        mix_done_bits  = 0;
        done_day       = today;
        saveToFRAM();
        LOG_INFO("KalkwasserScheduler: new day (%d) — done bits cleared", today);
    }
}

static void markSlotDone(uint32_t nowTs, const int* hoursArr, int count,
                         uint16_t& bits, const char* label) {
    int h = getCurrentHour(nowTs);
    for (int i = 0; i < count; i++) {
        if (h == hoursArr[i]) {
            bits |= (uint16_t)(1u << i);
            LOG_INFO("KalkwasserScheduler: %s slot %d (%02d:00) marked done", label, i, h);
            return;
        }
    }
}

void KalkwasserScheduler::markMixSlotDone(uint32_t nowTs) {
    uint16_t bits = mix_done_bits;
    markSlotDone(nowTs, MIX_BASE_HOURS, 4, bits, "mix");
    mix_done_bits = (uint8_t)bits;
}

void KalkwasserScheduler::markDoseSlotDone(uint32_t nowTs) {
    markSlotDone(nowTs, DOSE_HOURS_ARR, 16, dose_done_bits, "dose");
}

void KalkwasserScheduler::checkNoTopoffAlarm() {
    if (daily_dose_ml > 0 && waterAlgorithm.getRolling24hVolume() == 0) {
        noTopoffAlarm = true;
        LOG_WARNING("KalkwasserScheduler: ALARM — kalkwasser active but no top-off in 24h!");
    } else {
        noTopoffAlarm = false;
    }
}

const char* KalkwasserScheduler::getStateString() const {
    switch (state) {
        case KALK_IDLE:             return "IDLE";
        case KALK_WAIT_TOPOFF_MIX:  return "WAIT_MIX";
        case KALK_WAIT_TOPOFF_DOSE: return "WAIT_DOSE";
        case KALK_MIXING:           return "MIXING";
        case KALK_SETTLING:         return "SETTLING";
        case KALK_DOSING:           return "DOSING";
        case KALK_CALIBRATING:      return "CALIBRATING";
        case KALK_DIRECT_MIX:       return "DIRECT_MIX";
        case KALK_DIRECT_DOSE:      return "DIRECT_DOSE";
        default:                    return "UNKNOWN";
    }
}
