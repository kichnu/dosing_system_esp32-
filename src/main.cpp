/**
 * DOZOWNIK - Main Entry Point
 * 
 * Firmware testowy - FAZA 2
 * Testowanie RelayController i GpioValidator przez Serial.
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "dosing_types.h"
#include "fram_layout.h"
#include "relay_controller.h"
#include "gpio_validator.h"
#include "fram_controller.h"
#include "algorithm/channel_manager.h"
#include "rtc_controller.h"
#include "dosing_scheduler.h"

#include "web_server.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "provisioning/prov_detector.h"
#include "provisioning/ap_core.h"
#include "provisioning/ap_server.h"
#include "config/credentials_manager.h"
#include "hardware/safety_manager.h"




// ============================================================================
// TIMED PUMP TEST
// ============================================================================
void testTimedPump(uint8_t channel, uint32_t duration_ms) {
    Serial.printf("[TEST] Running CH%d for %lu ms\n", channel, duration_ms);
    
    RelayResult res = relayController.turnOn(channel, duration_ms);
    if (res != RelayResult::OK) {
        Serial.printf("[TEST] Failed to start: %s\n", 
                      RelayController::resultToString(res));
        return;
    }
    
    // Wait for completion
    while (relayController.isChannelOn(channel)) {
        relayController.update();
        
        // Print progress every 500ms
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 500) {
            lastPrint = millis();
            Serial.printf("[TEST] CH%d: %lu ms / %lu ms\n",
                          channel,
                          relayController.getActiveRuntime(),
                          duration_ms);
        }
        
        delay(50);
    }
    
    Serial.println(F("[TEST] Complete"));
}

// ============================================================================
// I2C SCAN
// ============================================================================
void i2cScan() {
    Serial.println(F("[I2C] Scanning bus..."));
    Serial.println(F("      ┌──────────────────────────────────────┐"));
    
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            found++;
            Serial.printf("      │  0x%02X - ", addr);
            
            if (addr == FRAM_I2C_ADDRESS) {
                Serial.print(F("FRAM MB85RC256V ✓"));
            } else if (addr == RTC_I2C_ADDRESS) {
                Serial.print(F("RTC DS3231M ✓"));
            } else {
                Serial.print(F("Unknown device"));
            }
            Serial.println(F("             │"));
        }
    }
    
    Serial.println(F("      └──────────────────────────────────────┘"));
    Serial.printf("[I2C] Found %d device(s)\n\n", found);
}

// ============================================================================
// FRAM TESTS
// ============================================================================
void testFRAM() {
    Serial.println(F("\n[FRAM TEST] Starting FRAM tests...\n"));
    
    if (!framController.isReady()) {
        Serial.println(F("[FRAM TEST] FRAM not ready!"));
        return;
    }
    
    // Test 1: Read header
    Serial.println(F("--- Test 1: Read Header ---"));
    FramHeader header;
    if (framController.readHeader(&header)) {
        Serial.printf("  Magic:   0x%08X %s\n", header.magic, 
                      header.magic == FRAM_MAGIC_NUMBER ? "(OK)" : "(INVALID)");
        Serial.printf("  Version: %d\n", header.layout_version);
        Serial.printf("  Channels: %d\n", header.channel_count);
    } else {
        Serial.println(F("  Failed to read header!"));
    }
    
    // Test 2: Read channel configs
    Serial.println(F("\n--- Test 2: Channel Configs ---"));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        ChannelConfig cfg;
        if (framController.readActiveConfig(i, &cfg)) {
            Serial.printf("  CH%d: events=0x%06X days=0x%02X dose=%.1f rate=%.2f\n",
                          i, cfg.events_bitmask, cfg.days_bitmask, 
                          cfg.daily_dose_ml, cfg.dosing_rate);
        }
    }
    
    // Test 3: Read system state
    Serial.println(F("\n--- Test 3: System State ---"));
    SystemState sys;
    if (framController.readSystemState(&sys)) {
        Serial.printf("  Enabled: %d\n", sys.system_enabled);
        Serial.printf("  Halted:  %d\n", sys.system_halted);
        Serial.printf("  Active:  %d\n", sys.active_channel);
        Serial.printf("  Boots:   %lu\n", sys.boot_count);
    }
    
    // Test 4: Write/Read test
    Serial.println(F("\n--- Test 4: Write/Read Test ---"));
    uint8_t testData[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78};
    uint8_t readBack[8] = {0};
    
    // Write to reserved area (safe for testing)
    uint16_t testAddr = FRAM_ADDR_RESERVED;
    
    if (framController.writeBytes(testAddr, testData, sizeof(testData))) {
        Serial.println(F("  Write OK"));
        
        if (framController.readBytes(testAddr, readBack, sizeof(readBack))) {
            bool match = memcmp(testData, readBack, sizeof(testData)) == 0;
            Serial.printf("  Read OK, data %s\n", match ? "MATCHES" : "MISMATCH!");
            
            if (!match) {
                Serial.print(F("  Expected: "));
                for (int i = 0; i < 8; i++) Serial.printf("%02X ", testData[i]);
                Serial.print(F("\n  Got:      "));
                for (int i = 0; i < 8; i++) Serial.printf("%02X ", readBack[i]);
                Serial.println();
            }
        }
    } else {
        Serial.println(F("  Write FAILED!"));
    }
    
    // Test 5: Dump header section
    Serial.println(F("\n--- Test 5: Header Dump ---"));
    framController.dumpSection(FRAM_ADDR_HEADER, 32);
    
    Serial.println(F("\n[FRAM TEST] Complete\n"));
}

// ============================================================================
// CHANNEL MANAGER TESTS
// ============================================================================
void testChannelManager() {
    Serial.println(F("\n[CH TEST] Channel Manager Test Menu"));
    Serial.println(F("  1 - Print all channels"));
    Serial.println(F("  2 - Print single channel detail"));
    Serial.println(F("  3 - Set test config on CH0"));
    Serial.println(F("  4 - Apply pending changes"));
    Serial.println(F("  5 - Simulate dosing event"));
    Serial.println(F("  6 - Reset daily states"));
    Serial.println(F("  0 - Exit"));
    
    while (true) {
        Serial.print(F("\n[CH TEST] > "));
        while (!Serial.available()) delay(10);
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();
        Serial.println(cmd);
        
        switch (cmd) {
            case '1':
                channelManager.printAllChannels();
                break;
                
            case '2': {
                Serial.print(F("Channel (0-5): "));
                while (!Serial.available()) delay(10);
                uint8_t ch = Serial.read() - '0';
                while (Serial.available()) Serial.read();
                Serial.println(ch);
                
                if (ch < CHANNEL_COUNT) {
                    channelManager.printChannelInfo(ch);
                }
                break;
            }
            
            case '3': {
                Serial.println(F("\nSetting test config on CH0:"));
                Serial.println(F("  Events: hours 8,12,18 (0x00044100)"));
                Serial.println(F("  Days: Mon-Fri (0x1F)"));
                Serial.println(F("  Daily dose: 6.0 ml"));
                Serial.println(F("  Rate: 0.5 ml/s"));
                
                // Set events: bits 8, 12, 18
                uint32_t events = (1 << 8) | (1 << 12) | (1 << 18);
                channelManager.setEventsBitmask(0, events);
                
                // Set days: Mon-Fri (bits 0-4)
                channelManager.setDaysBitmask(0, 0x1F);
                
                // Set dose
                channelManager.setDailyDose(0, 6.0);
                
                // Set rate
                channelManager.setDosingRate(0, 0.5);
                
                // Enable
                channelManager.setEnabled(0, true);
                
                Serial.println(F("Done! Config saved as PENDING."));
                Serial.println(F("Use '4' to apply or check with '2'."));
                
                channelManager.printChannelInfo(0);
                break;
            }
            
            case '4': {
                Serial.println(F("\nApplying all pending changes..."));
                if (channelManager.applyAllPendingChanges()) {
                    Serial.println(F("Success!"));
                } else {
                    Serial.println(F("Failed!"));
                }
                channelManager.printAllChannels();
                break;
            }
            
            case '5': {
                Serial.print(F("Channel (0-5): "));
                while (!Serial.available()) delay(10);
                uint8_t ch = Serial.read() - '0';
                while (Serial.available()) Serial.read();
                Serial.println(ch);
                
                if (ch < CHANNEL_COUNT) {
                    const ChannelCalculated& calc = channelManager.getCalculated(ch);
                    float dose = calc.single_dose_ml;
                    
                    Serial.printf("Simulating dose of %.2f ml on CH%d at hour 12\n", dose, ch);
                    
                    if (channelManager.markEventCompleted(ch, 12, dose)) {
                        Serial.println(F("Event marked complete!"));
                        channelManager.printChannelInfo(ch);
                    } else {
                        Serial.println(F("Failed!"));
                    }
                }
                break;
            }
            
            case '6':
                Serial.println(F("\nResetting daily states..."));
                if (channelManager.resetDailyStates()) {
                    Serial.println(F("Success!"));
                } else {
                    Serial.println(F("Failed!"));
                }
                break;
                
            case '0':
                Serial.println(F("Exiting test menu"));
                return;
                
            default:
                Serial.println(F("Unknown option"));
                break;
        }
    }
}

// ============================================================================
// RTC TESTS
// ============================================================================
void testRTC() {
    Serial.println(F("\n[RTC TEST] RTC Test Menu"));
    Serial.println(F("  1 - Print current time"));
    Serial.println(F("  2 - Set time manually"));
    Serial.println(F("  3 - Test midnight detection"));
    Serial.println(F("  0 - Exit"));
    
    while (true) {
        Serial.print(F("\n[RTC TEST] > "));
        while (!Serial.available()) delay(10);
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();
        Serial.println(cmd);
        
        switch (cmd) {
            case '1':
                rtcController.printTime();
                break;
                
            case '2': {
                Serial.println(F("\nSet time (UTC) - enter each value:"));
                
                Serial.print(F("Year (2025): "));
                while (!Serial.available()) delay(10);
                int year = Serial.parseInt();
                while (Serial.available()) Serial.read();
                Serial.println(year);
                
                Serial.print(F("Month (1-12): "));
                while (!Serial.available()) delay(10);
                int month = Serial.parseInt();
                while (Serial.available()) Serial.read();
                Serial.println(month);
                
                Serial.print(F("Day (1-31): "));
                while (!Serial.available()) delay(10);
                int day = Serial.parseInt();
                while (Serial.available()) Serial.read();
                Serial.println(day);
                
                Serial.print(F("Hour (0-23): "));
                while (!Serial.available()) delay(10);
                int hour = Serial.parseInt();
                while (Serial.available()) Serial.read();
                Serial.println(hour);
                
                Serial.print(F("Minute (0-59): "));
                while (!Serial.available()) delay(10);
                int minute = Serial.parseInt();
                while (Serial.available()) Serial.read();
                Serial.println(minute);
                
                TimeInfo t;
                t.year = year;
                t.month = month;
                t.day = day;
                t.hour = hour;
                t.minute = minute;
                t.second = 0;
                t.dayOfWeek = 0;
                
                Serial.printf("\nSetting: %04d-%02d-%02d %02d:%02d:00\n",
                              year, month, day, hour, minute);
                
                if (rtcController.setTime(t)) {
                    Serial.println(F("Time set OK!"));
                    rtcController.printTime();
                } else {
                    Serial.println(F("Failed!"));
                }
                break;
            }
            
            case '3': {
                Serial.println(F("\nMidnight detection:"));
                Serial.printf("  hasMidnightPassed(): %s\n", 
                              rtcController.hasMidnightPassed() ? "YES" : "NO");
                break;
            }
                
            case '0':
                Serial.println(F("Exiting"));
                return;
                
            default:
                Serial.println(F("Unknown option"));
                break;
        }
    }
}


// ============================================================================
// SCHEDULER TESTS
// ============================================================================
void testScheduler() {
    Serial.println(F("\n[SCHED TEST] Scheduler Test Menu"));
    Serial.println(F("  1 - Print scheduler status"));
    Serial.println(F("  2 - Enable scheduler"));
    Serial.println(F("  3 - Disable scheduler"));
    Serial.println(F("  4 - Trigger manual dose"));
    Serial.println(F("  5 - Force daily reset"));
    Serial.println(F("  6 - Setup quick test (CH0, 1 event now)"));
    Serial.println(F("  0 - Exit"));
    
    while (true) {
        Serial.print(F("\n[SCHED TEST] > "));
        while (!Serial.available()) delay(10);
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();
        Serial.println(cmd);
        
        switch (cmd) {
            case '1':
                dosingScheduler.printStatus();
                channelManager.printAllChannels();
                rtcController.printTime();
                break;
                
            case '2':
                dosingScheduler.setEnabled(true);
                Serial.println(F("Scheduler ENABLED"));
                break;
                
            case '3':
                dosingScheduler.setEnabled(false);
                Serial.println(F("Scheduler DISABLED"));
                break;
                
            case '4': {
                Serial.print(F("Channel (0-5): "));
                while (!Serial.available()) delay(10);
                uint8_t ch = Serial.read() - '0';
                while (Serial.available()) Serial.read();
                Serial.println(ch);
                
                if (ch < CHANNEL_COUNT) {
                    if (dosingScheduler.triggerManualDose(ch)) {
                        Serial.println(F("Manual dose started!"));
                        
                        // Wait and show progress
                        SchedulerState state;
                        while ((state = dosingScheduler.getState()) == SchedulerState::VALIDATING ||
                               state == SchedulerState::DOSING ||
                               state == SchedulerState::WAITING_PUMP) {
                            dosingScheduler.update();
                            relayController.update();
                            gpioValidator.update();
                            
                            if (state == SchedulerState::VALIDATING) {
                                Serial.println(F("  Validating GPIO..."));
                            } else if (relayController.isAnyOn()) {
                                Serial.printf("  Pump running: %lu ms\n", 
                                              relayController.getActiveRuntime());
                            }
                            delay(100);  // Faster update for validation
                        }
                        
                        if (dosingScheduler.getCurrentEvent().failed) {
                            Serial.println(F("Dose FAILED!"));
                        } else {
                            Serial.println(F("Dose complete!"));
                        }
                    } else {
                        Serial.println(F("Failed to start dose"));
                    }
                }
                break;
            }
            
            case '5':
                Serial.println(F("Forcing daily reset..."));
                dosingScheduler.forceDailyReset();
                Serial.println(F("Done!"));
                break;
                
            case '6': {
                Serial.println(F("\nQuick test setup for CH0:"));
                
                // Get current time
                TimeInfo now = rtcController.getTime();
                uint8_t nextHour = now.hour;
                if (now.minute > 50) {
                    nextHour = (now.hour + 1) % 24;
                }
                
                // Skip hour 0
                if (nextHour == 0) nextHour = 1;
                
                Serial.printf("  Setting event at hour %d\n", nextHour);
                Serial.printf("  Current day of week: %d\n", now.dayOfWeek);
                
                // Set single event at next hour
                uint32_t events = (1UL << nextHour);
                channelManager.setEventsBitmask(0, events);
                
                // Set current day only
                uint8_t days = (1 << now.dayOfWeek);
                channelManager.setDaysBitmask(0, days);
                
                // Set small dose for testing
                channelManager.setDailyDose(0, 2.0);  // 2ml
                channelManager.setDosingRate(0, 0.5); // 0.5 ml/s = 4 sec pump
                channelManager.setEnabled(0, true);
                
                // Apply immediately (bypass pending)
                channelManager.applyPendingChanges(0);
                
                Serial.println(F("\nConfig applied:"));
                channelManager.printChannelInfo(0);
                
                Serial.println(F("\nEnable scheduler with '2' to activate!"));
                break;
            }
                
            case '0':
                Serial.println(F("Exiting"));
                return;
                
            default:
                Serial.println(F("Unknown option"));
                break;
        }
    }
}

// ============================================================================
// GPIO TIMING MEASUREMENT
// ============================================================================
void measureGpioTiming(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) {
        Serial.println(F("[MEASURE] Invalid channel"));
        return;
    }
    
    Serial.printf("\n[MEASURE] === GPIO Timing Test CH%d ===\n", channel);
    Serial.printf("[MEASURE] Relay pin: GPIO%d\n", RELAY_PINS[channel]);
    Serial.printf("[MEASURE] Validate pin: GPIO%d\n", VALIDATE_PINS[channel]);
    
    // Read initial state
    bool initialState = digitalRead(VALIDATE_PINS[channel]);
    Serial.printf("[MEASURE] Initial validate state: %s\n", initialState ? "HIGH" : "LOW");
    
    // Check if pump already running
    if (relayController.isAnyOn()) {
        Serial.println(F("[MEASURE] ERROR: Another pump running!"));
        return;
    }
    
    Serial.println(F("[MEASURE] Starting relay..."));
    
    // Turn on relay
    uint32_t startTime = micros();
    RelayResult res = relayController.turnOn(channel, 10000); // 10s max
    
    if (res != RelayResult::OK) {
        Serial.printf("[MEASURE] Failed to start: %s\n", RelayController::resultToString(res));
        return;
    }
    
    // Poll for state change
    const uint32_t TIMEOUT_MS = 5000;  // 5 second timeout
    const uint32_t POLL_INTERVAL_US = 100;  // Poll every 100µs
    
    uint32_t changeTime = 0;
    bool stateChanged = false;
    bool expectedState = !initialState;  // Oczekujemy zmiany stanu
    
    Serial.printf("[MEASURE] Waiting for validate pin to go %s...\n", 
                  expectedState ? "HIGH" : "LOW");
    
    while ((micros() - startTime) < (TIMEOUT_MS * 1000UL)) {
        bool currentState = digitalRead(VALIDATE_PINS[channel]);
        
        if (currentState == expectedState) {
            changeTime = micros() - startTime;
            stateChanged = true;
            break;
        }
        
        delayMicroseconds(POLL_INTERVAL_US);
    }
    
    // Turn off relay
    relayController.turnOff(channel);
    
    // Report results
    Serial.println(F("\n[MEASURE] === RESULTS ==="));
    
    if (stateChanged) {
        float timeMs = changeTime / 1000.0f;
        Serial.printf("[MEASURE] State changed after: %.2f ms (%lu µs)\n", timeMs, changeTime);
        Serial.printf("[MEASURE] Current GPIO_CHECK_DELAY_MS: %d ms\n", GPIO_CHECK_DELAY_MS);
        
        if (timeMs < GPIO_CHECK_DELAY_MS) {
            Serial.printf("[MEASURE] OK - delay is sufficient (margin: %.1f ms)\n", 
                          GPIO_CHECK_DELAY_MS - timeMs);
        } else {
            Serial.printf("[MEASURE] WARNING - delay too short! Increase by %.1f ms\n",
                          timeMs - GPIO_CHECK_DELAY_MS + 500);
        }
    } else {
        Serial.println(F("[MEASURE] TIMEOUT - no state change detected!"));
        Serial.println(F("[MEASURE] Check wiring or validate pin configuration"));
    }
    
    // Final state
    bool finalState = digitalRead(VALIDATE_PINS[channel]);
    Serial.printf("[MEASURE] Final validate state: %s\n", finalState ? "HIGH" : "LOW");
    Serial.println();
}

// ============================================================================
// CONFIG STRUCT SIZES
// ============================================================================
void printChannelConfigSize() {
    Serial.println(F("[CONFIG] Structure sizes:"));
    Serial.println(F("┌─────────────────────────────────────────┐"));
    Serial.printf("│  ChannelConfig:      %3d bytes          │\n", sizeof(ChannelConfig));
    Serial.printf("│  ChannelDailyState:  %3d bytes          │\n", sizeof(ChannelDailyState));
    Serial.printf("│  SystemState:        %3d bytes          │\n", sizeof(SystemState));
    Serial.printf("│  ErrorState:         %3d bytes          │\n", sizeof(ErrorState));
    Serial.printf("│  FramHeader:         %3d bytes          │\n", sizeof(FramHeader));
    Serial.printf("│  AuthData:           %3d bytes          │\n", sizeof(AuthData));
    Serial.println(F("├─────────────────────────────────────────┤"));
    Serial.printf("│  RelayState:         %3d bytes          │\n", sizeof(RelayState));
    Serial.println(F("└─────────────────────────────────────────┘"));
    Serial.println();
}

// ============================================================================
// GLOBAL STATE
// ============================================================================
bool systemHalted = false;
bool pumpGlobalEnabled = true;
bool gpioValidationEnabled = GPIO_VALIDATION_DEFAULT;
InitStatus initStatus;

void printSystemInfo();
// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
#if ENABLE_CLI
void printBanner();
void printMenu();
void processSerialCommand();
void i2cScan();
void printChannelConfigSize();
void testTimedPump(uint8_t channel, uint32_t duration_ms);
void testFRAM();
void testChannelManager();
void testRTC();
void testScheduler();
void measureGpioTiming(uint8_t channel);
#endif

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * Inicjalizacja hardware: I2C, FRAM, RTC, Relay
 */
