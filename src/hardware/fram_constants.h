

#ifndef FRAM_CONSTANTS_H
#define FRAM_CONSTANTS_H

// ===============================
// FRAM ENCRYPTION CONSTANTS
// ===============================

// Magic number for valid credentials
#define FRAM_MAGIC_NUMBER       0x444F5A41  // "DOZA" - kompatybilne z fram_layout.h

// Field size limits
#define MAX_DEVICE_NAME_LEN     32
#define MAX_WIFI_SSID_LEN       32
#define MAX_WIFI_PASSWORD_LEN   64

// Encryption constants
#define ENCRYPTION_SALT         "DOZOWNIK_SALT_2025"
#define ENCRYPTION_SEED         "SecretSeed_v1"

#endif