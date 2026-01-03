

/**
 * DOZOWNIK - Relay Controller Implementation
 * 
 * Z pełną walidacją GPIO (PRE → RUN → POST)
 */

#include "relay_controller.h"
#include "safety_manager.h"
#include "dosing_types.h"
#include "channel_manager.h"
#include "dosing_scheduler.h"

// Global instance
RelayController relayController;

// ============================================================================
// INITIALIZATION
// ============================================================================

void RelayController::begin() {
    Serial.println(F("[RELAY] Initializing relay controller..."));
    
    // Initialize relay pins
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], HIGH);  // OFF (active LOW)
        
        _channels[i].is_on = false;
        _channels[i].on_since_ms = 0;
        _channels[i].total_on_time_ms = 0;
        _channels[i].activation_count = 0;
        
        Serial.printf("        CH%d -> Relay GPIO%d\n", i, RELAY_PINS[i]);
    }
    
    // Initialize validation pins
    Serial.println(F("[RELAY] Initializing GPIO validation pins..."));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        pinMode(VALIDATE_PINS[i], INPUT_PULLUP);
        Serial.printf("        CH%d -> Validate GPIO%d\n", i, VALIDATE_PINS[i]);
    }
    
    _activeChannel = 255;
    _activeMaxDuration = 0;
    _validationState = GpioValidationState::IDLE;
    _validationEnabled = false;
    _stateStartTime = 0;
    _lastGpioReading = -1;
    _pumpStartTime = 0;
    _initialized = true;
    
    Serial.println(F("[RELAY] Controller ready"));
}

// ============================================================================
// UPDATE (call in loop)
// ============================================================================

void RelayController::update() {
    if (!_initialized) return;
    
    // Aktualizuj maszynę stanów walidacji
    _updateValidation();
    
    // Sprawdź timeout tylko gdy pompa pracuje
    if (_validationState == GpioValidationState::RUNNING) {
        _checkTimeout();
    }
}

void RelayController::_checkTimeout() {
    if (_activeChannel >= CHANNEL_COUNT) return;
    if (_activeMaxDuration == 0) return;
    
    uint32_t runtime = millis() - _pumpStartTime;
    
    if (runtime >= _activeMaxDuration) {
        Serial.printf("[RELAY] CH%d TIMEOUT after %lu ms\n", _activeChannel, runtime);
        turnOff(_activeChannel);
    }
}

// ============================================================================
// TURN ON (z walidacją GPIO)
// ============================================================================

RelayResult RelayController::turnOn(uint8_t channel, uint32_t max_duration_ms, bool validate) {
    // Validate channel
    if (channel >= CHANNEL_COUNT) {
        Serial.printf("[RELAY] ERROR: Invalid channel %d\n", channel);
        return RelayResult::ERROR_INVALID_CHANNEL;
    }
    
    // Check system halt
    if (systemHalted) {
        Serial.println(F("[RELAY] ERROR: System halted"));
        return RelayResult::ERROR_SYSTEM_HALTED;
    }
    
    // Check safety manager
    if (safetyManager.isCriticalErrorActive()) {
        Serial.println(F("[RELAY] ERROR: Critical error active"));
        return RelayResult::ERROR_SYSTEM_HALTED;
    }
    
    // Check mutex
    if (_activeChannel < CHANNEL_COUNT && _activeChannel != channel) {
        Serial.printf("[RELAY] ERROR: CH%d blocked, CH%d is active\n", channel, _activeChannel);
        return RelayResult::ERROR_MUTEX_LOCKED;
    }
    
    // Check if already on
    if (_channels[channel].is_on) {
        return RelayResult::ERROR_ALREADY_ON;
    }
    
    // Set parameters
    _activeMaxDuration = (max_duration_ms > 0) ? max_duration_ms : MAX_PUMP_DURATION_MS;
    _activeChannel = channel;
    _validationEnabled = validate;
    
    Serial.printf("[RELAY] CH%d starting (max %lu ms, validation: %s)\n", 
                  channel, _activeMaxDuration, validate ? "ON" : "OFF");
    
    if (_validationEnabled) {
        // Rozpocznij sekwencję z PRE-CHECK
        _startPreCheck();
    } else {
        // Bez walidacji - włącz od razu
        _setRelay(channel, true);
        _channels[channel].is_on = true;
        _channels[channel].on_since_ms = millis();
        _channels[channel].activation_count++;
        _pumpStartTime = millis();
        _validationState = GpioValidationState::RUNNING;
        Serial.printf("[RELAY] CH%d ON (no validation)\n", channel);
    }
    
    return RelayResult::OK;
}