void initHardware() {
    Serial.println(F("\n[INIT] === HARDWARE INIT ==="));
    
    // --- I2C ---
    Serial.print(F("[INIT] I2C... "));
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);
    initStatus.i2c_ok = true;  // Wire.begin() nie zwraca błędu
    Serial.printf("OK (SDA=%d, SCL=%d)\n", I2C_SDA_PIN, I2C_SCL_PIN);
    
    // --- FRAM ---
    Serial.print(F("[INIT] FRAM... "));
    if (framController.begin()) {
        initStatus.fram_ok = true;
        Serial.println(F("OK"));
    } else {
        initStatus.fram_ok = false;
        Serial.println(F("FAILED!"));
    }
    
        // --- Credentials ---
    Serial.print(F("[INIT] Credentials... "));
    if (initStatus.fram_ok) {
        if (initCredentialsManager()) {
            Serial.println(F("OK (from FRAM)"));
        } else {
            Serial.println(F("using fallback"));
        }
    } else {
        Serial.println(F("SKIPPED (no FRAM)"));
    }
// --- RTC ---
    Serial.print(F("[INIT] RTC... "));
    if (rtcController.begin()) {
        delay(50);  // Daj RTC chwilę na stabilizację
        if (rtcController.isTimeValid()) {
            initStatus.rtc_ok = true;
            TimeInfo now = rtcController.getTime();
            Serial.printf("OK (%04d-%02d-%02d %02d:%02d)\n", 
                          now.year, now.month, now.day, now.hour, now.minute);
        } else {
            initStatus.rtc_ok = false;
            Serial.println(F("INVALID TIME!"));
        }
    } else {
        initStatus.rtc_ok = false;
        Serial.println(F("FAILED!"));
    }
    
    // --- Relay Controller ---
    Serial.print(F("[INIT] Relays... "));
    relayController.begin();
    initStatus.relays_ok = true;  // begin() nie zwraca błędu
    Serial.println(F("OK"));
    
    // --- GPIO Validator ---
    Serial.print(F("[INIT] GPIO Validator... "));
    gpioValidator.begin();
    Serial.println(F("OK"));
    
    // --- Hardware summary ---
    initStatus.critical_ok = initStatus.fram_ok && initStatus.rtc_ok;
    
    if (initStatus.isHardwareOk()) {
        Serial.println(F("[INIT] Hardware: ALL OK"));
    } else {
        Serial.println(F("[INIT] Hardware: ERRORS DETECTED!"));
        if (!initStatus.fram_ok) Serial.println(F("  - FRAM: CRITICAL!"));
        if (!initStatus.rtc_ok)  Serial.println(F("  - RTC: CRITICAL!"));
    }
}

