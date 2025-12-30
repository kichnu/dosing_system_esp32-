// #include "credentials_manager.h"
// #include "../core/logging.h"
// #include "../hardware/fram_controller.h"
// #include "../crypto/fram_encryption.h"
// #include "config.h"  // For fallback hardcoded credentials

// // Global dynamic credentials instance
// DynamicCredentials dynamicCredentials;

// bool initCredentialsManager() {
//     LOG_INFO("Initializing credentials manager...");
    
//     // Initialize with empty values
//     dynamicCredentials.wifi_ssid = "";
//     dynamicCredentials.wifi_password = "";
//     dynamicCredentials.admin_password_hash = "";
//     dynamicCredentials.vps_auth_token = "";
//     dynamicCredentials.vps_url = "";
//     dynamicCredentials.device_id = "";
//     dynamicCredentials.loaded_from_fram = false;
    
//     // Try to load from FRAM
//     if (loadCredentialsFromFRAM()) {
//         LOG_INFO("✅ Credentials loaded from FRAM successfully");
//         return true;
//     } else {
//         LOG_WARNING("⚠️ Could not load credentials from FRAM, using fallback");
//         fallbackToHardcodedCredentials();
//         return false;
//     }
// }

// bool loadCredentialsFromFRAM() {
//     LOG_INFO("Attempting to load credentials from FRAM...");
    
//     // First verify that credentials exist and are valid
//     if (!verifyCredentialsInFRAM()) {
//         LOG_WARNING("No valid credentials found in FRAM");
//         return false;
//     }
    
//     // Read encrypted credentials
//     FRAMCredentials fram_creds;
//     if (!readCredentialsFromFRAM(fram_creds)) {
//         LOG_ERROR("Failed to read credentials from FRAM");
//         return false;
//     }
    
//     // Check version compatibility - support v2 (without VPS_URL) and v3 (with VPS_URL)
//     if (fram_creds.version != 0x0002 && fram_creds.version != 0x0003) {
//         LOG_ERROR("Unsupported FRAM credentials version: %d", fram_creds.version);
//         return false;
//     }
    
//     // Decrypt credentials
//     DeviceCredentials creds;
//     if (!decryptCredentials(fram_creds, creds)) {
//         LOG_ERROR("Failed to decrypt credentials from FRAM");
//         return false;
//     }
    
//     // Validate decrypted credentials
//     if (creds.device_name.length() == 0 || 
//         creds.wifi_ssid.length() == 0 || 
//         creds.wifi_password.length() == 0 ||
//         creds.admin_password.length() == 0 ||
//         creds.vps_token.length() == 0) {
//         LOG_ERROR("Decrypted credentials are incomplete");
//         return false;
//     }
    
//     // Store in dynamic credentials
//     dynamicCredentials.wifi_ssid = creds.wifi_ssid;
//     dynamicCredentials.wifi_password = creds.wifi_password;
//     dynamicCredentials.admin_password_hash = creds.admin_password;  // Already hashed
//     dynamicCredentials.vps_auth_token = creds.vps_token;
//     dynamicCredentials.device_id = creds.device_name;
//     dynamicCredentials.loaded_from_fram = true;
    
//     // Handle VPS_URL (backward compatibility)
//     if (fram_creds.version >= 0x0003 && creds.vps_url.length() > 0) {
//         dynamicCredentials.vps_url = creds.vps_url;
//         LOG_INFO("VPS URL loaded from FRAM: %s", dynamicCredentials.vps_url.substring(0, 30).c_str());
//     } else {
//         // Fallback to hardcoded VPS_URL for older versions
//         dynamicCredentials.vps_url = String(VPS_URL);
//         LOG_WARNING("Using fallback VPS_URL (FRAM version %d)", fram_creds.version);
//     }
    
//     LOG_INFO("🔐 Dynamic credentials loaded:");
//     LOG_INFO("  Device ID: %s", dynamicCredentials.device_id.c_str());
//     LOG_INFO("  WiFi SSID: %s", dynamicCredentials.wifi_ssid.c_str());
//     LOG_INFO("  WiFi Password: ******* (%d chars)", dynamicCredentials.wifi_password.length());
//     LOG_INFO("  Admin Hash: %s", dynamicCredentials.admin_password_hash.substring(0, 16).c_str());
//     LOG_INFO("  VPS Token: %s...", dynamicCredentials.vps_auth_token.substring(0, 16).c_str());
//     LOG_INFO("  VPS URL: %s", dynamicCredentials.vps_url.c_str());
    
//     return true;
// }

// bool areCredentialsLoaded() {
//     return dynamicCredentials.loaded_from_fram;
// }

