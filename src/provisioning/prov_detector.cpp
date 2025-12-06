#include "prov_detector.h"
#include "prov_config.h"
#include "../core/logging.h"

bool checkProvisioningButton() {
    // Configure button pin as input with pull-up
    pinMode(PROV_BUTTON_PIN, INPUT_PULLUP);
    
    LOG_INFO("Checking provisioning button on GPIO%d...", PROV_BUTTON_PIN);
    
    // Initial debounce delay
    delay(PROV_BUTTON_DEBOUNCE_MS);
    
    // Check if button is pressed (LOW = pressed with pull-up)
    if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
        LOG_INFO("Provisioning button not pressed - normal boot");
        return false;
    }
    
    LOG_INFO("Provisioning button pressed - checking hold time...");
    
    // Button is pressed - check if held for required time
    unsigned long pressStart = millis();
    
    while (millis() - pressStart < PROV_BUTTON_HOLD_MS) {
        // Check every 50ms if button is still pressed
        if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
            // Button released too early
            unsigned long heldTime = millis() - pressStart;
            LOG_INFO("Button released after %lums (required: %dms)", 
                     heldTime, PROV_BUTTON_HOLD_MS);
            return false;
        }
        delay(50);
    }
    
    // Button held for full duration
    LOG_INFO("✓ Provisioning button held for %dms - entering provisioning mode", 
             PROV_BUTTON_HOLD_MS);
    
    return true;
}