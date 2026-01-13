/**
 * CLI Commands - Implementation
 */

#include "cli_commands.h"
#include "cli_menu.h"
#include "cli_tests.h"
#include "../config/config.h"
#include "../config/dosing_types.h"
#include "../hardware/relay_controller.h"
#include "../hardware/fram_controller.h"
#include "../hardware/rtc_controller.h"
#include "../algorithm/channel_manager.h"
#include "../hardware/dosing_scheduler.h"
#include "../config/daily_log.h"
#include "../config/fram_layout.h"
#include <esp_system.h>

// External references from main
extern RelayController relayController;
extern FramController framController;
extern RtcController rtcController;
extern ChannelManager channelManager;
extern DosingScheduler dosingScheduler;
extern bool systemHalted;
extern bool gpioValidationEnabled;
extern DailyLogManager* g_dailyLog;

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

        // case 'g':
        // case 'G':
        //     gpioValidator.printAllGpio();
        //     break;

        // case 'v':
        // case 'V': {
        //     uint8_t active = relayController.getActiveChannel();
        //     if (active < CHANNEL_COUNT) {
        //         Serial.printf("[CMD] Starting validation for CH%d\n", active);
        //         gpioValidator.startValidation(active);
        //
        //         // Wait for result
        //         while (gpioValidator.isValidating()) {
        //             gpioValidator.update();
        //             delay(100);
        //         }
        //
        //         ValidationResult res = gpioValidator.getLastResult();
        //         Serial.printf("[CMD] Validation result: %s\n",
        //                       GpioValidator::resultToString(res));
        //     } else {
        //         Serial.println(F("[CMD] No pump active - turn one on first"));
        //     }
        //     break;
        // }

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

        case 'Q':
        case 'q': {
            Serial.println(F("[TEST] Simulating 105 days of Daily Log..."));

            // Zapisz początkowy stan headera
            DailyLogStats initial = g_dailyLog->getStats();
            Serial.printf("[TEST] Initial: count=%d, total=%lu\n",
                          initial.count, initial.total_written);

            for (int day = 0; day < 105; day++) {
                uint32_t fake_time = rtcController.getUnixTime() + (day * 86400);
                uint32_t fake_day = timestampToUtcDay(fake_time);

                // Inicjalizuj nowy dzień
                g_dailyLog->initializeNewDay(fake_time);

                // === ZAMIAST recordPowerCycle(), ręcznie zwiększ licznik ===
                DayLogEntry current;
                if (g_dailyLog->getCurrentEntry(current) == DailyLogResult::OK) {
                    // Wpis istnieje, kontynuuj bez dodatkowych zapisów
                }

                // Finalizuj dzień
                g_dailyLog->finalizeDay();

                if (day % 10 == 0) {
                    DailyLogStats stats = g_dailyLog->getStats();
                    Serial.printf("[TEST] Day %d/105: count=%d, total=%lu\n",
                                  day, stats.count, stats.total_written);
                }
            }

            Serial.println(F("[TEST] Complete! Checking stats..."));
            DailyLogStats stats = g_dailyLog->getStats();
            Serial.printf("[TEST] Final: count=%d (max=100), total=%lu (expect=105+)\n",
                          stats.count, stats.total_written);

            // Sprawdź Container Volume
            ContainerVolume vol;
            if (framController.readContainerVolume(0, &vol)) {
                Serial.printf("[TEST] Container: %.1fml ", vol.getContainerMl());
                if (vol.getContainerMl() == 1000.0f) {
                    Serial.println(F("✓ PASS"));
                } else {
                    Serial.println(F("✗ CORRUPTED"));
                }
            }
            break;
        }

        default:
            Serial.printf("[?] Unknown command: '%c' (0x%02X). Press 'h' for help.\n",
                          cmd, cmd);
            break;
    }
}
