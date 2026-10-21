#include "level_event_log.h"
#include <time.h>
#include "fram_controller.h"
#include "rtc_controller.h"
#include "../core/logging.h"

namespace {

constexpr uint16_t LOG_MAGIC = 0x4C45;   // "LE"

struct LevelEvent {
    uint32_t utc;
    uint32_t uptime_s;
    char     text[LEVEL_EVENT_TEXT_LEN];
};
static_assert(sizeof(LevelEvent) == 128, "LevelEvent layout changed");

struct LogHeader {
    uint16_t magic;
    uint16_t total;      // events ever written (ring index = total % LEVEL_EVENT_COUNT)
    uint16_t checksum;   // magic ^ total
    uint8_t  pad[10];
};
static_assert(sizeof(LogHeader) == 16, "LogHeader layout changed");

constexpr uint16_t ADDR_HEADER = FRAM_ADDR_LEVEL_LOG;
constexpr uint16_t ADDR_EVENTS = FRAM_ADDR_LEVEL_LOG + sizeof(LogHeader);

LevelEvent   g_events[LEVEL_EVENT_COUNT];
uint16_t     g_total = 0;
portMUX_TYPE g_mux   = portMUX_INITIALIZER_UNLOCKED;   // RAM copy is read from the web task

void writeHeader() {
    LogHeader h = {};
    h.magic    = LOG_MAGIC;
    h.total    = g_total;
    h.checksum = LOG_MAGIC ^ g_total;
    framWriteRaw(ADDR_HEADER, (const uint8_t*)&h, sizeof(h));
}

}  // namespace

void initLevelEventLog() {
    LogHeader h = {};
    if (!framReadRaw(ADDR_HEADER, (uint8_t*)&h, sizeof(h)) ||
        h.magic != LOG_MAGIC || h.checksum != (uint16_t)(LOG_MAGIC ^ h.total)) {
        memset(g_events, 0, sizeof(g_events));
        g_total = 0;
        framWriteRaw(ADDR_EVENTS, (const uint8_t*)g_events, sizeof(g_events));
        writeHeader();
        LOG_INFO("Level event log: initialized empty");
        return;
    }
    framReadRaw(ADDR_EVENTS, (uint8_t*)g_events, sizeof(g_events));
    for (auto& e : g_events) e.text[LEVEL_EVENT_TEXT_LEN - 1] = '\0';
    g_total = h.total;
    LOG_INFO("Level event log: %u events stored", g_total < LEVEL_EVENT_COUNT ? g_total : LEVEL_EVENT_COUNT);
}

void addLevelEvent(const char* text) {
    LevelEvent e = {};
    e.utc      = (uint32_t)getUnixTimestamp();
    e.uptime_s = millis() / 1000;
    strlcpy(e.text, text, sizeof(e.text));

    uint16_t idx = g_total % LEVEL_EVENT_COUNT;
    portENTER_CRITICAL(&g_mux);
    g_events[idx] = e;
    g_total++;
    portEXIT_CRITICAL(&g_mux);

    framWriteRaw(ADDR_EVENTS + idx * sizeof(LevelEvent), (const uint8_t*)&e, sizeof(e));
    writeHeader();
}

String getLevelEventsText() {
    static LevelEvent copy[LEVEL_EVENT_COUNT];
    portENTER_CRITICAL(&g_mux);
    memcpy(copy, g_events, sizeof(copy));
    uint16_t total = g_total;
    portEXIT_CRITICAL(&g_mux);

    uint16_t n     = total < LEVEL_EVENT_COUNT ? total : LEVEL_EVENT_COUNT;
    uint16_t first = total - n;
    String out;
    out.reserve(n * 150 + 48);
    out += "Level sensor events: " + String(n) + " of " + String(total) + " total\n";
    for (uint16_t i = 0; i < n; i++) {
        const LevelEvent& e = copy[(first + i) % LEVEL_EVENT_COUNT];
        time_t ts = e.utc;
        struct tm t;
        localtime_r(&ts, &t);
        char line[180];
        size_t len = strftime(line, sizeof(line), "%Y-%m-%d %H:%M:%S", &t);
        snprintf(line + len, sizeof(line) - len, " (up %lu s) %s\n", (unsigned long)e.uptime_s, e.text);
        out += line;
    }
    return out;
}

void clearLevelEvents() {
    portENTER_CRITICAL(&g_mux);
    memset(g_events, 0, sizeof(g_events));
    g_total = 0;
    portEXIT_CRITICAL(&g_mux);
    writeHeader();
}