// void fallbackToHardcodedCredentials() {
//     LOG_WARNING("🔄 Falling back to placeholder credentials");
//     LOG_WARNING("⚠️ Web authentication will be DISABLED until FRAM is programmed!");
    
//     // Use placeholder credentials from config.h - NOT FUNCTIONAL!
//     dynamicCredentials.wifi_ssid = String(WIFI_SSID);
//     dynamicCredentials.wifi_password = String(WIFI_PASSWORD);
//     dynamicCredentials.vps_auth_token = String(VPS_AUTH_TOKEN);
//     dynamicCredentials.vps_url = String(VPS_URL);
//     dynamicCredentials.device_id = String(DEVICE_ID);
//     dynamicCredentials.loaded_from_fram = false;
    
//     // NO FALLBACK AUTHENTICATION - Force FRAM setup
//     if (ADMIN_PASSWORD_HASH != nullptr) {
//         dynamicCredentials.admin_password_hash = String(ADMIN_PASSWORD_HASH);
//     } else {
//         dynamicCredentials.admin_password_hash = "NO_AUTH_REQUIRES_FRAM_PROGRAMMING";
//     }
    
//     LOG_INFO("📌 Placeholder fallback credentials (NON-FUNCTIONAL):");
//     LOG_INFO("  Device ID: %s", dynamicCredentials.device_id.c_str());
//     LOG_INFO("  WiFi SSID: %s", dynamicCredentials.wifi_ssid.c_str());
//     LOG_INFO("  VPS URL: %s", dynamicCredentials.vps_url.c_str());
//     LOG_INFO("  Admin Hash: %s", dynamicCredentials.admin_password_hash.c_str());
//     LOG_WARNING("🔧 Program FRAM credentials to enable web authentication!");
// }

// // Accessor functions for compatibility with existing code
// const char* getWiFiSSID() {
//     return dynamicCredentials.wifi_ssid.c_str();
// }

// const char* getWiFiPassword() {
//     return dynamicCredentials.wifi_password.c_str();
// }

// const char* getAdminPasswordHash() {
//     return dynamicCredentials.admin_password_hash.c_str();
// }

// const char* getVPSAuthToken() {
//     return dynamicCredentials.vps_auth_token.c_str();
// }

// const char* getVPSURL() {
//     return dynamicCredentials.vps_url.c_str();
// }

// const char* getDeviceID() {
//     return dynamicCredentials.device_id.c_str();
// }




#include "credentials_manager.h"
#include "../hardware/fram_controller.h"
#include "fram_layout.h"
#include "../core/logging.h"

// ===============================
// CREDENTIALS STATE
// ===============================

static bool credentialsLoaded = false;
static DeviceCredentials loadedCredentials;

// Fallback credentials (gdy brak FRAM)
static const char* FALLBACK_WIFI_SSID = "FALLBACK_SSID";
static const char* FALLBACK_WIFI_PASSWORD = "FALLBACK_PASS";
static const char* FALLBACK_DEVICE_NAME = "DOZOWNIK";

// ===============================
// INITIALIZATION
// ===============================

bool initCredentialsManager() {
    LOG_INFO("Initializing credentials manager...");
    
    // Check if FRAM is ready
    if (!framController.isReady()) {
        LOG_ERROR("FRAM not ready - cannot load credentials");
        credentialsLoaded = false;
        return false;
    }
    
    // Read credentials from FRAM
    FRAMCredentials framCreds;
    
    if (!framController.readBytes(FRAM_ADDR_CREDENTIALS, &framCreds, sizeof(FRAMCredentials))) {
        LOG_ERROR("Failed to read credentials from FRAM");
        credentialsLoaded = false;
        return false;
    }
    
    // Verify magic number
    if (framCreds.magic != FRAM_MAGIC_NUMBER) {
        LOG_WARNING("No valid credentials in FRAM (magic: 0x%08X, expected: 0x%08X)",
                    framCreds.magic, FRAM_MAGIC_NUMBER);
        credentialsLoaded = false;
        return false;
    }
    
    // Verify checksum
    size_t checksumOffset = offsetof(FRAMCredentials, checksum);
    uint16_t calculatedChecksum = calculateChecksum((uint8_t*)&framCreds, checksumOffset);
    
    if (calculatedChecksum != framCreds.checksum) {
        LOG_ERROR("Credentials checksum mismatch! (got: 0x%04X, expected: 0x%04X)",
                  calculatedChecksum, framCreds.checksum);
        credentialsLoaded = false;
        return false;
    }
    
    // Decrypt credentials
    if (!decryptCredentials(framCreds, loadedCredentials)) {
        LOG_ERROR("Failed to decrypt credentials");
        credentialsLoaded = false;
        return false;
    }
    
    credentialsLoaded = true;
    LOG_INFO("Credentials loaded successfully for device: %s", 
             loadedCredentials.device_name.c_str());
    
    return true;
}

