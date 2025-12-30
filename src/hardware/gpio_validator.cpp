/**
 * DOZOWNIK - GPIO Validator Implementation
 */

#include "gpio_validator.h"

// Global instance
GpioValidator gpioValidator;

// ============================================================================
// INITIALIZATION
// ============================================================================

void GpioValidator::begin() {
    Serial.println(F("[GPIO_VAL] Initializing GPIO validator..."));
    


    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        pinMode(VALIDATE_PINS[i], INPUT_PULLUP);
        Serial.printf("CH%d -> GPIO%d (INPUT_PULLUP, active LOW)\n",
                  i, VALIDATE_PINS[i]);
    }
    
    _state = State::IDLE;
    _channel = 255;
    _startTime = 0;
    _debounceStart = 0;
    _debounceFirstRead = false;
    _lastResult = ValidationResult::SKIPPED;
    _successCount = 0;
    _failCount = 0;
    _initialized = true;
    
    if (gpioValidationEnabled) {
        Serial.println(F("[GPIO_VAL] Validation ENABLED"));
    } else {
        Serial.println(F("[GPIO_VAL] Validation DISABLED (GPIO_VALIDATION_ENABLED=false)"));
    }
}

// ============================================================================
// START VALIDATION
// ============================================================================

void GpioValidator::startValidation(uint8_t channel) {
    if (!_initialized) {
        Serial.println(F("[GPIO_VAL] ERROR: Not initialized"));
        return;
    }
    
    if (channel >= CHANNEL_COUNT) {
        Serial.printf("[GPIO_VAL] ERROR: Invalid channel %d\n", channel);
        return;
    }
    
    // Check if validation is enabled
    if (!gpioValidationEnabled) {
        _lastResult = ValidationResult::SKIPPED;
        _state = State::IDLE;
        Serial.printf("[GPIO_VAL] CH%d validation SKIPPED (disabled)\n", channel);
        return;
    }
    
    // Start validation sequence
    _channel = channel;
    _startTime = millis();
    _state = State::DELAY_WAIT;
    _lastResult = ValidationResult::PENDING;
    
    Serial.printf("[GPIO_VAL] CH%d validation started (wait %d ms)\n", 
                  channel, GPIO_CHECK_DELAY_MS);
}

// ============================================================================
// UPDATE
// ============================================================================

ValidationResult GpioValidator::update() {
    if (!_initialized || _state == State::IDLE) {
        return _lastResult;
    }
    
    uint32_t elapsed = millis() - _startTime;
    
    switch (_state) {
        case State::DELAY_WAIT:
            // Wait for pump to stabilize
            if (elapsed >= GPIO_CHECK_DELAY_MS) {
                Serial.printf("[GPIO_VAL] CH%d starting debounce read\n", _channel);
                _state = State::DEBOUNCE;
                _debounceStart = millis();
                _debounceFirstRead = readGpioRaw(_channel);
            }
            break;
            
        case State::DEBOUNCE: {
            // Debounce check
            uint32_t debounceElapsed = millis() - _debounceStart;
            
            if (debounceElapsed >= GPIO_DEBOUNCE_MS) {
                // Final read
                bool finalRead = readGpioRaw(_channel);
                
                // Check if consistent
                if (finalRead != _debounceFirstRead) {
                    // Inconsistent - restart debounce
                    Serial.println(F("[GPIO_VAL] Debounce inconsistent, restarting"));
                    _debounceStart = millis();
                    _debounceFirstRead = finalRead;
                    
                    // But check total timeout
                    if (elapsed > GPIO_CHECK_DELAY_MS + (GPIO_DEBOUNCE_MS * 3)) {
                        _lastResult = ValidationResult::FAILED_TIMEOUT;
                        _state = State::COMPLETE;
                        _failCount++;
                        Serial.println(F("[GPIO_VAL] FAILED: Debounce timeout"));
                    }
                } else {
           
                    if (finalRead == GPIO_EXPECTED_STATE) {
                        _lastResult = ValidationResult::OK;
                        _successCount++;
                        Serial.printf("[GPIO_VAL] CH%d OK (GPIO=%d)\n", 
                                      _channel, finalRead);
                    } else {
                        _lastResult = ValidationResult::FAILED_NO_SIGNAL;
                        _failCount++;
                        Serial.printf("[GPIO_VAL] CH%d FAILED (GPIO=%d)\n",
                                      _channel, finalRead);
                    }
                    _state = State::COMPLETE;
                }
            }
            break;
        }
            
        case State::COMPLETE:
            _state = State::IDLE;
            break;
            
        default:
            break;
    }
    
    return _lastResult;
}

// ============================================================================
// CANCEL
// ============================================================================

void GpioValidator::cancel() {
    if (_state != State::IDLE) {
        Serial.printf("[GPIO_VAL] CH%d validation cancelled\n", _channel);
        _state = State::IDLE;
        _lastResult = ValidationResult::SKIPPED;
    }
}

// ============================================================================
// STATUS
// ============================================================================

bool GpioValidator::isValidating() const {
    return _state != State::IDLE && _state != State::COMPLETE;
}

ValidationResult GpioValidator::getLastResult() const {
    return _lastResult;
}

uint8_t GpioValidator::getValidatingChannel() const {
    return _channel;
}

// ============================================================================
// GPIO READING
// ============================================================================

bool GpioValidator::readGpioRaw(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return false;
    return digitalRead(VALIDATE_PINS[channel]) == HIGH; // aktywne gdy przekaźnik rozwiera się/styk przełącza się na pompę
}

bool GpioValidator::readGpioDebounced(uint8_t channel, uint32_t debounce_ms) const {
    if (channel >= CHANNEL_COUNT) return false;
    
    bool firstRead = readGpioRaw(channel);
    delay(debounce_ms);
    bool secondRead = readGpioRaw(channel);
    
    // Return true only if both reads match
    if (firstRead == secondRead) {
        return firstRead;
    }
    
    // Inconsistent - do one more read
    delay(debounce_ms);
    return readGpioRaw(channel);
}

void GpioValidator::printAllGpio() const {
    Serial.print(F("[GPIO_VAL] States: "));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        Serial.printf("CH%d=%d ", i, readGpioRaw(i) ? 1 : 0);
    }
    Serial.println();
}

// ============================================================================
// STATS
// ============================================================================

void GpioValidator::resetStats() {
    _successCount = 0;
    _failCount = 0;
}

// ============================================================================
// UTILITY
// ============================================================================

const char* GpioValidator::resultToString(ValidationResult result) {
    switch (result) {
        case ValidationResult::OK:                return "OK";
        case ValidationResult::PENDING:           return "PENDING";
        case ValidationResult::SKIPPED:           return "SKIPPED";
        case ValidationResult::FAILED_NO_SIGNAL:  return "FAILED_NO_SIGNAL";
        case ValidationResult::FAILED_WRONG_STATE: return "FAILED_WRONG_STATE";
        case ValidationResult::FAILED_TIMEOUT:    return "FAILED_TIMEOUT";
        case ValidationResult::ERROR_NO_PUMP_ACTIVE: return "ERROR_NO_PUMP";
        default:                                  return "UNKNOWN";
    }
}