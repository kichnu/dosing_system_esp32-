/**
 * CLI Commands - Serial Command Processor
 *
 * Główny parser komend CLI
 */

#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <Arduino.h>

/**
 * Przetwarzaj komendę z Serial
 * Funkcja blokująca - czeka na input użytkownika
 */
void processSerialCommand();

#endif // CLI_COMMANDS_H
