
#ifndef PROV_CONFIG_H
#define PROV_CONFIG_H

#include <Arduino.h>

// ===============================
// PROVISIONING MODE CONFIGURATION
// ===============================

// Button Configuration - DOZOWNIK
#define PROV_BUTTON_PIN         40      // GPIO40 zwierany do masy
#define PROV_BUTTON_HOLD_MS     5000    // Hold time to enter provisioning (5 seconds)
#define PROV_BUTTON_DEBOUNCE_MS 100     // Debounce time

// Audio/Visual feedback during button hold - DOZOWNIK
#define PROV_FEEDBACK_PIN       39      // GPIO39 buzzer (aktywny HIGH)
#define PROV_BEEP_HIGH_MS       10      // High state duration
#define PROV_BEEP_LOW_MS        100     // Low state duration
#define PROV_HOLD_MARGIN_MS     500     // Extra time margin for feedback

// Access Point Configuration - DOZOWNIK
#define PROV_AP_SSID            "DOZOWNIK-SETUP"
#define PROV_AP_PASSWORD        "setup12345"
#define PROV_AP_CHANNEL         6
#define PROV_AP_MAX_CLIENTS     4

// DNS Server Configuration
#define PROV_DNS_PORT           53

// Web Server Configuration
#define PROV_WEB_PORT           80

// Test Configuration
#define PROV_WIFI_TIMEOUT_MS    10000
#define PROV_WIFI_RETRY_COUNT   3
#define PROV_DNS_TIMEOUT_MS     10000
#define PROV_HTTP_TIMEOUT_MS    10000
#define PROV_WS_TIMEOUT_MS      15000

// Log Buffer Configuration
#define PROV_LOG_BUFFER_SIZE    50

// Session Configuration
#define PROV_SESSION_TIMEOUT_MS 600000  // 10 minutes

#endif