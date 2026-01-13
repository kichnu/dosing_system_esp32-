/**
 * CLI Menu - Banner, Menu, System Info
 *
 * Funkcje wyświetlające interfejs CLI
 */

#ifndef CLI_MENU_H
#define CLI_MENU_H

#include <Arduino.h>

/**
 * Wyświetl banner startowy
 */
void printBanner();

/**
 * Wyświetl menu komend CLI
 */
void printMenu();

/**
 * Wyświetl informacje o systemie
 */
void printSystemInfo();

#endif // CLI_MENU_H