// ============================================================================
// TURN OFF (z walidacją POST)
// ============================================================================

RelayResult RelayController::turnOff(uint8_t channel) {
    uint32_t duration;
    return turnOffWithDuration(channel, &duration);
}

RelayResult RelayController::turnOffWithDuration(uint8_t channel, uint32_t* actual_duration_ms) {
    // Validate channel
    if (channel >= CHANNEL_COUNT) {
        if (actual_duration_ms) *actual_duration_ms = 0;
        return RelayResult::ERROR_INVALID_CHANNEL;
    }
    
    // Check if already off
    if (!_channels[channel].is_on && _validationState == GpioValidationState::IDLE) {
        if (actual_duration_ms) *actual_duration_ms = 0;
        return RelayResult::ERROR_ALREADY_OFF;
    }
    
    // Calculate duration
    uint32_t duration = 0;
    if (_pumpStartTime > 0) {
        duration = millis() - _pumpStartTime;
    }
    if (actual_duration_ms) *actual_duration_ms = duration;
    
    // Wyłącz przekaźnik
    _setRelay(channel, false);
    
    Serial.printf("[RELAY] CH%d OFF (ran %lu ms)\n", channel, duration);
    
    // Aktualizuj statystyki
    _channels[channel].total_on_time_ms += duration;
    
    if (_validationEnabled) {
        // Rozpocznij POST-CHECK
        Serial.println(F("[GPIO_VAL] Starting POST-CHECK..."));
        _transitionTo(GpioValidationState::POST_CHECK_DELAY);
    } else {
        // Bez walidacji - zakończ od razu
        _channels[channel].is_on = false;
        _activeChannel = 255;
        _activeMaxDuration = 0;
        _validationState = GpioValidationState::IDLE;
        _pumpStartTime = 0;
    }
    
    return RelayResult::OK;
}

// ============================================================================
// FORCE OFF (bez walidacji POST - dla błędów)
// ============================================================================

void RelayController::forceOffImmediate(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return;
    
    Serial.printf("[RELAY] CH%d FORCE OFF (immediate)\n", channel);
    
    // Wyłącz przekaźnik natychmiast
    _setRelay(channel, false);
    
    // Wyczyść stan
    if (_channels[channel].is_on) {
        uint32_t duration = millis() - _channels[channel].on_since_ms;
        _channels[channel].total_on_time_ms += duration;
    }
    _channels[channel].is_on = false;
    
    if (_activeChannel == channel) {
        _activeChannel = 255;
        _activeMaxDuration = 0;
    }
    
    _validationState = GpioValidationState::IDLE;
    _pumpStartTime = 0;
}

// ============================================================================
// EMERGENCY CONTROLS
// ============================================================================

void RelayController::allOff() {
    Serial.println(F("[RELAY] ALL OFF"));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        _setRelay(i, false);
        
        if (_channels[i].is_on) {
            uint32_t duration = millis() - _channels[i].on_since_ms;
            _channels[i].total_on_time_ms += duration;
            _channels[i].is_on = false;
        }
    }
    
    _activeChannel = 255;
    _activeMaxDuration = 0;
    _validationState = GpioValidationState::IDLE;
    _pumpStartTime = 0;
}

void RelayController::emergencyStop() {
    Serial.println(F("[RELAY] !!! EMERGENCY STOP !!!"));
    allOff();
    systemHalted = true;
}

// ============================================================================
// GPIO VALIDATION STATE MACHINE
// ============================================================================

