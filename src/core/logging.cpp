#include "logging.h"
#include <stdarg.h>
#include <WiFi.h>
#include <lwip/sockets.h>

// ============================================================
// Network log (read-only TCP mirror of the log output)
// ============================================================
// Log lines are copied into a ring buffer (callable from any task, never
// blocks); handleNetworkLog() drains it to a single TCP client from loop()
// using non-blocking sends, so a stalled client can't stall the ATO loop.
// On connect the client first receives the buffered backlog.

namespace {
constexpr uint16_t NET_LOG_PORT     = 23;
constexpr size_t   NET_LOG_BUF_SIZE = 4096;
constexpr size_t   NET_LOG_CHUNK    = 512;   // max bytes sent per loop() call

WiFiServer netLogServer(NET_LOG_PORT);
WiFiClient netLogClient;
bool netLogStarted = false;

char netLogBuf[NET_LOG_BUF_SIZE];
uint32_t netLogHead = 0;   // total bytes ever written (monotonic, wraps safely)
uint32_t netLogTail = 0;   // next byte to send to the client
portMUX_TYPE netLogMux = portMUX_INITIALIZER_UNLOCKED;

void netLogAppend(const char* s, size_t len) {
    if (!netLogStarted) return;
    portENTER_CRITICAL(&netLogMux);
    for (size_t i = 0; i < len; i++) {
        netLogBuf[netLogHead % NET_LOG_BUF_SIZE] = s[i];
        netLogHead++;
    }
    // Client too slow — drop the oldest unsent bytes
    if (netLogHead - netLogTail > NET_LOG_BUF_SIZE) {
        netLogTail = netLogHead - NET_LOG_BUF_SIZE;
    }
    portEXIT_CRITICAL(&netLogMux);
}

void emitLine(const char* format, va_list args) {
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);

    char line[280];
    int len = snprintf(line, sizeof(line), "[%lu] %s\n", millis(), buffer);
    if (len < 0) return;
    if ((size_t)len >= sizeof(line)) len = sizeof(line) - 1;

    Serial.write((const uint8_t*)line, len);
    netLogAppend(line, len);
}
}  // namespace

void initLogging() {
#if ENABLE_SERIAL_DEBUG || ENABLE_FULL_LOGGING
    Serial.begin(115200);
    delay(1000);

    #if ENABLE_FULL_LOGGING
        LOG_INFO("Logging system initialized");
    #else
        Serial.println("Serial debug initialized (limited logging)");
    #endif
#endif
}

void startNetworkLog() {
    if (netLogStarted) return;
    netLogServer.begin();
    netLogServer.setNoDelay(true);
    netLogStarted = true;
    LOG_INFO("Network log listening on TCP port %u", NET_LOG_PORT);
}

void handleNetworkLog() {
    if (!netLogStarted) return;

    // New client replaces the old one (covers half-open connections
    // left behind e.g. after the laptop went to sleep)
    if (netLogServer.hasClient()) {
        WiFiClient incoming = netLogServer.accept();
        if (netLogClient) netLogClient.stop();
        netLogClient = incoming;
        portENTER_CRITICAL(&netLogMux);
        netLogTail = (netLogHead > NET_LOG_BUF_SIZE) ? netLogHead - NET_LOG_BUF_SIZE : 0;
        portEXIT_CRITICAL(&netLogMux);
        LOG_INFO("Network log client connected: %s", netLogClient.remoteIP().toString().c_str());
    }

    if (!netLogClient) return;
    if (!netLogClient.connected()) {
        netLogClient.stop();
        return;
    }

    char chunk[NET_LOG_CHUNK];
    size_t n = 0;
    portENTER_CRITICAL(&netLogMux);
    uint32_t from = netLogTail;
    while (n < NET_LOG_CHUNK && from + n != netLogHead) {
        chunk[n] = netLogBuf[(from + n) % NET_LOG_BUF_SIZE];
        n++;
    }
    portEXIT_CRITICAL(&netLogMux);
    if (n == 0) return;

    int sent = send(netLogClient.fd(), chunk, n, MSG_DONTWAIT);
    if (sent > 0) {
        portENTER_CRITICAL(&netLogMux);
        // Tail may have been pushed further by an overflow meanwhile
        uint32_t newTail = from + sent;
        if ((int32_t)(newTail - netLogTail) > 0) netLogTail = newTail;
        portEXIT_CRITICAL(&netLogMux);
    } else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        netLogClient.stop();
    }
}

void logInfo(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    va_list args;
    va_start(args, format);
    emitLine(format, args);
    va_end(args);
#endif
}

void logWarning(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    va_list args;
    va_start(args, format);
    emitLine(format, args);
    va_end(args);
#endif
}

void logError(const char* format, ...) {
#if ENABLE_FULL_LOGGING || ENABLE_SERIAL_DEBUG
    va_list args;
    va_start(args, format);
    emitLine(format, args);
    va_end(args);
#endif
}
