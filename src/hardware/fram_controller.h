




#ifndef FRAM_CONTROLLER_H
#define FRAM_CONTROLLER_H

#include <Arduino.h>
#include <vector>

#include "fram_constants.h" 

// ===============================
// LEGACY: PumpCycle — kept for compatibility with the existing FRAM layout
// New code uses TopOffRecord (see algorithm_config.h)
// ===============================
struct PumpCycle {
    uint32_t timestamp;
    uint32_t trigger_time;
    uint32_t time_gap_1;
    uint32_t time_gap_2;
    uint32_t water_trigger_time;
    uint16_t pump_duration;
    uint8_t  pump_attempts;
    uint8_t  sensor_results;
    uint8_t  error_code;
    uint16_t volume_dose;
    uint8_t  _pad;            // padding to 28 bytes
};

// ===============================
// FRAM MEMORY LAYOUT (UNIFIED)
// ===============================

// 0x0000-0x0017: Common area (shared between modes)
#define FRAM_ADDR_MAGIC        0x0000  // 4 bytes - magic number for validation
#define FRAM_ADDR_VERSION      0x0004  // 2 bytes - data structure version

// 0x0018-0x0417: FRAM Programmer credentials (1024 bytes)
#define FRAM_CREDENTIALS_ADDR  0x0018  // Start of credentials section
#define FRAM_CREDENTIALS_SIZE  1024    // 1024 bytes for credentials

// 0x0500+: ESP32 Water System data (moved to avoid conflict)
#define FRAM_ESP32_BASE        0x0500  // Base address for ESP32 data

// ESP32 volume settings
#define FRAM_ADDR_VOLUME_ML    (FRAM_ESP32_BASE + 0x06)  // 4 bytes - volume per second (float)
#define FRAM_ADDR_CHECKSUM     (FRAM_ESP32_BASE + 0x0A)  // 2 bytes - simple checksum

// ESP32 error statistics
#define FRAM_ADDR_GAP1_SUM     (FRAM_ESP32_BASE + 0x0C)  // 2 bytes - gap1_fail_sum
#define FRAM_ADDR_GAP2_SUM     (FRAM_ESP32_BASE + 0x0E)  // 2 bytes - gap2_fail_sum  
#define FRAM_ADDR_WATER_SUM    (FRAM_ESP32_BASE + 0x10)  // 2 bytes - water_fail_sum
#define FRAM_ADDR_LAST_RESET   (FRAM_ESP32_BASE + 0x12)  // 4 bytes - timestamp of the last reset
#define FRAM_ADDR_STATS_CHKSUM (FRAM_ESP32_BASE + 0x16)  // 2 bytes - stats checksum

// 🆕 NEW: Daily volume tracking
#define FRAM_ADDR_DAILY_VOLUME    (FRAM_ESP32_BASE + 0x18)  // 2 bytes - uint16_t
#define FRAM_ADDR_LAST_RESET_UTC  (FRAM_ESP32_BASE + 0x1A)  // 4 bytes - uint32_t (was 12)
#define FRAM_ADDR_DAILY_CHECKSUM  (FRAM_ESP32_BASE + 0x1E)  // 2 bytes - checksum
\
// 0x0520–0x0529: former reserve volume (configured ml + current ml + checksum) — unused

// 🆕 NEW: Configurable daily limit (replaces hardcoded FILL_WATER_MAX)
#define FRAM_ADDR_FILL_WATER_MAX     (FRAM_ESP32_BASE + 0x2A)  // 2 bytes - uint16_t (ml)
#define FRAM_ADDR_FILL_MAX_CHKSUM    (FRAM_ESP32_BASE + 0x2C)  // 2 bytes - checksum

// ESP32 cycle management (LEGACY — replaced by the TopOffRecord ring buffer below)
#define FRAM_ADDR_CYCLE_COUNT  (FRAM_ESP32_BASE + 0x30)  // 2 bytes - number of stored cycles
#define FRAM_ADDR_CYCLE_INDEX  (FRAM_ESP32_BASE + 0x32)  // 2 bytes - current write index (circular buffer)
#define FRAM_ADDR_CYCLE_DATA   (FRAM_ESP32_BASE + 0x100) // Start of cycle data (legacy)

