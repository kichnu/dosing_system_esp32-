/**
 * DOZOWNIK - Safety Manager Implementation
 */

#include "safety_manager.h"
#include "fram_layout.h"
#include "rtc_controller.h"
#include "fram_controller.h"

// Global instance
SafetyManager safetyManager;

// ============================================================================
// INITIALIZATION
// ============================================================================

void SafetyManager::begin() {
    Serial.println(F("[SAFETY] Initializing..."));
    
    // --- GPIO Setup (NAJWCZEŚNIEJ!) ---
    
    // Master relay - domyślnie OFF (bezpieczny stan)
    pinMode(MASTER_RELAY_PIN, OUTPUT);
    digitalWrite(MASTER_RELAY_PIN, MASTER_RELAY_INACTIVE);
    _masterRelayEnabled = false;
    Serial.println(F("[SAFETY] Master relay: INACTIVE (safe default)"));
    
    // Buzzer - domyślnie OFF
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, BUZZER_INACTIVE);
    _buzzerState = false;
    
    // Reset button - input z pull-up
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    
    // Inicjalizuj strukturę błędu
    memset(&_currentError, 0, sizeof(_currentError));
    
    _initialized = true;
    Serial.println(F("[SAFETY] GPIO initialized"));
}

bool SafetyManager::enableIfSafe() {
    if (!_initialized) {
        Serial.println(F("[SAFETY] ERROR: Not initialized!"));
        return false;
    }
    
    // Odczytaj stan błędu z FRAM
    _loadErrorFromFRAM();
    
    if (_currentError.active_flag) {
        // Błąd aktywny w FRAM - pozostań w trybie bezpiecznym
        _errorActive = true;
        _masterRelayEnabled = false;
        
        Serial.println(F("[SAFETY] *** CRITICAL ERROR ACTIVE FROM FRAM ***"));
        Serial.printf("[SAFETY] Type: %s, Channel: %d\n", 
                      errorTypeToString(_currentError.error_type),
                      _currentError.channel);
        Serial.printf("[SAFETY] Timestamp: %lu, Phase: %d\n",
                      _currentError.timestamp,
                      _currentError.phase);
        Serial.println(F("[SAFETY] Master relay: REMAINS INACTIVE"));
        Serial.println(F("[SAFETY] Buzzer: ACTIVE"));
        Serial.println(F("[SAFETY] Press RESET button for 5s to clear"));
        
        return false;
    }
    
    // Brak błędu - włącz master relay
    _setMasterRelay(true);
    Serial.println(F("[SAFETY] No error in FRAM - Master relay ENABLED"));
    
    return true;
}

// ============================================================================
// MAIN UPDATE LOOP
// ============================================================================

void SafetyManager::update() {
    if (!_initialized) return;
    
    // Obsługa wzorca buzzera (jeśli błąd aktywny)
    if (_errorActive) {
        _updateBuzzerPattern();
    }
    
    // Obsługa przycisku reset
    _handleResetButton();
}

// ============================================================================
// CRITICAL ERROR HANDLING
// ============================================================================

void SafetyManager::triggerCriticalError(CriticalErrorType type,
                                          uint8_t channel,
                                          ValidationPhase phase,
                                          uint32_t errorData) {
    Serial.println();
    Serial.println(F("+==========================================================+"));
    Serial.println(F("|            *** CRITICAL ERROR TRIGGERED ***              |"));
    Serial.println(F("+==========================================================+"));

    
    // 1. NATYCHMIAST wyłącz master relay
    _setMasterRelay(false);
    Serial.println(F("[CRITICAL] Master relay DISABLED immediately!"));
    
    // 2. Zapisz snapshot GPIO
    _takeGpioSnapshot();
    
    // 3. Wypełnij strukturę błędu
    _currentError.active_flag = 1;
    _currentError.error_type = type;
    _currentError.channel = channel;
    _currentError.phase = phase;
    _currentError.timestamp = rtcController.getUnixTime();
    _currentError.error_data = errorData;
    _currentError.total_critical_errors++;
    _currentError.write_count++;
    
    // 4. Zapisz do FRAM (persystencja!)
    _saveErrorToFRAM();
    Serial.println(F("[CRITICAL] Error saved to FRAM"));

    // 5. Ustaw flagę i włącz buzzer
    _errorActive = true;
    _buzzerState = true;
    digitalWrite(BUZZER_PIN, BUZZER_ACTIVE);
    _buzzerLastToggle = millis();
    
    // 7. Wypisz szczegóły
    Serial.printf("[CRITICAL] Type: %s (%d)\n", 
                  errorTypeToString(type), type);
    Serial.printf("[CRITICAL] Channel: %d\n", channel);
    Serial.printf("[CRITICAL] Phase: %d\n", phase);
    Serial.printf("[CRITICAL] GPIO snapshot: 0x%02X\n", 
                  _currentError.gpio_state_snapshot);
    Serial.printf("[CRITICAL] Relay snapshot: 0x%02X\n",
                  _currentError.relay_state_snapshot);
    Serial.printf("[CRITICAL] Total errors: %d\n",
                  _currentError.total_critical_errors);
    Serial.println(F(""));
    Serial.println(F("[CRITICAL] >>> SYSTEM LOCKED - PRESS RESET FOR 5s <<<"));
    Serial.println(F(""));
}

