/**
 * DOZOWNIK - Main Entry Point
 * 
 * Firmware testowy - FAZA 1
 * Testowanie podstawowych funkcji hardware przez Serial.
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "dosing_types.h"
#include "fram_layout.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================
bool systemHalted = false;
bool pumpGlobalEnabled = true;

// Test state
static bool relayState[CHANNEL_COUNT] = {false};
static uint32_t lastPrintTime = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void printBanner();
void printMenu();
void processSerialCommand();
void i2cScan();
void testRelay(uint8_t channel, bool state);
void testAllRelays(bool state);
void readValidateGPIO();
void printSystemInfo();
void runRelaySequence();
void printChannelConfigSize();

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
    
    printBanner();
    
    // Initialize I2C
    Serial.println(F("[INIT] Initializing I2C..."));
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);
    Serial.printf("        SDA=%d, SCL=%d, Freq=%d Hz\n", I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    
    // Initialize Relay GPIOs (OUTPUT, LOW)
    Serial.println(F("[INIT] Initializing RELAY GPIOs..."));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
        relayState[i] = false;
        Serial.printf("        CH%d -> GPIO%d (OUTPUT, LOW)\n", i, RELAY_PINS[i]);
    }
    
    // Initialize Validate GPIOs (INPUT)
    Serial.println(F("[INIT] Initializing VALIDATE GPIOs..."));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        pinMode(VALIDATE_PINS[i], INPUT);
        Serial.printf("        CH%d -> GPIO%d (INPUT)\n", i, VALIDATE_PINS[i]);
    }
    
    Serial.println();
    Serial.println(F("[INIT] === INITIALIZATION COMPLETE ==="));
    Serial.println();
    
    // Run I2C scan
    i2cScan();
    
    // Print menu
    printMenu();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    // Process Serial commands
    if (Serial.available()) {
        processSerialCommand();
    }
    
    // Periodic status (every 5 seconds if any relay is ON)
    bool anyRelayOn = false;
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (relayState[i]) anyRelayOn = true;
    }
    
    if (anyRelayOn && (millis() - lastPrintTime > 5000)) {
        lastPrintTime = millis();
        Serial.print(F("[STATUS] Relays ON: "));
        for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
            if (relayState[i]) Serial.printf("CH%d ", i);
        }
        Serial.println();
        readValidateGPIO();
    }
    
    delay(10);
}

// ============================================================================
// BANNER & MENU
// ============================================================================
void printBanner() {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║          DOZOWNIK - Automatic Fertilizer Dosing System        ║"));
    Serial.println(F("║                    Hardware Test Firmware                     ║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));
    Serial.printf("║  Firmware: %-20s  Channels: %-14d  ║\n", FIRMWARE_VERSION, CHANNEL_COUNT);
    Serial.printf("║  Device:   %-20s  Build: %s %-5s  ║\n", DEVICE_ID, __DATE__, "");
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.println();
}

void printMenu() {
    Serial.println(F("┌─────────────────────────────────────────────────────────────────┐"));
    Serial.println(F("│                      SERIAL TEST MENU                           │"));
    Serial.println(F("├─────────────────────────────────────────────────────────────────┤"));
    Serial.println(F("│  i - I2C scan                    s - System info                │"));
    Serial.println(F("│  0-5 - Toggle relay CH0-CH5     a - All relays ON               │"));
    Serial.println(F("│  o - All relays OFF             r - Read validate GPIOs         │"));
    Serial.println(F("│  q - Relay sequence test        c - Config struct sizes         │"));
    Serial.println(F("│  h - This help menu             x - Reboot                      │"));
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
        case 'i':
        case 'I':
            i2cScan();
            break;
            
        case '0': case '1': case '2': case '3': case '4': case '5': {
            uint8_t ch = cmd - '0';
            if (ch < CHANNEL_COUNT) {
                relayState[ch] = !relayState[ch];
                testRelay(ch, relayState[ch]);
            }
            break;
        }
        
        case 'a':
        case 'A':
            testAllRelays(true);
            break;
            
        case 'o':
        case 'O':
            testAllRelays(false);
            break;
            
        case 'r':
        case 'R':
            readValidateGPIO();
            break;
            
        case 'q':
        case 'Q':
            runRelaySequence();
            break;
            
        case 's':
        case 'S':
            printSystemInfo();
            break;
            
        case 'c':
        case 'C':
            printChannelConfigSize();
            break;
            
        case 'h':
        case 'H':
        case '?':
            printMenu();
            break;
            
        case 'x':
        case 'X':
            Serial.println(F("[REBOOT] Restarting in 2 seconds..."));
            delay(2000);
            ESP.restart();
            break;
            
        case '\n':
        case '\r':
            // Ignore newlines
            break;
            
        default:
            Serial.printf("[?] Unknown command: '%c' (0x%02X)\n", cmd, cmd);
            Serial.println(F("    Press 'h' for help"));
            break;
    }
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
            
            // Identify known devices
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
    Serial.printf("[I2C] Scan complete. Found %d device(s)\n", found);
    Serial.println();
    
    // Check for expected devices
    if (found == 0) {
        Serial.println(F("[WARN] No I2C devices found! Check wiring."));
    } else {
        Wire.beginTransmission(FRAM_I2C_ADDRESS);
        if (Wire.endTransmission() != 0) {
            Serial.println(F("[WARN] FRAM not found at 0x50!"));
        }
        
        Wire.beginTransmission(RTC_I2C_ADDRESS);
        if (Wire.endTransmission() != 0) {
            Serial.println(F("[WARN] RTC not found at 0x68!"));
        }
    }
    Serial.println();
}

// ============================================================================
// RELAY TESTS
// ============================================================================
void testRelay(uint8_t channel, bool state) {
    if (channel >= CHANNEL_COUNT) {
        Serial.println(F("[ERROR] Invalid channel!"));
        return;
    }
    
    digitalWrite(RELAY_PINS[channel], state ? HIGH : LOW);
    relayState[channel] = state;
    
    Serial.printf("[RELAY] CH%d (GPIO%d) -> %s\n", 
                  channel, RELAY_PINS[channel], 
                  state ? "ON ■" : "OFF □");
    
    // Read corresponding validate GPIO
    delay(100);
    int validateState = digitalRead(VALIDATE_PINS[channel]);
    Serial.printf("        VALIDATE GPIO%d = %d %s\n",
                  VALIDATE_PINS[channel], validateState,
                  validateState == GPIO_EXPECTED_STATE ? "(expected)" : "(unexpected!)");
}

void testAllRelays(bool state) {
    Serial.printf("[RELAY] Setting ALL relays to %s\n", state ? "ON" : "OFF");
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        digitalWrite(RELAY_PINS[i], state ? HIGH : LOW);
        relayState[i] = state;
    }
    
    // Print status
    Serial.print(F("        Status: "));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        Serial.printf("CH%d=%s ", i, state ? "ON" : "OFF");
    }
    Serial.println();
    
    if (state) {
        Serial.println(F("[WARN] All pumps running! Press 'o' to turn off."));
    }
}

void runRelaySequence() {
    Serial.println(F("[TEST] Running relay sequence test..."));
    Serial.println(F("       Each relay ON for 1 second"));
    Serial.println();
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        Serial.printf("[SEQ] CH%d ON  (GPIO%d)\n", i, RELAY_PINS[i]);
        digitalWrite(RELAY_PINS[i], HIGH);
        relayState[i] = true;
        
        // Read validate GPIO
        delay(500);
        int valState = digitalRead(VALIDATE_PINS[i]);
        Serial.printf("      Validate GPIO%d = %d\n", VALIDATE_PINS[i], valState);
        
        delay(500);
        
        Serial.printf("[SEQ] CH%d OFF\n", i);
        digitalWrite(RELAY_PINS[i], LOW);
        relayState[i] = false;
        
        delay(200);
    }
    
    Serial.println(F("[TEST] Sequence complete"));
    Serial.println();
}

// ============================================================================
// VALIDATE GPIO READ
// ============================================================================
void readValidateGPIO() {
    Serial.println(F("[GPIO] Reading VALIDATE inputs:"));
    Serial.print(F("       "));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        int state = digitalRead(VALIDATE_PINS[i]);
        Serial.printf("CH%d(G%d)=%d  ", i, VALIDATE_PINS[i], state);
    }
    Serial.println();
}

// ============================================================================
// SYSTEM INFO
// ============================================================================
void printSystemInfo() {
    Serial.println(F("[SYSTEM] Device Information:"));
    Serial.println(F("┌──────────────────────────────────────────────────────────┐"));
    Serial.printf("│  Device ID:      %-40s│\n", DEVICE_ID);
    Serial.printf("│  Firmware:       %-40s│\n", FIRMWARE_VERSION);
    Serial.printf("│  Device Type:    %-40s│\n", DEVICE_TYPE);
    Serial.printf("│  Channels:       %-40d│\n", CHANNEL_COUNT);
    Serial.println(F("├──────────────────────────────────────────────────────────┤"));
    Serial.printf("│  Chip Model:     %-40s│\n", ESP.getChipModel());
    Serial.printf("│  Chip Revision:  %-40d│\n", ESP.getChipRevision());
    Serial.printf("│  CPU Freq:       %-37d MHz│\n", ESP.getCpuFreqMHz());
    Serial.printf("│  Flash Size:     %-37d KB │\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("│  Free Heap:      %-37d B  │\n", ESP.getFreeHeap());
    Serial.printf("│  Min Free Heap:  %-37d B  │\n", ESP.getMinFreeHeap());
    Serial.println(F("├──────────────────────────────────────────────────────────┤"));
    Serial.printf("│  Uptime:         %-34lu ms  │\n", millis());
    Serial.println(F("└──────────────────────────────────────────────────────────┘"));
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
    Serial.printf("│  FRAM Total Used:    ~%4d bytes        │\n", 
                  FRAM_ADDR_RESERVED);
    Serial.printf("│  FRAM Available:     %5d bytes        │\n", 
                  FRAM_SIZE_BYTES - FRAM_ADDR_RESERVED);
    Serial.println(F("└─────────────────────────────────────────┘"));
    Serial.println();
    
    // Verify static_asserts passed
    Serial.println(F("[CONFIG] All structure sizes verified OK"));
    Serial.println();
}