#define FRAM_MAX_CYCLES        30      // Legacy — not used in new code
#define FRAM_CYCLE_SIZE        28      // Legacy — not used in new code

// ===============================
// 🆕 TOP-OFF ALGORITHM SECTIONS
// ===============================

// Algorithm configuration (TopOffConfig) — 40 bytes
// NOTE: sizeof(TopOffConfig) == 40 (verified by static_assert)
//       The checksum MUST be at an address >= FRAM_ADDR_TOPOFF_CONFIG + 40 = 0x0588
#define FRAM_ADDR_TOPOFF_CONFIG     0x0560   // 40 bytes — TopOffConfig struct (0x0560–0x0587)
#define FRAM_ADDR_TOPOFF_CFG_CHKSUM 0x0588   // 2 bytes — configuration checksum

// EMA block (EmaBlock) — 16 bytes
#define FRAM_ADDR_EMA_BLOCK         0x0590   // 16 bytes — EmaBlock struct (0x0590–0x059F)
#define FRAM_ADDR_EMA_CHKSUM        0x05A0   // 2 bytes — checksum EMA

// Reservoir geometry and level-sensor settings (ReservoirConfig) — 32 bytes
#define FRAM_ADDR_RESERVOIR_CFG     0x05A2   // 32 bytes — ReservoirConfig struct (0x05A2–0x05C1)
#define FRAM_ADDR_RESERVOIR_CHKSUM  0x05C2   // 2 bytes — checksum

// Reserve tracking (ReserveTrack: last level reading, water pumped since, alarm latch) — 16 bytes
#define FRAM_ADDR_RESERVE_TRACK     0x05C4   // 16 bytes — ReserveTrack struct (0x05C4–0x05D3)
#define FRAM_ADDR_RESERVE_CHKSUM    0x05D4   // 2 bytes — checksum
// ATO error latch, anomaly alarm disarm time, no-top-off watchdog (AlgErrorState) — 12 bytes
#define FRAM_ADDR_ALG_ERROR         0x05D6   // 12 bytes — AlgErrorState struct (0x05D6–0x05E1)
#define FRAM_ADDR_ALG_ERROR_CHKSUM  0x05E2   // 2 bytes — checksum
// 0x05E4–0x05FE: free
#define FRAM_ADDR_HEALTH_SCRATCH    0x05FF   // 1 byte — hw_health write/read-back check

// Top-off record ring buffer (TopOffRecord, 20 bytes each)
#define TOPOFF_HISTORY_SIZE         60       // At most 60 records (60×20=1200 bytes)
#define FRAM_TOPOFF_RECORD_SIZE     20       // sizeof(TopOffRecord) — verified by static_assert
#define FRAM_ADDR_TOPOFF_COUNT      0x0600   // 2 bytes — number of stored records
#define FRAM_ADDR_TOPOFF_WPTR       0x0602   // 2 bytes — write pointer (ring buffer)
#define FRAM_ADDR_TOPOFF_DATA       0x0610   // Data start: 60 × 20 = 1200 bytes → ends at 0x0AC0

// Level sensor event log (diagnostics, see level_event_log.h): 16 B header + 48 × 128 B → ends 0x2910
// (0x1000 is the scratch address of testFRAM())
#define FRAM_ADDR_LEVEL_LOG         0x1100

// Common constants
// #define FRAM_MAGIC_NUMBER      0x57415452  // "WATR" in hex
// #define FRAM_DATA_VERSION      0x0002      // Version 2 (updated for dual-mode)

