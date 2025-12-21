/**
 * DOZOWNIK - Relay Controller Implementation
 */

#include "relay_controller.h"

// Global instance
RelayController relayController;

// ============================================================================
// INITIALIZATION
// ============================================================================

void RelayController::begin() {
    Serial.println(F("[RELAY] Initializing relay controller..."));
    
    // Initialize all channels
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        // Configure GPIO
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
        
        // Initialize state
        _channels[i].is_on = false;
        _channels[i].on_since_ms = 0;
        _channels[i].total_on_time_ms = 0;
        _channels[i].activation_count = 0;
        
        Serial.printf("        CH%d -> GPIO%d ready\n", i, RELAY_PINS[i]);
    }
    
    _activeChannel = 255;
    _activeMaxDuration = 0;
    _initialized = true;
    
    Serial.println(F("[RELAY] Controller ready"));
}

// ============================================================================
// UPDATE (call in loop)
// ============================================================================

void RelayController::update() {
    if (!_initialized) return;
    
    _checkTimeout();
}

void RelayController::_checkTimeout() {
    if (_activeChannel >= CHANNEL_COUNT) return;
    
    uint32_t runtime = millis() - _channels[_activeChannel].on_since_ms;
    
    // Check timeout
    if (_activeMaxDuration > 0 && runtime >= _activeMaxDuration) {
        Serial.printf("[RELAY] CH%d TIMEOUT after %lu ms\n", 
                      _activeChannel, runtime);
        turnOff(_activeChannel);
    }
}

// ============================================================================
// TURN ON
// ============================================================================

RelayResult RelayController::turnOn(uint8_t channel, uint32_t max_duration_ms) {
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
    
    // Check mutex - is another channel active?
    if (_activeChannel < CHANNEL_COUNT && _activeChannel != channel) {
        Serial.printf("[RELAY] ERROR: CH%d blocked, CH%d is active\n", 
                      channel, _activeChannel);
        return RelayResult::ERROR_MUTEX_LOCKED;
    }
    
    // Check if already on
    if (_channels[channel].is_on) {
        return RelayResult::ERROR_ALREADY_ON;
    }
    
    // Set max duration (default or provided)
    _activeMaxDuration = (max_duration_ms > 0) ? max_duration_ms : MAX_PUMP_DURATION_MS;
    
    // Turn on
    _setRelay(channel, true);
    
    // Update state
    _channels[channel].is_on = true;
    _channels[channel].on_since_ms = millis();
    _channels[channel].activation_count++;
    _activeChannel = channel;
    
    Serial.printf("[RELAY] CH%d ON (max %lu ms)\n", channel, _activeMaxDuration);
    
    return RelayResult::OK;
}

// ============================================================================
// TURN OFF
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
    if (!_channels[channel].is_on) {
        if (actual_duration_ms) *actual_duration_ms = 0;
        return RelayResult::ERROR_ALREADY_OFF;
    }
    
    // Calculate duration
    uint32_t duration = millis() - _channels[channel].on_since_ms;
    if (actual_duration_ms) *actual_duration_ms = duration;
    
    // Turn off
    _setRelay(channel, false);
    
    // Update state
    _channels[channel].is_on = false;
    _channels[channel].total_on_time_ms += duration;
    
    // Clear active
    if (_activeChannel == channel) {
        _activeChannel = 255;
        _activeMaxDuration = 0;
    }
    
    Serial.printf("[RELAY] CH%d OFF (ran %lu ms)\n", channel, duration);
    
    return RelayResult::OK;
}

// ============================================================================
// EMERGENCY CONTROLS
// ============================================================================

void RelayController::allOff() {
    Serial.println(F("[RELAY] ALL OFF"));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (_channels[i].is_on) {
            _setRelay(i, false);
            
            uint32_t duration = millis() - _channels[i].on_since_ms;
            _channels[i].is_on = false;
            _channels[i].total_on_time_ms += duration;
        }
    }
    
    _activeChannel = 255;
    _activeMaxDuration = 0;
}

void RelayController::emergencyStop() {
    Serial.println(F("[RELAY] !!! EMERGENCY STOP !!!"));
    
    allOff();
    systemHalted = true;
}

// ============================================================================
// STATUS QUERIES
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
    return millis() - _channels[_activeChannel].on_since_ms;
}

uint32_t RelayController::getRemainingTime() const {
    if (_activeChannel >= CHANNEL_COUNT) return 0;
    if (_activeMaxDuration == 0) return 0;
    
    uint32_t runtime = getActiveRuntime();
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
        if (_channels[i].is_on) {
            total += millis() - _channels[i].on_since_ms;
        }
    }
    return total;
}

// ============================================================================
// INTERNAL
// ============================================================================

void RelayController::_setRelay(uint8_t channel, bool state) {
    if (channel >= CHANNEL_COUNT) return;
    digitalWrite(RELAY_PINS[channel], state ? HIGH : LOW);
}

// ============================================================================
// DEBUG
// ============================================================================

void RelayController::printStatus() const {
    Serial.println(F("[RELAY] Status:"));
    Serial.printf("        Active channel: %s\n", 
                  _activeChannel < CHANNEL_COUNT ? 
                  String(_activeChannel).c_str() : "none");
    
    if (_activeChannel < CHANNEL_COUNT) {
        Serial.printf("        Runtime: %lu ms / %lu ms\n",
                      getActiveRuntime(), _activeMaxDuration);
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
        default:                                 return "UNKNOWN";
    }
}