void RelayController::_updateValidation() {
    switch (_validationState) {
        case GpioValidationState::IDLE:
            // Nic do roboty
            break;
            
        case GpioValidationState::PRE_CHECK_START:
            _handlePreCheckDebounce();
            break;
            
        case GpioValidationState::PRE_CHECK_DEBOUNCE:
            _handlePreCheckDebounce();
            break;
            
        case GpioValidationState::PRE_CHECK_VERIFY:
            _handlePreCheckVerify();
            break;
            
        case GpioValidationState::RELAY_ON_DELAY:
            _handleRelayOnDelay();
            break;
            
        case GpioValidationState::RUN_CHECK_DEBOUNCE:
            _handleRunCheckDebounce();
            break;
            
        case GpioValidationState::RUN_CHECK_VERIFY:
            _handleRunCheckVerify();
            break;
            
        case GpioValidationState::RUNNING:
            _handleRunning();
            break;
            
        case GpioValidationState::POST_CHECK_DELAY:
            _handlePostCheckDelay();
            break;
            
        case GpioValidationState::POST_CHECK_DEBOUNCE:
            _handlePostCheckDebounce();
            break;
            
        case GpioValidationState::POST_CHECK_VERIFY:
            _handlePostCheckVerify();
            break;
            
        case GpioValidationState::VALIDATION_OK:
        case GpioValidationState::VALIDATION_FAILED_PRE:
        case GpioValidationState::VALIDATION_FAILED_RUN:
        case GpioValidationState::VALIDATION_FAILED_POST:
            // Stany końcowe - nic do roboty
            break;
    }
}

// ============================================================================
// PRE-CHECK: Sprawdź LOW przed włączeniem przekaźnika
// ============================================================================

void RelayController::_startPreCheck() {
    Serial.printf("[GPIO_VAL] CH%d PRE-CHECK starting...\n", _activeChannel);
    _transitionTo(GpioValidationState::PRE_CHECK_DEBOUNCE);
}

void RelayController::_handlePreCheckDebounce() {
    if (millis() - _stateStartTime >= GPIO_DEBOUNCE_MS) {
        _transitionTo(GpioValidationState::PRE_CHECK_VERIFY);
    }
}

void RelayController::_handlePreCheckVerify() {
    _lastGpioReading = digitalRead(VALIDATE_PINS[_activeChannel]);
    
    Serial.printf("[GPIO_VAL] CH%d PRE-CHECK: GPIO=%d (expected %d)\n",
                  _activeChannel, _lastGpioReading, GPIO_STATE_IDLE);
    
    if (_lastGpioReading == GPIO_STATE_IDLE) {
        // OK - przewód podłączony, przekaźnik OFF
        Serial.printf("[GPIO_VAL] CH%d PRE-CHECK OK\n", _activeChannel);
        
        // Włącz przekaźnik
        _setRelay(_activeChannel, true);
        _channels[_activeChannel].is_on = true;
        _channels[_activeChannel].on_since_ms = millis();
        _channels[_activeChannel].activation_count++;
        
        Serial.printf("[RELAY] CH%d ON\n", _activeChannel);
        
        // Przejdź do RUN-CHECK
        _transitionTo(GpioValidationState::RELAY_ON_DELAY);
        
    } else {
        // FAIL - przewód urwany lub przekaźnik już włączony!
        Serial.printf("[GPIO_VAL] CH%d PRE-CHECK FAILED! Wire disconnected?\n", _activeChannel);
        _validationFailed(
            GpioValidationState::VALIDATION_FAILED_PRE,
            ERROR_GPIO_PRE_CHECK_FAILED,
            PHASE_PRE
        );
    }
}

// ============================================================================
// RUN-CHECK: Sprawdź HIGH po włączeniu przekaźnika
// ============================================================================

void RelayController::_handleRelayOnDelay() {
    if (millis() - _stateStartTime >= GPIO_CHECK_DELAY_MS) {
        Serial.printf("[GPIO_VAL] CH%d RUN-CHECK starting debounce...\n", _activeChannel);
        _transitionTo(GpioValidationState::RUN_CHECK_DEBOUNCE);
    }
}