// ===============================
// STATUS
// ===============================

bool areCredentialsLoaded() {
    return credentialsLoaded;
}

// ===============================
// GETTERS
// ===============================

const char* getWiFiSSID() {
    if (credentialsLoaded && loadedCredentials.wifi_ssid.length() > 0) {
        return loadedCredentials.wifi_ssid.c_str();
    }
    return FALLBACK_WIFI_SSID;
}

const char* getWiFiPassword() {
    if (credentialsLoaded && loadedCredentials.wifi_password.length() > 0) {
        return loadedCredentials.wifi_password.c_str();
    }
    return FALLBACK_WIFI_PASSWORD;
}

const char* getAdminPasswordHash() {
    if (credentialsLoaded && loadedCredentials.admin_password.length() > 0) {
        return loadedCredentials.admin_password.c_str();
    }
    return "NO_AUTH_REQUIRES_FRAM_PROGRAMMING";
}

const char* getVPSToken() {
    if (credentialsLoaded && loadedCredentials.vps_token.length() > 0) {
        return loadedCredentials.vps_token.c_str();
    }
    return "";
}

const char* getVPSURL() {
    if (credentialsLoaded && loadedCredentials.vps_url.length() > 0) {
        return loadedCredentials.vps_url.c_str();
    }
    return "";
}

const char* getDeviceName() {
    if (credentialsLoaded && loadedCredentials.device_name.length() > 0) {
        return loadedCredentials.device_name.c_str();
    }
    return FALLBACK_DEVICE_NAME;
}

// ===============================
// FRAM OPERATIONS
// ===============================

bool writeCredentialsToFRAM(const FRAMCredentials& creds) {
    LOG_INFO("Writing credentials to FRAM...");
    
    if (!framController.isReady()) {
        LOG_ERROR("FRAM not ready");
        return false;
    }
    
    if (!framController.writeBytes(FRAM_ADDR_CREDENTIALS, &creds, sizeof(FRAMCredentials))) {
        LOG_ERROR("Failed to write credentials to FRAM");
        return false;
    }
    
    LOG_INFO("Credentials written to FRAM at address 0x%04X", FRAM_ADDR_CREDENTIALS);
    return true;
}

bool verifyCredentialsInFRAM() {
    FRAMCredentials framCreds;
    
    if (!framController.readBytes(FRAM_ADDR_CREDENTIALS, &framCreds, sizeof(FRAMCredentials))) {
        LOG_ERROR("Failed to read credentials for verification");
        return false;
    }
    
    // Verify magic
    if (framCreds.magic != FRAM_MAGIC_NUMBER) {
        LOG_ERROR("Magic number verification failed");
        return false;
    }
    
    // Verify checksum
    size_t checksumOffset = offsetof(FRAMCredentials, checksum);
    uint16_t calculatedChecksum = calculateChecksum((uint8_t*)&framCreds, checksumOffset);
    
    if (calculatedChecksum != framCreds.checksum) {
        LOG_ERROR("Checksum verification failed");
        return false;
    }
    
    LOG_INFO("FRAM credentials verification passed");
    return true;
}

bool clearCredentialsInFRAM() {
    LOG_WARNING("Clearing credentials from FRAM...");
    
    FRAMCredentials empty;
    memset(&empty, 0, sizeof(FRAMCredentials));
    
    if (!framController.writeBytes(FRAM_ADDR_CREDENTIALS, &empty, sizeof(FRAMCredentials))) {
        LOG_ERROR("Failed to clear credentials");
        return false;
    }
    
    credentialsLoaded = false;
    LOG_INFO("Credentials cleared");
    return true;
}

// ===============================
// DEBUG
// ===============================

void printCredentialsStatus() {
    Serial.println(F("\n[CRED] Credentials Status:"));
    Serial.printf("  Loaded: %s\n", credentialsLoaded ? "YES" : "NO");
    
    if (credentialsLoaded) {
        Serial.printf("  Device: %s\n", loadedCredentials.device_name.c_str());
        Serial.printf("  WiFi SSID: %s\n", loadedCredentials.wifi_ssid.c_str());
        Serial.printf("  WiFi Pass: %s\n", loadedCredentials.wifi_password.length() > 0 ? "***" : "(empty)");
        Serial.printf("  Admin Hash: %s\n", loadedCredentials.admin_password.length() > 0 ? "***" : "(empty)");
        Serial.printf("  VPS Token: %s\n", loadedCredentials.vps_token.length() > 0 ? "***" : "(empty)");
        Serial.printf("  VPS URL: %s\n", loadedCredentials.vps_url.c_str());
    } else {
        Serial.println(F("  Using fallback credentials"));
    }
    Serial.println();
}