/**
 * Inicjalizacja sieci: WiFi, WebServer
 */
void initNetwork() {
    Serial.println(F("\n[INIT] === NETWORK INIT ==="));
    
    // --- WiFi ---
    Serial.print(F("[INIT] WiFi... "));
    
    // TODO: Docelowo credentials z FRAM/Captive Portal
    // WiFi.begin("KiG_2.4_IOT", "*qY4I@5&*%0lK1Q$U6UV7^S");

        // Use credentials from FRAM or fallback
    const char* ssid = getWiFiSSID();
    const char* password = getWiFiPassword();
    
    Serial.printf("connecting to %s (%s)... ", 
                  ssid, 
                  areCredentialsLoaded() ? "FRAM" : "fallback");
    
    WiFi.begin(ssid, password);
    
    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - wifiStart > WIFI_CONNECT_TIMEOUT_MS) {
            break;
        }
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        initStatus.wifi_ok = true;
        Serial.printf(" OK (%s)\n", WiFi.localIP().toString().c_str());
    } else {
        initStatus.wifi_ok = false;
        Serial.println(F(" FAILED!"));
    }
    
    // --- WebServer (only if WiFi OK) ---
    if (initStatus.wifi_ok) {
        Serial.print(F("[INIT] WebServer... "));
        initWebServer();
        initStatus.webserver_ok = true;
        Serial.println(F("OK"));
    } else {
        initStatus.webserver_ok = false;
        Serial.println(F("[INIT] WebServer: SKIPPED (no WiFi)"));
    }
    
    // --- Network summary ---
    if (initStatus.wifi_ok && initStatus.webserver_ok) {
        Serial.println(F("[INIT] Network: ALL OK"));
        Serial.printf("[INIT] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(F("[INIT] Network: DEGRADED MODE"));
        Serial.println(F("  - Dosing will work, but no remote access"));
    }
}

