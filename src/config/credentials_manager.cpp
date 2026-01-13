

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
    } else {
        Serial.println(F("  Using fallback credentials"));
    }
    Serial.println();
}
