

#ifndef CREDENTIALS_MANAGER_H
#define CREDENTIALS_MANAGER_H

#include <Arduino.h>
#include "../crypto/fram_encryption.h"

// ===============================
// CREDENTIALS MANAGER
// Zarządzanie credentials w FRAM
// ===============================

/**
 * Inicjalizacja - próba odczytu credentials z FRAM
 * @return true jeśli credentials załadowane poprawnie
 */
bool initCredentialsManager();

/**
 * Czy credentials są załadowane z FRAM
 */
bool areCredentialsLoaded();

/**
 * Gettery dla credentials (zwracają fallback jeśli brak FRAM)
 */
const char* getWiFiSSID();
const char* getWiFiPassword();
const char* getAdminPasswordHash();
// const char* getVPSToken();
// const char* getVPSURL();
const char* getDeviceName();

/**
 * Zapis credentials do FRAM
 * @return true jeśli zapis udany
 */
bool writeCredentialsToFRAM(const FRAMCredentials& creds);

/**
 * Weryfikacja credentials w FRAM
 * @return true jeśli checksum i magic OK
 */
bool verifyCredentialsInFRAM();

/**
 * Wyczyść credentials z FRAM
 */
bool clearCredentialsInFRAM();

/**
 * Debug - wyświetl status credentials
 */
void printCredentialsStatus();

#endif