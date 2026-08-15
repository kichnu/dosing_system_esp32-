#include "logging.h"
#include <WiFi.h>
#include <stdarg.h>

#define LOG_SERVER_PORT 8880

static WiFiServer g_logServer(LOG_SERVER_PORT);
static WiFiClient g_logClient;
static bool g_logServerStarted = false;

void startLogServer() {
    g_logServer.begin();
    g_logServerStarted = true;
    LOG_INFO("Log-socket: nasluchuje na porcie %d (pio device monitor --port socket://<ip>:%d)",
              LOG_SERVER_PORT, LOG_SERVER_PORT);
}

void updateLogServer() {
    if (!g_logServerStarted) return;

    if (g_logServer.hasClient()) {
        WiFiClient newClient = g_logServer.available();
        if (g_logClient.connected()) {
            newClient.stop();  // tylko jeden klient naraz — dev tool, LAN-only
        } else {
            g_logClient = newClient;
        }
    }
}

void stopLogServer() {
    if (g_logClient) {
        g_logClient.stop();
    }
    g_logServer.close();
    g_logServerStarted = false;
}

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

void logInfo(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] %s\n", millis(), buffer);
    if (g_logClient && g_logClient.connected()) {
        g_logClient.printf("[%lu] %s\n", millis(), buffer);
    }
#endif
}

void logWarning(const char* format, ...) {
#if ENABLE_FULL_LOGGING
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] %s\n", millis(), buffer);
    if (g_logClient && g_logClient.connected()) {
        g_logClient.printf("[%lu] %s\n", millis(), buffer);
    }
#endif
}

void logError(const char* format, ...) {
#if ENABLE_FULL_LOGGING || ENABLE_SERIAL_DEBUG
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.printf("[%lu] [ERROR] %s\n", millis(), buffer);
    if (g_logClient && g_logClient.connected()) {
        g_logClient.printf("[%lu] [ERROR] %s\n", millis(), buffer);
    }
#endif
}