void RelayController::_handleRunCheckDebounce() {
    if (millis() - _stateStartTime >= GPIO_DEBOUNCE_MS) {
        _transitionTo(GpioValidationState::RUN_CHECK_VERIFY);
    }
}

void RelayController::_handleRunCheckVerify() {
    _lastGpioReading = digitalRead(VALIDATE_PINS[_activeChannel]);
    
    Serial.printf("[GPIO_VAL] CH%d RUN-CHECK: GPIO=%d (expected %d)\n",
                  _activeChannel, _lastGpioReading, GPIO_STATE_ACTIVE);
    
    if (_lastGpioReading == GPIO_STATE_ACTIVE) {
        // OK - przekaźnik zadziałał
        Serial.printf("[GPIO_VAL] CH%d RUN-CHECK OK - pump running\n", _activeChannel);
        _pumpStartTime = millis();
        _transitionTo(GpioValidationState::RUNNING);
        
    } else {
        // FAIL - przekaźnik nie zadziałał
        Serial.printf("[GPIO_VAL] CH%d RUN-CHECK FAILED! Relay not activated?\n", _activeChannel);
        
        // Wyłącz przekaźnik
        _setRelay(_activeChannel, false);
        
        _validationFailed(
            GpioValidationState::VALIDATION_FAILED_RUN,
            ERROR_GPIO_RUN_CHECK_FAILED,
            PHASE_RUN
        );
    }
}

// ============================================================================
// RUNNING: Pompa pracuje normalnie
// ============================================================================

void RelayController::_handleRunning() {
    // Timeout jest obsługiwany w _checkTimeout()
    // Tu można dodać dodatkowe sprawdzenia w trakcie pracy
}

// ============================================================================
// POST-CHECK: Sprawdź LOW po wyłączeniu przekaźnika
// ============================================================================

void RelayController::_handlePostCheckDelay() {
    if (millis() - _stateStartTime >= GPIO_POST_CHECK_DELAY_MS) {
        Serial.printf("[GPIO_VAL] CH%d POST-CHECK starting debounce...\n", _activeChannel);
        _transitionTo(GpioValidationState::POST_CHECK_DEBOUNCE);
    }
}

void RelayController::_handlePostCheckDebounce() {
    if (millis() - _stateStartTime >= GPIO_DEBOUNCE_MS) {
        _transitionTo(GpioValidationState::POST_CHECK_VERIFY);
    }
}

void RelayController::_handlePostCheckVerify() {
    _lastGpioReading = digitalRead(VALIDATE_PINS[_activeChannel]);
    
    Serial.printf("[GPIO_VAL] CH%d POST-CHECK: GPIO=%d (expected %d)\n",
                  _activeChannel, _lastGpioReading, GPIO_STATE_IDLE);
    
    if (_lastGpioReading == GPIO_STATE_IDLE) {
        // OK - przekaźnik wyłączony prawidłowo
        Serial.printf("[GPIO_VAL] CH%d POST-CHECK OK - cycle complete\n", _activeChannel);
        _validationSuccess();
        
    } else {
        // CRITICAL FAIL - przekaźnik zablokowany w stanie ON!
        Serial.printf("[GPIO_VAL] CH%d POST-CHECK FAILED! RELAY STUCK ON!\n", _activeChannel);
        _validationFailed(
            GpioValidationState::VALIDATION_FAILED_POST,
            ERROR_GPIO_POST_CHECK_FAILED,
            PHASE_POST
        );
    }
}

// ============================================================================
// VALIDATION RESULTS
// ============================================================================

void RelayController::_validationSuccess() {
    Serial.printf("[GPIO_VAL] CH%d validation complete - SUCCESS\n", _activeChannel);
    
    // Wyczyść stan
    _channels[_activeChannel].is_on = false;
    _activeChannel = 255;
    _activeMaxDuration = 0;
    _pumpStartTime = 0;
    
    _transitionTo(GpioValidationState::VALIDATION_OK);
    
    // Po krótkim czasie wróć do IDLE
    _validationState = GpioValidationState::IDLE;
}

