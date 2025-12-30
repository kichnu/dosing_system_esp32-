// #ifndef CREDENTIALS_MANAGER_H
// #define CREDENTIALS_MANAGER_H

// #include <Arduino.h>

// // Dynamic credentials structure
// struct DynamicCredentials {
//     String wifi_ssid;
//     String wifi_password;
//     String admin_password_hash;  // SHA-256 hex string
//     String vps_auth_token;
//     String vps_url;             // VPS URL
//     String device_id;
//     bool loaded_from_fram;
// };

// // Global dynamic credentials
// extern DynamicCredentials dynamicCredentials;

// // Credentials management functions
// bool initCredentialsManager();
// bool loadCredentialsFromFRAM();
// bool areCredentialsLoaded();
// void fallbackToHardcodedCredentials();

// // Accessor functions for compatibility with existing code
// const char* getWiFiSSID();
// const char* getWiFiPassword();
// const char* getAdminPasswordHash();
// const char* getVPSAuthToken();
// const char* getVPSURL();
// const char* getDeviceID();

// #endif

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
const char* getVPSToken();
const char* getVPSURL();
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