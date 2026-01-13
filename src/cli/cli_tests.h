/**
 * CLI Tests - Test Functions
 *
 * Funkcje testowe dla różnych komponentów systemu
 */

#ifndef CLI_TESTS_H
#define CLI_TESTS_H

#include <Arduino.h>

/**
 * Test pompy z czasem
 */
void testTimedPump(uint8_t channel, uint32_t duration_ms);

/**
 * Skanowanie szyny I2C
 */
void i2cScan();

/**
 * Test FRAM - odczyt/zapis/dump
 */
void testFRAM();

/**
 * Test Channel Manager - menu interaktywne
 */
void testChannelManager();

/**
 * Test RTC - ustawienie czasu, odczyt
 */
void testRTC();

/**
 * Test Schedulera - menu interaktywne
 */
void testScheduler();

/**
 * Pomiar czasu reakcji GPIO relay->validate
 */
void measureGpioTiming(uint8_t channel);

/**
 * Wyświetl rozmiary struktur config
 */
void printChannelConfigSize();

#endif // CLI_TESTS_H
