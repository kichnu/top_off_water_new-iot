#ifndef KALKWASSER_SCHEDULER_H
#define KALKWASSER_SCHEDULER_H

#include <Arduino.h>

// ============================================================
// KalkwasserScheduler — fixed daily schedule for kalkwasser dosing.
//
// Mixing pump: 4× per day at 00:15, 06:15, 12:15, 18:15 (local time)
//   Duration:  KALK_MIX_DURATION_S (5 min, hardcoded)
//   Settling:  KALK_SETTLE_S (60 min after mixing ends)
//
// Peristaltic pump: every full hour except blocked windows
//   Blocked: HH:00 and (HH+1):00 for each mix base hour
//   Available: 16 hours/day → 02,03,04,05, 08,09,10,11, 14,15,16,17, 20,21,22,23
//
// Concurrency: both pumps wait up to KALK_WAIT_TIMEOUT_S for top-off
//   cycle to complete before executing. If timeout → skip (log warning).
// ============================================================

#define KALK_MIX_DURATION_S    300    // 5 minutes (hardcoded)
#define KALK_SETTLE_S          3600   // 1 hour settling (hardcoded)
#define KALK_WAIT_TIMEOUT_S    600    // 10 min max wait for top-off
#define KALK_CALIBRATION_S     30     // calibration run duration
#define KALK_DOSES_PER_DAY     16     // doses/day based on fixed schedule
#define KALK_DIRECT_MIX_TIMEOUT_S   300   // 5 min auto-off for manual mixing pump
#define KALK_DIRECT_DOSE_TIMEOUT_S   60   // 60 s auto-off for manual peristaltic pump

enum KalkwasserState {
    KALK_IDLE = 0,
    KALK_WAIT_TOPOFF_MIX,   // waiting for top-off to end before mixing
    KALK_WAIT_TOPOFF_DOSE,  // waiting for top-off to end before dosing
    KALK_MIXING,
    KALK_SETTLING,
    KALK_DOSING,
    KALK_CALIBRATING,
    KALK_DIRECT_MIX,        // manual direct control — mixing pump on until explicit stop
    KALK_DIRECT_DOSE,       // manual direct control — peristaltic pump on until explicit stop
};

class KalkwasserScheduler {
public:
    void init();    // call after initFRAM()
    void update();  // call from loop()

    // Config
    bool setConfig(bool enabled, uint16_t daily_dose_ml);
    bool setFlowRate(float measured_ml_30s);  // from calibration result
    bool startCalibration();
    void stopCalibration();

    // Direct manual control (GUI buttons)
    bool directMixOn();
    void directMixOff();
    bool directDoseOn();
    void directDoseOff();

    // Status getters
    KalkwasserState getState()           const { return state; }
    const char*     getStateString()     const;
    bool            isEnabled()          const { return enabled; }
    uint16_t        getDailyDoseMl()     const { return daily_dose_ml; }
    uint32_t        getFlowRateUlPerS()  const { return flow_rate_ul_per_s; }
    float           getFlowRateMlPerS()  const { return flow_rate_ul_per_s / 1000.0f; }
    uint32_t        getLastDoseTs()      const { return last_dose_ts; }
    uint32_t        getLastMixTs()       const { return last_mix_ts; }
    uint32_t        getDoseDurationS()   const { return doseDurationS; }
    bool            isNoTopoffAlarm()    const { return noTopoffAlarm; }
    void            clearAlarm()               { noTopoffAlarm = false; }
    uint16_t        getDoseDoneBits()    const { return dose_done_bits; }
    uint8_t         getMixDoneBits()     const { return mix_done_bits; }

    // Schedule helpers for GUI
    bool     isHourBlocked(int localHour) const;
    uint32_t getNextMixTs()  const;
    uint32_t getNextDoseTs() const;

private:
    KalkwasserState state;
    uint32_t stateStartMs;
    uint32_t doseDurationS;
    bool     noTopoffAlarm;

    // Cached from FRAM
    bool     enabled;
    uint16_t daily_dose_ml;
    uint32_t flow_rate_ul_per_s;
    uint32_t last_dose_ts;
    uint32_t last_mix_ts;
    uint16_t dose_done_bits;   // bitmaska slotów dawkowania wykonanych dziś
    uint8_t  mix_done_bits;    // bitmaska slotów mieszania wykonanych dziś
    uint8_t  done_day;         // dzień miesiąca (local) dla którego bity obowiązują

    void markMixSlotDone(uint32_t nowTs);
    void markDoseSlotDone(uint32_t nowTs);
    void checkDayRollover(uint32_t nowTs);

    bool     isMixTime(uint32_t nowTs)          const;
    bool     isDoseTime(uint32_t nowTs)          const;
    bool     mixAlreadyDoneThisSlot(uint32_t nowTs) const;
    bool     doseAlreadyDoneThisHour(uint32_t nowTs) const;
    uint32_t calcDoseDurationS()                const;
    void     saveToFRAM();
    void     checkNoTopoffAlarm();
};

extern KalkwasserScheduler kalkwasserScheduler;

#endif // KALKWASSER_SCHEDULER_H