/**
 * Inicjalizacja aplikacji: ChannelManager, Scheduler
 */
void initApplication() {
    Serial.println(F("\n[INIT] === APPLICATION INIT ==="));
    
    // --- Channel Manager (wymaga FRAM) ---
    Serial.print(F("[INIT] Channel Manager... "));
    if (initStatus.fram_ok) {
        if (channelManager.begin()) {
            initStatus.channel_manager_ok = true;
            Serial.println(F("OK"));
        } else {
            initStatus.channel_manager_ok = false;
            Serial.println(F("FAILED!"));
        }
    } else {
        initStatus.channel_manager_ok = false;
        Serial.println(F("SKIPPED (no FRAM)"));
    }
    
    // --- Dosing Scheduler (wymaga RTC + ChannelManager) ---
    Serial.print(F("[INIT] Scheduler... "));
    if (initStatus.rtc_ok && initStatus.channel_manager_ok) {
        if (dosingScheduler.begin()) {
            initStatus.scheduler_ok = true;
            Serial.println(F("OK"));
        } else {
            initStatus.scheduler_ok = false;
            Serial.println(F("FAILED!"));
        }
    } else {
        initStatus.scheduler_ok = false;
        Serial.println(F("SKIPPED (missing dependencies)"));
        if (!initStatus.rtc_ok) Serial.println(F("  - Requires: RTC"));
        if (!initStatus.channel_manager_ok) Serial.println(F("  - Requires: Channel Manager"));
    }

    // --- NTP Sync (wymaga WiFi + RTC) ---
    if (initStatus.wifi_ok && initStatus.rtc_ok) {
        Serial.print(F("[INIT] NTP Sync... "));
        if (rtcController.syncNTPWithRetry()) {
                // Synchronizuj stan schedulera z nowym czasem
        dosingScheduler.syncTimeState();
            Serial.println(F("OK"));
        } else {
            Serial.println(F("FAILED (will retry later)"));
        }
    } else {
        Serial.println(F("[INIT] NTP Sync... SKIPPED (missing WiFi or RTC)"));
    }
    
    // --- Application summary ---
    if (initStatus.isApplicationOk()) {
        Serial.println(F("[INIT] Application: ALL OK"));
    } else {
        Serial.println(F("[INIT] Application: ERRORS!"));
        Serial.println(F("  - Dosing may not work correctly"));
    }
    
    // --- Final system status ---
    initStatus.system_ready = initStatus.isHardwareOk() && initStatus.isApplicationOk();
    
    Serial.println(F("\n[INIT] =============================="));
    if (initStatus.system_ready) {
        Serial.println(F("[INIT] SYSTEM READY"));
    } else {
        Serial.println(F("[INIT] SYSTEM DEGRADED"));
        systemHalted = !initStatus.critical_ok;
        if (systemHalted) {
            Serial.println(F("[INIT] CRITICAL ERROR - SYSTEM HALTED!"));
        }
    }
    Serial.println(F("[INIT] ==============================\n"));
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize Serial
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Wait for Serial (USB CDC)
    uint32_t startWait = millis();
    while (!Serial && (millis() - startWait < 3000)) {
        delay(10);
    }
    delay(500);

     // === CHECK PROVISIONING BUTTON (before any other init) ===
    if (checkProvisioningButton()) {
        Serial.println(F("\n"));
        Serial.println(F("╔═══════════════════════════════════════╗"));
        Serial.println(F("║     ENTERING PROVISIONING MODE        ║"));
        Serial.println(F("╚═══════════════════════════════════════╝"));
        
        // Minimal init for provisioning
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        delay(100);
        framController.begin();
        
        // Start AP + Web Server
        if (startAccessPoint() && startDNSServer() && startWebServer()) {
            Serial.println(F("[PROV] Configuration portal ready!"));
            Serial.println(F("[PROV] Connect to WiFi: DOZOWNIK-SETUP"));
            Serial.println(F("[PROV] Password: setup12345"));
            Serial.println(F("[PROV] Open: http://192.168.4.1"));
            
            // Blocking loop - never returns
            runProvisioningLoop();
        } else {
            Serial.println(F("[PROV] Failed to start provisioning!"));
            Serial.println(F("[PROV] Rebooting in 5 seconds..."));
            delay(5000);
            ESP.restart();
        }
    }

    
    // Reset init status
    initStatus.reset();
    
    #if ENABLE_CLI
    printBanner();
    #endif

    Serial.println(F("\n"));
    Serial.println(F("╔══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║              DOZOWNIK v2.0 - Starting...                 ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════╝"));
    
    // === INITIALIZATION SEQUENCE ===
    initHardware();
    initNetwork();
    initApplication();

    safetyManager.begin();
    
        if (!safetyManager.enableIfSafe()) {
        // Błąd krytyczny aktywny - system zablokowany
        // Wejdź w pętlę oczekiwania na reset
        Serial.println(F("[MAIN] System locked due to critical error"));
        Serial.println(F("[MAIN] Only reset button will be handled"));
        
        while (safetyManager.isCriticalErrorActive()) {
            safetyManager.update();  // Obsługa buzzera i przycisku
            delay(10);
        }
        
        // Po resecie - restart systemu
        Serial.println(F("[MAIN] Error cleared - restarting..."));
        delay(1000);
        ESP.restart();
    }

    // === POST-INIT ===
    #if ENABLE_CLI
    i2cScan();
    printMenu();
    #endif

// === WATCHDOG TIMER (production only) ===
    #if !ENABLE_CLI
    Serial.print(F("[INIT] Watchdog Timer... "));
    if (esp_task_wdt_add(NULL) == ESP_OK) {
        Serial.println(F("OK (subscribed to default WDT)"));
    } else {
        Serial.println(F("SKIPPED"));
    }
    #else
    Serial.println(F("[INIT] Watchdog Timer... DISABLED (debug mode)"));
    #endif
    
    // Final status
    if (initStatus.system_ready) {
        Serial.println(F("[MAIN] Entering main loop..."));
    } else if (systemHalted) {
        Serial.println(F("[MAIN] System halted - check errors above"));
    } else {
        Serial.println(F("[MAIN] Running in degraded mode"));
    }
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // === CRITICAL: Always update relay (safety) ===
    safetyManager.update();
    relayController.update();

    if (safetyManager.isCriticalErrorActive()) {
        // Zatrzymaj wszelkie operacje
        // (relay_controller powinien już być zatrzymany przez triggerCriticalError)
        return;
    }


 
    // === FEED WATCHDOG (if subscribed) ===
    static bool wdtSubscribed = false;
    static bool wdtChecked = false;
    if (!wdtChecked) {
        wdtSubscribed = (esp_task_wdt_status(NULL) == ESP_OK);
        wdtChecked = true;
    }
    if (wdtSubscribed) {
        // === FEED WATCHDOG (production only) ===
        #if !ENABLE_CLI
            esp_task_wdt_reset();
        #endif
    }
    
    // === Skip processing if system halted ===
    if (systemHalted) {
        static uint32_t lastHaltMsg = 0;
        if (millis() - lastHaltMsg > 30000) {  // Co 30s przypomnienie
            lastHaltMsg = millis();
            Serial.println(F("[MAIN] System HALTED - restart required"));
        }
        delay(100);
        return;
    }
    
    // === Normal operation ===
    
    // Update GPIO validator
        // Update GPIO validator
    gpioValidator.update();
    
    // === NTP Resync (hourly) ===
    static uint32_t lastNtpCheck = 0;
    if (millis() - lastNtpCheck > 60000) {  // Check every minute
        lastNtpCheck = millis();
        
        if (initStatus.wifi_ok && initStatus.rtc_ok && rtcController.needsResync()) {
            Serial.println(F("[MAIN] NTP resync due..."));
            rtcController.syncNTPWithRetry();
        }
    }
    
    // Update scheduler (main dosing logic)
    if (initStatus.scheduler_ok) {
        dosingScheduler.update();
    }
    
    // === CLI (debug only) ===
    #if ENABLE_CLI
    if (Serial.available()) {
        processSerialCommand();
    }
    
    // Show pump status when running
    static uint32_t lastStatusPrint = 0;
    if (relayController.isAnyOn() && (millis() - lastStatusPrint > 1000)) {
        lastStatusPrint = millis();
        uint8_t ch = relayController.getActiveChannel();
        uint32_t runtime = relayController.getActiveRuntime();
        uint32_t remaining = relayController.getRemainingTime();
        Serial.printf("[STATUS] CH%d running: %lu ms (remaining: %lu ms)\n",
                      ch, runtime, remaining);
    }
    #endif
    
    // === Heartbeat (production) ===
    #if !ENABLE_CLI
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 60000) {  // Co 1 minutę
        lastHeartbeat = millis();
        Serial.printf("[HEARTBEAT] Uptime: %lu min, Scheduler: %s\n",
                      millis() / 60000,
                      dosingScheduler.isEnabled() ? "ON" : "OFF");
    }
    #endif
    
    delay(10);
}