void RelayController::_validationFailed(GpioValidationState failState, 
                                         CriticalErrorType errorType,
                                         ValidationPhase phase) {
    uint8_t failedChannel = _activeChannel;
    
    Serial.println();
    Serial.println(F("+==========================================================+"));
    Serial.printf("|  GPIO VALIDATION FAILED - CH%d - %-24s |\n",failedChannel,
                  validationStateToString(failState));
    Serial.println(F("+==========================================================+"));

    
    // Natychmiast wyłącz przekaźnik (jeśli jeszcze włączony)
    _setRelay(failedChannel, false);
    
    // Wyczyść stan lokalny
    _channels[failedChannel].is_on = false;
    _activeChannel = 255;
    _activeMaxDuration = 0;
    _pumpStartTime = 0;
    
    _transitionTo(failState);

    // === OZNACZ EVENT JAKO FAILED (przed zablokowaniem systemu!) ===
    uint8_t eventHour = dosingScheduler.getCurrentEvent().hour;
    if (eventHour >= FIRST_EVENT_HOUR && eventHour <= LAST_EVENT_HOUR) {
        channelManager.markEventFailed(failedChannel, eventHour);
    }
    
    // TRIGGER CRITICAL ERROR w SafetyManager
    safetyManager.triggerCriticalError(
        errorType,
        failedChannel,
        phase,
        (uint32_t)_lastGpioReading
    );
}

// ============================================================================
// HELPERS
// ============================================================================

void RelayController::_transitionTo(GpioValidationState newState) {
    _validationState = newState;
    _stateStartTime = millis();
}

void RelayController::_setRelay(uint8_t channel, bool state) {
    if (channel >= CHANNEL_COUNT) return;
    // Active LOW - LOW = ON, HIGH = OFF
    digitalWrite(RELAY_PINS[channel], state ? LOW : HIGH);
}

GpioValidationResult RelayController::getValidationResult() const {
    switch (_validationState) {
        case GpioValidationState::VALIDATION_OK:
            return GpioValidationResult::OK;
        case GpioValidationState::VALIDATION_FAILED_PRE:
            return GpioValidationResult::FAILED_PRE;
        case GpioValidationState::VALIDATION_FAILED_RUN:
            return GpioValidationResult::FAILED_RUN;
        case GpioValidationState::VALIDATION_FAILED_POST:
            return GpioValidationResult::FAILED_POST;
        default:
            return GpioValidationResult::PENDING;
    }
}

bool RelayController::isValidating() const {
    return _validationState != GpioValidationState::IDLE &&
           _validationState != GpioValidationState::VALIDATION_OK &&
           _validationState != GpioValidationState::VALIDATION_FAILED_PRE &&
           _validationState != GpioValidationState::VALIDATION_FAILED_RUN &&
           _validationState != GpioValidationState::VALIDATION_FAILED_POST;
}

// ============================================================================
// STATUS QUERIES (istniejące + rozszerzone)
// ============================================================================

bool RelayController::isAnyOn() const {
    return _activeChannel < CHANNEL_COUNT;
}

uint8_t RelayController::getActiveChannel() const {
    return _activeChannel;
}

bool RelayController::isChannelOn(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return false;
    return _channels[channel].is_on;
}

uint32_t RelayController::getActiveRuntime() const {
    if (_activeChannel >= CHANNEL_COUNT) return 0;
    if (_pumpStartTime == 0) return 0;
    return millis() - _pumpStartTime;
}

uint32_t RelayController::getRemainingTime() const {
    if (_activeChannel >= CHANNEL_COUNT) return 0;
    if (_activeMaxDuration == 0) return 0;
    if (_pumpStartTime == 0) return _activeMaxDuration;
    
    uint32_t runtime = millis() - _pumpStartTime;
    if (runtime >= _activeMaxDuration) return 0;
    return _activeMaxDuration - runtime;
}

const RelayState& RelayController::getChannelState(uint8_t channel) const {
    static RelayState empty = {false, 0, 0, 0};
    if (channel >= CHANNEL_COUNT) return empty;
    return _channels[channel];
}