bool SafetyManager::resetCriticalError() {
    if (!_errorActive) {
        Serial.println(F("[SAFETY] No active error to reset"));
        return false;
    }
    
    Serial.println(F(""));
    Serial.println(F("[SAFETY] === CRITICAL ERROR RESET ==="));
    
    // 1. Wyłącz buzzer
    _setBuzzer(false);
    
    // 2. Aktualizuj strukturę
    _currentError.active_flag = 0;
    _currentError.reset_count++;
    _currentError.last_reset_timestamp = rtcController.getUnixTime();
    _currentError.write_count++;
    
    // 3. Zapisz do FRAM (wyczyść flagę aktywnego błędu)
    _clearErrorInFRAM();
    Serial.println(F("[SAFETY] Error cleared in FRAM"));
    
    // 4. Zresetuj stan
    _errorActive = false;
    
    // 5. Włącz master relay
    _setMasterRelay(true);
    Serial.println(F("[SAFETY] Master relay RE-ENABLED"));
    
    // 6. Potwierdzenie dźwiękiem (2x krótki beep)
    _confirmResetBeep();
    
    Serial.println(F("[SAFETY] System unlocked - normal operation resumed"));
    Serial.println(F(""));
    
    return true;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void SafetyManager::_setMasterRelay(bool enabled) {
    digitalWrite(MASTER_RELAY_PIN, enabled ? MASTER_RELAY_ACTIVE : MASTER_RELAY_INACTIVE);
    _masterRelayEnabled = enabled;
    Serial.printf("[SAFETY] Master relay: %s\n", enabled ? "ENABLED" : "DISABLED");
}

void SafetyManager::_setBuzzer(bool on) {
    digitalWrite(BUZZER_PIN, on ? BUZZER_ACTIVE : BUZZER_INACTIVE);
    _buzzerState = on;
}

void SafetyManager::_updateBuzzerPattern() {
    // Wzorzec: ON 0.5s, OFF 1.0s
    unsigned long now = millis();
    unsigned long interval = _buzzerState ? BUZZER_ERROR_ON_MS : BUZZER_ERROR_OFF_MS;
    
    if (now - _buzzerLastToggle >= interval) {
        _buzzerState = !_buzzerState;
        digitalWrite(BUZZER_PIN, _buzzerState ? BUZZER_ACTIVE : BUZZER_INACTIVE);
        _buzzerLastToggle = now;
    }
}

void SafetyManager::_handleResetButton() {
    bool buttonPressed = (digitalRead(RESET_BUTTON_PIN) == RESET_BUTTON_ACTIVE);
    unsigned long now = millis();
    
    if (buttonPressed) {
        if (!_buttonWasPressed) {
            // Początek naciśnięcia
            _buttonPressStart = now;
            _buttonWasPressed = true;
            Serial.println(F("[SAFETY] Reset button pressed..."));
        } else {
            // Sprawdź czy przytrzymano wystarczająco długo
            if (!_resetInProgress && (now - _buttonPressStart >= RESET_BUTTON_HOLD_MS)) {
                _resetInProgress = true;
                
                if (_errorActive) {
                    // Reset błędu krytycznego
                    resetCriticalError();
                } else {
                    // Brak błędu - może być użyte do provisioning
                    Serial.println(F("[SAFETY] No error active - button may trigger provisioning"));
                    // Tu można dodać callback do provisioning
                }
            }
        }
    } else {
        // Przycisk puszczony
        if (_buttonWasPressed) {
            if (!_resetInProgress) {
                unsigned long holdTime = now - _buttonPressStart;
                Serial.printf("[SAFETY] Button released after %lu ms (need %d ms)\n",
                              holdTime, RESET_BUTTON_HOLD_MS);
            }
            _buttonWasPressed = false;
            _resetInProgress = false;
        }
    }
}

void SafetyManager::_takeGpioSnapshot() {
    // Snapshot GPIO validation pins
    uint8_t gpioSnapshot = 0;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (digitalRead(VALIDATE_PINS[i]) == HIGH) {
            gpioSnapshot |= (1 << i);
        }
    }
    _currentError.gpio_state_snapshot = gpioSnapshot;
    
    // Snapshot relay pins
    uint8_t relaySnapshot = 0;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (digitalRead(RELAY_PINS[i]) == HIGH) {
            relaySnapshot |= (1 << i);
        }
    }
    _currentError.relay_state_snapshot = relaySnapshot;
    
    // Czy pompa pracowała
    _currentError.pump_was_running = (relaySnapshot != 0) ? 1 : 0;
}

