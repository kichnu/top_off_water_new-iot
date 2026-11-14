#pragma once
// Wire format of the top-off ESP-NOW protocol (node ↔ touch panel home_display-iot).
// Copied, not shared: the panel keeps the same file (src/topoff/topoff_proto.h) — keep both in sync.
// Plan: home_display-iot/docs/PLAN_DOLEWKA.md.
//
// Frame = FrameHdr + body + HMAC-SHA256 truncated to 16 B over (hdr + body).
// Key = SHA-256(KEY_CONTEXT + Wi-Fi password) — own context, so lamp and top-off frames never
// validate against each other.

#include <stddef.h>
#include <stdint.h>

namespace topoff {

constexpr uint16_t FRAME_MAGIC   = 0x4F54;  // "TO"
constexpr uint8_t  FRAME_VERSION = 1;
constexpr size_t   FRAME_MAX     = 250;     // ESP-NOW v1
constexpr size_t   TAG_LEN       = 16;
constexpr const char* KEY_CONTEXT = "TOPOFF-ESPNOW-v1:";

constexpr uint32_t STATUS_INTERVAL_MS = 5000;   // panel: 3 missed = offline
constexpr uint32_t STATUS_DEBOUNCE_MS = 300;    // change → status right away

enum FrameType : uint8_t {
    FT_STATUS = 1,   // node → broadcast
    FT_CMD    = 2,   // panel → node (stage D4)
    FT_ACK    = 3,   // node → panel (stage D4)
};

enum NodeStatus : uint8_t { NS_OK = 0, NS_WARNING, NS_ALARM, NS_FAULT, NS_SERVICE };

// StatusBody.flags
constexpr uint8_t SF_SUMP_LOW        = 0x01;  // float: water below the level
constexpr uint8_t SF_PUMP_ON         = 0x02;
constexpr uint8_t SF_MANUAL_SESSION  = 0x04;  // manual portions running (incl. pauses)
constexpr uint8_t SF_MANUAL_PAUSE    = 0x08;
constexpr uint8_t SF_CALIBRATED      = 0x10;
constexpr uint8_t SF_RES_READING     = 0x20;  // reservoir measured by ToF
constexpr uint8_t SF_RES_ESTIMATED   = 0x40;  // no reading — minimum estimate
constexpr uint8_t SF_TIME_VALID      = 0x80;  // utc is a real time

// StatusBody.alarms
constexpr uint16_t AL_LOW_RESERVOIR      = 0x0001;  // P1
constexpr uint16_t AL_DAILY_LIMIT        = 0x0002;  // P1
constexpr uint16_t AL_RATE_ANOMALY       = 0x0004;  // P1
constexpr uint16_t AL_NO_TOPOFF          = 0x0008;  // P1
constexpr uint16_t AL_HW_FAULT           = 0x0010;  // P1
constexpr uint16_t AL_RESERVE_LOW        = 0x0020;  // P3
constexpr uint16_t AL_LEVEL_NO_READING   = 0x0040;  // P3
constexpr uint16_t AL_LEVEL_SENSOR_FAULT = 0x0080;  // P3
constexpr uint16_t AL_UNCALIBRATED       = 0x0100;  // P3 (status "Uwaga")

#pragma pack(push, 1)
struct FrameHdr {
    uint16_t magic;
    uint8_t  version;
    uint8_t  type;
    uint32_t boot_id;          // random at every boot
    uint32_t seq;              // increases within boot_id — replay rejection
};

struct StatusBody {
    char     name[32];         // device ID from provisioning
    char     fw[12];
    uint32_t ip;
    uint32_t utc;              // 0 = no valid time
    uint32_t cmd_nonce;        // commands must echo it (stage D4); changes after each executed command
    uint8_t  node_status;      // NodeStatus
    uint8_t  auto_mode;        // 1 = Auto, 0 = Service
    uint16_t service_left_min; // Service Mode: minutes to the auto return (0 = none)
    uint8_t  algo_state;       // AlgorithmState
    uint8_t  error_code;       // ErrorCode, 0 outside STATE_ERROR
    uint8_t  flags;            // SF_*
    uint8_t  manual_left;      // portions incl. the running one
    uint8_t  manual_done;      // finished in the current session
    uint8_t  hw_fault;         // HW_FAULT_* bits (FRAM / RTC failing now)
    uint16_t phase_remaining_s;
    uint16_t volume_24h_ml;    // auto top-offs only
    uint16_t daily_limit_ml;
    uint16_t dose_ml;
    uint16_t alarms;           // AL_*
    uint8_t  res_zone;         // 0 OK, 1 reserve, 2 alarm, 3 unknown
    uint8_t  res_pct;          // fill 0–100 (clamped)
    uint8_t  res_reserve_pct;  // reserve threshold, % of capacity
    uint8_t  res_alarm_pct;    // alarm threshold, % of capacity
    uint16_t res_portions;     // whole portions above the alarm level
    int16_t  res_hours_x10;    // reserve time × 10, −1 = unknown
    uint8_t  res_alarm_cause;  // ReserveAlarmCause
    uint8_t  level_sensor;     // 0 absent, 1 OK, 2 fault
};
#pragma pack(pop)

static_assert(sizeof(FrameHdr) == 12, "FrameHdr size");
static_assert(sizeof(StatusBody) == 86, "StatusBody size");
static_assert(sizeof(FrameHdr) + sizeof(StatusBody) + TAG_LEN <= FRAME_MAX, "status frame too long");

}  // namespace topoff