uint32_t RelayController::getTotalRuntime() const {
    uint32_t total = 0;
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        total += _channels[i].total_on_time_ms;
        if (_channels[i].is_on && _pumpStartTime > 0) {
            total += millis() - _pumpStartTime;
        }
    }
    return total;
}

// ============================================================================
// DEBUG
// ============================================================================

void RelayController::printStatus() const {
    Serial.println(F("[RELAY] Status:"));
    Serial.printf("        Active channel: %s\n", 
                  _activeChannel < CHANNEL_COUNT ? String(_activeChannel).c_str() : "none");
    Serial.printf("        Validation state: %s\n", validationStateToString(_validationState));
    Serial.printf("        Validation enabled: %s\n", _validationEnabled ? "YES" : "NO");
    
    if (_activeChannel < CHANNEL_COUNT) {
        Serial.printf("        Runtime: %lu ms / %lu ms\n", getActiveRuntime(), _activeMaxDuration);
        Serial.printf("        Last GPIO reading: %d\n", _lastGpioReading);
    }
    
    Serial.println(F("        Channel stats:"));
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        Serial.printf("          CH%d: %s, total=%lu ms, count=%lu\n",
                      i,
                      _channels[i].is_on ? "ON" : "OFF",
                      _channels[i].total_on_time_ms,
                      _channels[i].activation_count);
    }
}

const char* RelayController::resultToString(RelayResult result) {
    switch (result) {
        case RelayResult::OK:                    return "OK";
        case RelayResult::ERROR_INVALID_CHANNEL: return "INVALID_CHANNEL";
        case RelayResult::ERROR_MUTEX_LOCKED:    return "MUTEX_LOCKED";
        case RelayResult::ERROR_SYSTEM_HALTED:   return "SYSTEM_HALTED";
        case RelayResult::ERROR_ALREADY_ON:      return "ALREADY_ON";
        case RelayResult::ERROR_ALREADY_OFF:     return "ALREADY_OFF";
        case RelayResult::ERROR_TIMEOUT:         return "TIMEOUT";
        case RelayResult::ERROR_GPIO_PRE_CHECK:  return "GPIO_PRE_CHECK_FAILED";
        case RelayResult::ERROR_GPIO_RUN_CHECK:  return "GPIO_RUN_CHECK_FAILED";
        case RelayResult::ERROR_GPIO_POST_CHECK: return "GPIO_POST_CHECK_FAILED";
        default:                                 return "UNKNOWN";
    }
}

const char* RelayController::validationStateToString(GpioValidationState state) {
    switch (state) {
        case GpioValidationState::IDLE:                  return "IDLE";
        case GpioValidationState::PRE_CHECK_START:       return "PRE_CHECK_START";
        case GpioValidationState::PRE_CHECK_DEBOUNCE:    return "PRE_CHECK_DEBOUNCE";
        case GpioValidationState::PRE_CHECK_VERIFY:      return "PRE_CHECK_VERIFY";
        case GpioValidationState::RELAY_ON_DELAY:        return "RELAY_ON_DELAY";
        case GpioValidationState::RUN_CHECK_DEBOUNCE:    return "RUN_CHECK_DEBOUNCE";
        case GpioValidationState::RUN_CHECK_VERIFY:      return "RUN_CHECK_VERIFY";
        case GpioValidationState::RUNNING:               return "RUNNING";
        case GpioValidationState::POST_CHECK_DELAY:      return "POST_CHECK_DELAY";
        case GpioValidationState::POST_CHECK_DEBOUNCE:   return "POST_CHECK_DEBOUNCE";
        case GpioValidationState::POST_CHECK_VERIFY:     return "POST_CHECK_VERIFY";
        case GpioValidationState::VALIDATION_OK:         return "VALIDATION_OK";
        case GpioValidationState::VALIDATION_FAILED_PRE: return "VALIDATION_FAILED_PRE";
        case GpioValidationState::VALIDATION_FAILED_RUN: return "VALIDATION_FAILED_RUN";
        case GpioValidationState::VALIDATION_FAILED_POST:return "VALIDATION_FAILED_POST";
        default:                                         return "UNKNOWN";
    }
}