void SafetyManager::_saveErrorToFRAM() {
    // Oblicz CRC
    _currentError.crc32 = FramController::calculateCRC32(&_currentError, 
                                          sizeof(CriticalErrorState) - sizeof(uint32_t));
    
    // Zapisz do FRAM
    framController.writeBytes(FRAM_ADDR_CRITICAL_ERROR, &_currentError, sizeof(CriticalErrorState));
}

void SafetyManager::_loadErrorFromFRAM() {
    CriticalErrorState loaded;
    
    if (!framController.readBytes(FRAM_ADDR_CRITICAL_ERROR, &loaded, sizeof(CriticalErrorState))) {
        Serial.println(F("[SAFETY] Failed to read error state from FRAM"));
        memset(&_currentError, 0, sizeof(_currentError));
        return;
    }
    
    // Walidacja CRC
    uint32_t expectedCrc = FramController::calculateCRC32(&loaded, sizeof(CriticalErrorState) - sizeof(uint32_t));
    if (loaded.crc32 != expectedCrc) {
        Serial.println(F("[SAFETY] Error state CRC mismatch - treating as no error"));
        memset(&_currentError, 0, sizeof(_currentError));
        return;
    }
    
    _currentError = loaded;
}

void SafetyManager::_clearErrorInFRAM() {
    // Zachowaj statystyki, wyczyść tylko flagę aktywnego błędu
    _currentError.active_flag = 0;
    _currentError.crc32 = FramController::calculateCRC32(&_currentError,
                                          sizeof(CriticalErrorState) - sizeof(uint32_t));
    
    framController.writeBytes(FRAM_ADDR_CRITICAL_ERROR, &_currentError, sizeof(CriticalErrorState));
}

void SafetyManager::_confirmResetBeep() {
    // 2x krótki beep potwierdzający reset
    for (int i = 0; i < 2; i++) {
        _setBuzzer(true);
        delay(100);
        _setBuzzer(false);
        delay(100);
    }
}

// ============================================================================
// STATUS / DEBUG
// ============================================================================

void SafetyManager::printStatus() const {
    Serial.println(F(""));
    Serial.println(F("=== SAFETY SYSTEM STATUS ==="));
    Serial.printf("Master relay: %s\n", _masterRelayEnabled ? "ENABLED" : "DISABLED");
    Serial.printf("Critical error: %s\n", _errorActive ? "ACTIVE" : "None");
    
    if (_errorActive) {
        Serial.printf("  Type: %s (%d)\n", 
                      errorTypeToString(_currentError.error_type),
                      _currentError.error_type);
        Serial.printf("  Channel: %d\n", _currentError.channel);
        Serial.printf("  Phase: %d\n", _currentError.phase);
        Serial.printf("  Timestamp: %lu\n", _currentError.timestamp);
    }
    
    Serial.printf("Total errors (history): %d\n", _currentError.total_critical_errors);
    Serial.printf("Reset count: %d\n", _currentError.reset_count);
    Serial.println(F("============================"));
    Serial.println(F(""));
}