// Basic FRAM functions
bool initFRAM();
bool isFramInitialized();
bool framReconnect();
bool loadVolumeFromFRAM(float& volume);
bool saveVolumeToFRAM(float volume);
bool verifyFRAM();
void testFRAM();
// Raw access for self-checked blocks (e.g. the level event log)
bool framReadRaw(uint16_t addr, uint8_t* buf, size_t len);
bool framWriteRaw(uint16_t addr, const uint8_t* buf, size_t len);

struct DailyVolumeData {
    uint16_t volume_ml;
    uint32_t last_reset_utc_day;
};

bool saveDailyVolumeToFRAM(uint16_t dailyVolume, uint32_t utcDay);
bool loadDailyVolumeFromFRAM(uint16_t& dailyVolume, uint32_t& utcDay);

// Cycle management functions (implemented in fram_controller.cpp)
bool saveCycleToFRAM(const PumpCycle& cycle);
bool loadCyclesFromFRAM(std::vector<PumpCycle>& cycles, uint16_t maxCount = FRAM_MAX_CYCLES);
uint16_t getCycleCountFromFRAM();
// FRAM busy flag - prevents concurrent I2C access during writes
extern volatile bool framBusy;

// Error statistics struct
struct ErrorStats {
    uint16_t gap1_fail_sum;
    uint16_t gap2_fail_sum;
    uint16_t water_fail_sum;
    uint32_t last_reset_timestamp;
};

// Error statistics functions
bool loadErrorStatsFromFRAM(ErrorStats& stats);
bool saveErrorStatsToFRAM(const ErrorStats& stats);
bool resetErrorStatsInFRAM();
bool incrementErrorStats(uint8_t gap1_increment, uint8_t gap2_increment, uint8_t water_increment);

// ===============================
// CONFIGURABLE FILL_WATER_MAX
// ===============================
bool saveFillWaterMaxToFRAM(uint16_t fillWaterMax);
bool loadFillWaterMaxFromFRAM(uint16_t& fillWaterMax);

// ===============================
// 🆕 TOP-OFF RECORD FUNCTIONS
// ===============================

// Forward declarations (structs defined in algorithm_config.h)
struct TopOffRecord;
struct EmaBlock;
struct TopOffConfig;
struct ReservoirConfig;
struct ReserveTrack;
struct AlgErrorState;

// Top-off record ring buffer
bool saveTopOffRecord(const TopOffRecord& record);
uint16_t loadTopOffHistory(TopOffRecord* buf, uint16_t maxCount);
bool loadLastTopOffRecord(TopOffRecord& record);
uint16_t getTopOffRecordCount();
bool clearTopOffRingBuffer();      // Clears the ring buffer and the EMA
bool isTopOffRingBufferValid();    // false when count > TOPOFF_HISTORY_SIZE (legacy/corrupt data)

// EMA block
bool saveEmaBlockToFRAM(const EmaBlock& ema);
bool loadEmaBlockFromFRAM(EmaBlock& ema);

// Algorithm configuration
bool saveTopOffConfigToFRAM(const TopOffConfig& cfg);
bool loadTopOffConfigFromFRAM(TopOffConfig& cfg);

// Reservoir geometry and level-sensor settings
bool saveReservoirConfigToFRAM(const ReservoirConfig& cfg);
bool loadReservoirConfigFromFRAM(ReservoirConfig& cfg);

// Reserve tracking (reserve_controller)
bool saveReserveTrackToFRAM(const ReserveTrack& t);
bool loadReserveTrackFromFRAM(ReserveTrack& t);

// ATO error latch (water_algorithm)
bool saveAlgErrorStateToFRAM(const AlgErrorState& s);
bool loadAlgErrorStateFromFRAM(AlgErrorState& s);

// ===============================
// FRAM CREDENTIALS SECTION
// (Used by programming mode)
// ===============================

// Forward declaration for credentials structure (defined in crypto/fram_encryption.h)
struct FRAMCredentials;

// Credentials management functions (conditional compilation)
bool readCredentialsFromFRAM(FRAMCredentials& creds);
bool writeCredentialsToFRAM(const FRAMCredentials& creds);
bool verifyCredentialsInFRAM();

#endif