// ============================================================================
// BANNER & MENU
// ============================================================================
void printBanner() {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║          DOZOWNIK - Automatic Fertilizer Dosing System        ║"));
    Serial.println(F("║                    FAZA 2 - Controller Test                   ║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));
    Serial.printf("║  Firmware: %-20s  Channels: %-14d  ║\n", FIRMWARE_VERSION, CHANNEL_COUNT);
    Serial.printf("║  Device:   %-20s  Build: %s %-5s  ║\n", DEVICE_ID, __DATE__, "");
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.println();
}

void printMenu() {
    Serial.println(F("┌─────────────────────────────────────────────────────────────────┐"));
    Serial.println(F("│                    FAZA 2 - CONTROLLER TEST                     │"));
    Serial.println(F("├─────────────────────────────────────────────────────────────────┤"));
    Serial.println(F("│  RELAY CONTROLLER:                                              │"));
    Serial.println(F("│    0-5   Toggle relay CH0-CH5 (with mutex)                      │"));
    Serial.println(F("│    t     Timed pump test (3s on selected channel)               │"));
    Serial.println(F("│    a     All relays ON (blocked by mutex!)                      │"));
    Serial.println(F("│    o     All relays OFF (emergency stop)                        │"));
    Serial.println(F("│    p     Print relay status                                     │"));
    Serial.println(F("├─────────────────────────────────────────────────────────────────┤"));
    Serial.println(F("│  GPIO VALIDATOR:                                                │"));
    Serial.println(F("│    g     Read all GPIO states                                   │"));
    Serial.println(F("│    v     Run validation on active channel                       │"));
    Serial.println(F("├─────────────────────────────────────────────────────────────────┤"));
    Serial.println(F("│  SYSTEM:                                                        │"));
    Serial.println(F("│    i     I2C scan                                               │"));
    Serial.println(F("│    s     System info                                            │"));
    Serial.println(F("│    c     Config struct sizes                                    │"));
    Serial.println(F("│    h     This help menu                                         │"));
    Serial.println(F("│    e     Toggle emergency halt                                  │"));
    Serial.println(F("│    x     Reboot                                                 │"));
    Serial.println(F("│    f     FRAM test (read all sections)                          │"));
    Serial.println(F("│    r     Factory reset FRAM                                     │"));
    Serial.println(F("│    m     Channel Manager test menu                              │"));
    Serial.println(F("│    w     RTC test menu (time/date)                              │"));
    Serial.println(F("│    d     Dosing scheduler test menu                             │"));
    Serial.println(F("│    y     Toggle GPIO validation                                 │"));
    Serial.println(F("│    z     Measure GPIO relay->validate timing                    │"));
    Serial.println(F("└─────────────────────────────────────────────────────────────────┘"));
    Serial.println();
}

// ============================================================================
// SERIAL COMMAND PROCESSOR
// ============================================================================
void processSerialCommand() {
    char cmd = Serial.read();
    
    // Flush remaining input
    while (Serial.available()) Serial.read();
    
    Serial.println();
    
    switch (cmd) {
        // --- Relay commands ---
        case '0': case '1': case '2': case '3': case '4': case '5': {
            uint8_t ch = cmd - '0';
            if (ch < CHANNEL_COUNT) {
                if (relayController.isChannelOn(ch)) {
                    RelayResult res = relayController.turnOff(ch);
                    Serial.printf("[CMD] CH%d OFF -> %s\n", ch, 
                                  RelayController::resultToString(res));
                } else {
                    RelayResult res = relayController.turnOn(ch, 3000); // 30s max
                    Serial.printf("[CMD] CH%d ON (30s max) -> %s\n", ch,
                                  RelayController::resultToString(res));
                }
            }
            break;
        }
        
        case 't':
        case 'T': {
            Serial.println(F("[CMD] Timed pump test"));
            Serial.print(F("      Enter channel (0-5): "));
            
            // Wait for input
            while (!Serial.available()) delay(10);
            char chChar = Serial.read();
            while (Serial.available()) Serial.read();
            
            uint8_t ch = chChar - '0';
            if (ch < CHANNEL_COUNT) {
                Serial.printf("%d\n", ch);
                testTimedPump(ch, 3000); // 3 second test
            } else {
                Serial.println(F("Invalid channel"));
            }
            break;
        }
        
        case 'a':
        case 'A':
            Serial.println(F("[CMD] Trying to turn ALL ON (mutex should block)"));
            for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
                RelayResult res = relayController.turnOn(i, 5000);
                Serial.printf("       CH%d -> %s\n", i, 
                              RelayController::resultToString(res));
            }
            break;
            
        case 'o':
        case 'O':
            Serial.println(F("[CMD] All OFF"));
            relayController.allOff();
            break;
            
        case 'p':
        case 'P':
            relayController.printStatus();
            break;
            
        // --- GPIO commands ---
        case 'g':
        case 'G':
            gpioValidator.printAllGpio();
            break;
            
        case 'v':
        case 'V': {
            uint8_t active = relayController.getActiveChannel();
            if (active < CHANNEL_COUNT) {
                Serial.printf("[CMD] Starting validation for CH%d\n", active);
                gpioValidator.startValidation(active);
                
                // Wait for result
                while (gpioValidator.isValidating()) {
                    gpioValidator.update();
                    delay(100);
                }
                
                ValidationResult res = gpioValidator.getLastResult();
                Serial.printf("[CMD] Validation result: %s\n",
                              GpioValidator::resultToString(res));
            } else {
                Serial.println(F("[CMD] No pump active - turn one on first"));
            }
            break;
        }

        case 'f':
        case 'F':
            testFRAM();
            break;
            
        case 'r':
        case 'R': {
            Serial.println(F("[CMD] Factory reset FRAM? (y/n): "));
            while (!Serial.available()) delay(10);
            char confirm = Serial.read();
            while (Serial.available()) Serial.read();
            
            if (confirm == 'y' || confirm == 'Y') {
                Serial.println(F("[CMD] Resetting FRAM..."));
                if (framController.factoryReset()) {
                    Serial.println(F("[CMD] Factory reset complete"));
                } else {
                    Serial.println(F("[CMD] Factory reset FAILED!"));
                }
            } else {
                Serial.println(F("[CMD] Cancelled"));
            }
            break;
        }
            
        // --- System commands ---
        case 'i':
        case 'I':
            i2cScan();
            break;
            
        case 's':
        case 'S':
            printSystemInfo();
            break;
            
        case 'c':
        case 'C':
            printChannelConfigSize();
            break;
            
        case 'e':
        case 'E':
            systemHalted = !systemHalted;
            Serial.printf("[CMD] System halt: %s\n", 
                          systemHalted ? "ENABLED (pumps blocked)" : "DISABLED");
            if (systemHalted) {
                relayController.allOff();
            }
            break;
            
        case 'h':
        case 'H':
        case '?':
            printMenu();
            break;
            
        case 'x':
        case 'X':
            Serial.println(F("[REBOOT] Restarting in 2 seconds..."));
            relayController.allOff();
            delay(2000);
            ESP.restart();
            break;
            
        case '\n':
        case '\r':
            break;

        case 'm':
        case 'M':
            testChannelManager();
            printMenu();
            break; 
            
        case 'w':
        case 'W':
            testRTC();
            printMenu();
            break;

        case 'd':
        case 'D':
            testScheduler();
            printMenu();
            break;

            case 'y':
        case 'Y':
            gpioValidationEnabled = !gpioValidationEnabled;
            Serial.printf("[CMD] GPIO Validation: %s\n", 
                          gpioValidationEnabled ? "ENABLED" : "DISABLED");
            break;

        case 'z':
        case 'Z': {
            Serial.println(F("[CMD] GPIO Timing Measurement"));
            Serial.print(F("      Enter channel (0-5): "));
            
            while (!Serial.available()) delay(10);
            char chChar = Serial.read();
            while (Serial.available()) Serial.read();
            
            uint8_t ch = chChar - '0';
            if (ch < CHANNEL_COUNT) {
                Serial.println(ch);
                measureGpioTiming(ch);
            } else {
                Serial.println(F("Invalid channel"));
            }
            break;
        }
            
        default:
            Serial.printf("[?] Unknown command: '%c' (0x%02X). Press 'h' for help.\n", 
                          cmd, cmd);
            break;
    }
}



// ============================================================================
// SYSTEM INFO
// ============================================================================
void printSystemInfo() {
    Serial.println(F("[SYSTEM] Device Information:"));
    Serial.println(F("┌──────────────────────────────────────────────────────────┐"));
    Serial.printf("│  Device ID:      %-40s│\n", DEVICE_ID);
    Serial.printf("│  Firmware:       %-40s│\n", FIRMWARE_VERSION);
    Serial.printf("│  Channels:       %-40d│\n", CHANNEL_COUNT);
    Serial.printf("│  System Halted:  %-40s│\n", systemHalted ? "YES" : "NO");
    Serial.printf("│  GPIO Validation:%-40s│\n", gpioValidationEnabled ? "ENABLED" : "DISABLED");
    Serial.println(F("├──────────────────────────────────────────────────────────┤"));
    Serial.printf("│  Chip Model:     %-40s│\n", ESP.getChipModel());
    Serial.printf("│  CPU Freq:       %-37d MHz│\n", ESP.getCpuFreqMHz());
    Serial.printf("│  Free Heap:      %-37d B  │\n", ESP.getFreeHeap());
    Serial.printf("│  Uptime:         %-34lu ms  │\n", millis());
    Serial.println(F("└──────────────────────────────────────────────────────────┘"));
    Serial.println();
}

