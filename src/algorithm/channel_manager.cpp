/**
 * DOZOWNIK - Channel Manager Implementation
 */

#include "channel_manager.h"

// Global instance
ChannelManager channelManager;

// Static empty instances for invalid access
ChannelConfig ChannelManager::_emptyConfig = {};
ChannelDailyState ChannelManager::_emptyDailyState = {};
ChannelCalculated ChannelManager::_emptyCalculated = {};
ContainerVolume ChannelManager::_emptyContainerVolume = {};

// ============================================================================
// INITIALIZATION
// ============================================================================

bool ChannelManager::begin() {
    Serial.println(F("[CH_MGR] Initializing..."));
    
    if (!framController.isReady()) {
        Serial.println(F("[CH_MGR] ERROR: FRAM not ready!"));
        _initialized = false;
        return false;
    }
    
    // Load all data from FRAM
    if (!reloadFromFRAM()) {
        Serial.println(F("[CH_MGR] ERROR: Failed to load from FRAM!"));
        _initialized = false;
        return false;
    }
    
    // Recalculate all channels
    recalculateAll();

    // Load container volumes
    if (!reloadContainerVolumes()) {
        Serial.println(F("[CH_MGR] WARNING: Failed to load container volumes, using defaults"));
        // Initialize with defaults
        for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
            _containerVolume[i].reset();
        }
    }
    
    _initialized = true;
    Serial.println(F("[CH_MGR] Ready"));
    return true;
}

// ============================================================================
// GETTERS
// ============================================================================

const ChannelConfig& ChannelManager::getActiveConfig(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return _emptyConfig;
    return _activeConfig[channel];
}

const ChannelConfig& ChannelManager::getPendingConfig(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return _emptyConfig;
    return _pendingConfig[channel];
}

const ChannelDailyState& ChannelManager::getDailyState(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return _emptyDailyState;
    return _dailyState[channel];
}

const ChannelCalculated& ChannelManager::getCalculated(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return _emptyCalculated;
    return _calculated[channel];
}

const ContainerVolume& ChannelManager::getContainerVolume(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return _emptyContainerVolume;
    return _containerVolume[channel];
}

ChannelState ChannelManager::getChannelState(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return CH_STATE_INACTIVE;
    
    const ChannelConfig& pending = _pendingConfig[channel];
    const ChannelConfig& active = _activeConfig[channel];
    const ChannelCalculated& calc = _calculated[channel];
    
    // Check if has pending changes
    if (pending.has_pending) {
        return CH_STATE_PENDING;
    }
    
    // Check if inactive (no events)
    if (active.events_bitmask == 0) {
        return CH_STATE_INACTIVE;
    }
    
    // Check if incomplete (no days or no dose)
    if (active.days_bitmask == 0 || active.daily_dose_ml <= 0) {
        return CH_STATE_INCOMPLETE;
    }
    
    // Check if invalid (validation failed)
    if (!calc.is_valid) {
        return CH_STATE_INVALID;
    }
    
    // All good
    return CH_STATE_CONFIGURED;
}

// ============================================================================
// SETTERS (modify pending config)
// ============================================================================

bool ChannelManager::setEventsBitmask(uint8_t channel, uint32_t bitmask) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Mask to valid hours (1-23), clear bit 0 and bits 24+
    bitmask &= 0x00FFFFFE;
    
    _pendingConfig[channel].events_bitmask = bitmask;
    
    // Auto-enable if has events, auto-disable if no events
    _pendingConfig[channel].enabled = (bitmask > 0) ? 1 : 0;
    
    return _savePendingConfig(channel);
}

bool ChannelManager::setDaysBitmask(uint8_t channel, uint8_t bitmask) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Mask to valid days (0-6)
    bitmask &= 0x7F;
    
    _pendingConfig[channel].days_bitmask = bitmask;
    return _savePendingConfig(channel);
}

bool ChannelManager::setDailyDose(uint8_t channel, float dose_ml) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Clamp to valid range
    if (dose_ml < 0) dose_ml = 0;
    if (dose_ml > MAX_DAILY_DOSE_ML) dose_ml = MAX_DAILY_DOSE_ML;
    
    _pendingConfig[channel].daily_dose_ml = dose_ml;
    return _savePendingConfig(channel);
}

bool ChannelManager::setDosingRate(uint8_t channel, float rate) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Clamp to valid range
    if (rate < MIN_DOSING_RATE) rate = MIN_DOSING_RATE;
    if (rate > MAX_DOSING_RATE) rate = MAX_DOSING_RATE;
    
    _pendingConfig[channel].dosing_rate = rate;
    return _savePendingConfig(channel);
}

bool ChannelManager::setEnabled(uint8_t channel, bool enabled) {
    if (channel >= CHANNEL_COUNT) return false;
    
    _pendingConfig[channel].enabled = enabled ? 1 : 0;
    return _savePendingConfig(channel);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool ChannelManager::validateConfig(uint8_t channel, ValidationError* error) {
    if (channel >= CHANNEL_COUNT) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), "Invalid channel");
        }
        return false;
    }
    
    const ChannelConfig& cfg = _pendingConfig[channel];
    
    // Skip validation if no events (inactive channel)
    if (cfg.events_bitmask == 0) {
        if (error) error->has_error = false;
        return true;
    }
    
    // Check days
    if (cfg.days_bitmask == 0) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), "No days selected");
        }
        return false;
    }
    
    // Check daily dose
    if (cfg.daily_dose_ml <= 0) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), "Daily dose not set");
        }
        return false;
    }
    
    // Check dosing rate
    if (cfg.dosing_rate <= 0) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), "Dosing rate not calibrated");
        }
        return false;
    }
    
    // Calculate single dose
    uint8_t eventCount = cfg.getActiveEventsCount();
    float singleDose = cfg.daily_dose_ml / (float)eventCount;
    
    // Validate single dose minimum
    if (singleDose < MIN_SINGLE_DOSE_ML) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), 
                     "Single dose %.2f < %.1f ml min", singleDose, MIN_SINGLE_DOSE_ML);
        }
        return false;
    }
    
    // Validate pump duration
    uint32_t pumpMs = (uint32_t)((singleDose / cfg.dosing_rate) * 1000.0f);
    if (pumpMs > MAX_PUMP_DURATION_MS) {
        if (error) {
            error->has_error = true;
            error->channel = channel;
            snprintf(error->message, sizeof(error->message), 
                     "Pump time %lus > %ds max", pumpMs / 1000, MAX_PUMP_DURATION_SECONDS);
        }
        return false;
    }
    
    if (error) error->has_error = false;
    return true;
}

bool ChannelManager::validateAll(ValidationError* firstError) {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (!validateConfig(i, firstError)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// PENDING CHANGES
// ============================================================================

bool ChannelManager::hasPendingChanges(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return false;
    return _pendingConfig[channel].has_pending != 0;
}

bool ChannelManager::hasAnyPendingChanges() const {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (_pendingConfig[i].has_pending) return true;
    }
    return false;
}

bool ChannelManager::applyPendingChanges(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return false;
    
    if (!_pendingConfig[channel].has_pending) {
        return true; // Nothing to apply
    }
    
    Serial.printf("[CH_MGR] Applying pending CH%d\n", channel);
    
    // Copy pending to active
    memcpy(&_activeConfig[channel], &_pendingConfig[channel], sizeof(ChannelConfig));
    
    // Clear pending flag
    _activeConfig[channel].has_pending = 0;
    _pendingConfig[channel].has_pending = 0;
    
    // Update CRCs
    _updateConfigCRC(&_activeConfig[channel]);
    _updateConfigCRC(&_pendingConfig[channel]);
    
    // Save to FRAM
    if (!framController.writeActiveConfig(channel, &_activeConfig[channel])) {
        return false;
    }
    if (!framController.writePendingConfig(channel, &_pendingConfig[channel])) {
        return false;
    }
    
    // Recalculate
    recalculate(channel);
    
    return true;
}

bool ChannelManager::applyAllPendingChanges() {
    bool success = true;
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (!applyPendingChanges(i)) {
            success = false;
        }
    }
    return success;
}

bool ChannelManager::revertPendingChanges(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Copy active to pending
    memcpy(&_pendingConfig[channel], &_activeConfig[channel], sizeof(ChannelConfig));
    _pendingConfig[channel].has_pending = 0;
    
    _updateConfigCRC(&_pendingConfig[channel]);
    
    return framController.writePendingConfig(channel, &_pendingConfig[channel]);
}

// ============================================================================
// DAILY STATE
// ============================================================================

bool ChannelManager::markEventCompleted(uint8_t channel, uint8_t hour, float dosed_ml) {
    if (channel >= CHANNEL_COUNT) return false;
    if (hour < FIRST_EVENT_HOUR || hour > LAST_EVENT_HOUR) return false;
    
    _dailyState[channel].markEventCompleted(hour);
    _dailyState[channel].today_added_ml += dosed_ml;
    
    _updateDailyStateCRC(&_dailyState[channel]);
    
    if (!framController.writeDailyState(channel, &_dailyState[channel])) {
        return false;
    }
    
    // Deduct from container volume
    deductVolume(channel, dosed_ml);
    
    return true;
    
}

bool ChannelManager::markEventFailed(uint8_t channel, uint8_t hour) {
    if (channel >= CHANNEL_COUNT) return false;
    if (hour < FIRST_EVENT_HOUR || hour > LAST_EVENT_HOUR) return false;
    
    _dailyState[channel].markEventFailed(hour);
    
    _updateDailyStateCRC(&_dailyState[channel]);
    
    Serial.printf("[CH_MGR] CH%d hour %d marked as FAILED (total failed today: %d)\n", 
                  channel, hour, _dailyState[channel].failed_count);
    
    return framController.writeDailyState(channel, &_dailyState[channel]);
}

bool ChannelManager::isEventFailed(uint8_t channel, uint8_t hour) const {
    if (channel >= CHANNEL_COUNT) return false;
    return _dailyState[channel].isEventFailed(hour);
}

bool ChannelManager::resetDailyStates() {
    Serial.println(F("[CH_MGR] Resetting daily states"));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        _dailyState[i].reset();
        _updateDailyStateCRC(&_dailyState[i]);
        
        if (!framController.writeDailyState(i, &_dailyState[i])) {
            return false;
        }
    }
    
    recalculateAll();
    return true;
}

bool ChannelManager::isEventCompleted(uint8_t channel, uint8_t hour) const {
    if (channel >= CHANNEL_COUNT) return false;
    return _dailyState[channel].isEventCompleted(hour);
}

float ChannelManager::getTodayDosed(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return 0;
    return _dailyState[channel].today_added_ml;
}

// ============================================================================
// QUERIES
// ============================================================================

bool ChannelManager::isActiveToday(uint8_t channel, uint8_t dayOfWeek) const {
    if (channel >= CHANNEL_COUNT) return false;
    if (dayOfWeek > 6) return false;
    
    return _activeConfig[channel].isDayEnabled(dayOfWeek);
}

bool ChannelManager::shouldExecuteEvent(uint8_t channel, uint8_t hour, uint8_t dayOfWeek) const {
    if (channel >= CHANNEL_COUNT) return false;
    
    const ChannelConfig& cfg = _activeConfig[channel];
    const ChannelDailyState& state = _dailyState[channel];
    const ChannelCalculated& calc = _calculated[channel];
    
    // Check if channel is valid and enabled
    if (!calc.is_valid || !cfg.enabled) return false;
    
    // Check if day is active
    if (!cfg.isDayEnabled(dayOfWeek)) return false;
    
    // Check if hour is scheduled
    if (!cfg.isEventEnabled(hour)) return false;
    
    // Check if already completed
    if (state.isEventCompleted(hour)) return false;

    // Check if failed (no retry after critical error)
    if (state.isEventFailed(hour)) return false;
    
    // Check if daily dose already reached
    if (state.today_added_ml >= cfg.daily_dose_ml) return false;
    
    return true;
}

uint8_t ChannelManager::getNextEventHour(uint8_t channel, uint8_t currentHour) const {
    if (channel >= CHANNEL_COUNT) return 255;
    
    const ChannelConfig& cfg = _activeConfig[channel];
    
    // Search from current hour to 23
    for (uint8_t h = currentHour; h <= LAST_EVENT_HOUR; h++) {
        if (cfg.isEventEnabled(h)) {
            return h;
        }
    }
    
    return 255; // No more events today
}

// ============================================================================
// CONTAINER VOLUME
// ============================================================================

bool ChannelManager::setContainerCapacity(uint8_t channel, float capacity_ml) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Clamp to valid range
    if (capacity_ml < CONTAINER_MIN_ML) capacity_ml = CONTAINER_MIN_ML;
    if (capacity_ml > CONTAINER_MAX_ML) capacity_ml = CONTAINER_MAX_ML;
    
    _containerVolume[channel].setContainerMl(capacity_ml);
    
    // If remaining > new capacity, adjust it
    if (_containerVolume[channel].remaining_ml > _containerVolume[channel].container_ml) {
        _containerVolume[channel].remaining_ml = _containerVolume[channel].container_ml;
    }
    
    _updateContainerVolumeCRC(&_containerVolume[channel]);
    
    Serial.printf("[CH_MGR] CH%d container capacity set to %.1f ml\n", channel, capacity_ml);
    
    return framController.writeContainerVolume(channel, &_containerVolume[channel]);
}

bool ChannelManager::refillContainer(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return false;
    
    _containerVolume[channel].refill();
    _updateContainerVolumeCRC(&_containerVolume[channel]);
    
    Serial.printf("[CH_MGR] CH%d refilled to %.1f ml\n", 
                  channel, _containerVolume[channel].getContainerMl());
    
    return framController.writeContainerVolume(channel, &_containerVolume[channel]);
}

bool ChannelManager::deductVolume(uint8_t channel, float ml) {
    if (channel >= CHANNEL_COUNT) return false;
    if (ml <= 0) return true;  // Nothing to deduct
    
    float before = _containerVolume[channel].getRemainingMl();
    _containerVolume[channel].deduct(ml);
    _updateContainerVolumeCRC(&_containerVolume[channel]);
    
    float after = _containerVolume[channel].getRemainingMl();
    
    Serial.printf("[CH_MGR] CH%d volume: %.1f -> %.1f ml (deducted %.2f ml)\n", 
                  channel, before, after, ml);
    
    // Check low volume warning
    if (_containerVolume[channel].isLowVolume()) {
        Serial.printf("[CH_MGR] WARNING: CH%d low volume! %.1f ml remaining (%.0f%%)\n",
                      channel, after, (float)_containerVolume[channel].getRemainingPercent());
    }
    
    return framController.writeContainerVolume(channel, &_containerVolume[channel]);
}

bool ChannelManager::isLowVolume(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return false;
    return _containerVolume[channel].isLowVolume();
}

float ChannelManager::getRemainingVolume(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return 0;
    return _containerVolume[channel].getRemainingMl();
}

float ChannelManager::getContainerCapacity(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return 0;
    return _containerVolume[channel].getContainerMl();
}

float ChannelManager::getDaysRemaining(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) return 0;
    
    const ChannelConfig& cfg = _activeConfig[channel];
    
    // If no daily dose configured, infinite days
    if (cfg.daily_dose_ml <= 0) return 999.0f;
    
    // If no active days, infinite
    uint8_t activeDays = cfg.getActiveDaysCount();
    if (activeDays == 0) return 999.0f;
    
    float remaining = _containerVolume[channel].getRemainingMl();
    
    // Average daily consumption (accounting for active days per week)
    float avgDailyConsumption = cfg.daily_dose_ml * ((float)activeDays / 7.0f);
    
    if (avgDailyConsumption <= 0) return 999.0f;
    
    return remaining / avgDailyConsumption;
}

bool ChannelManager::reloadContainerVolumes() {
    Serial.println(F("[CH_MGR] Loading container volumes..."));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (!framController.readContainerVolume(i, &_containerVolume[i])) {
            Serial.printf("[CH_MGR] Failed to read container volume CH%d\n", i);
            return false;
        }
        
        // Validate CRC
        uint32_t calc_crc = FramController::calculateCRC32(
            &_containerVolume[i], 
            sizeof(ContainerVolume) - sizeof(uint32_t)
        );
        
        if (calc_crc != _containerVolume[i].crc32) {
            Serial.printf("[CH_MGR] CH%d container volume CRC mismatch, resetting\n", i);
            _containerVolume[i].reset();
            _updateContainerVolumeCRC(&_containerVolume[i]);
            framController.writeContainerVolume(i, &_containerVolume[i]);
        }
    }
    
    return true;
}

// ============================================================================
// FRAM OPERATIONS
// ============================================================================

bool ChannelManager::reloadFromFRAM() {
    Serial.println(F("[CH_MGR] Loading from FRAM..."));
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        if (!framController.readActiveConfig(i, &_activeConfig[i])) {
            Serial.printf("[CH_MGR] Failed to read active CH%d\n", i);
            return false;
        }
        
        if (!framController.readPendingConfig(i, &_pendingConfig[i])) {
            Serial.printf("[CH_MGR] Failed to read pending CH%d\n", i);
            return false;
        }
        
        if (!framController.readDailyState(i, &_dailyState[i])) {
            Serial.printf("[CH_MGR] Failed to read daily CH%d\n", i);
            return false;
        }
    }
    
    return true;
}

bool ChannelManager::saveToFRAM() {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        _updateConfigCRC(&_activeConfig[i]);
        _updateConfigCRC(&_pendingConfig[i]);
        _updateDailyStateCRC(&_dailyState[i]);
        
        if (!framController.writeActiveConfig(i, &_activeConfig[i])) return false;
        if (!framController.writePendingConfig(i, &_pendingConfig[i])) return false;
        if (!framController.writeDailyState(i, &_dailyState[i])) return false;
    }
    return true;
}

bool ChannelManager::_savePendingConfig(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return false;
    
    _pendingConfig[channel].has_pending = 1;
    _updateConfigCRC(&_pendingConfig[channel]);
    
    // Recalculate with pending values
    recalculate(channel);
    
    return framController.writePendingConfig(channel, &_pendingConfig[channel]);
}

// ============================================================================
// CALCULATIONS
// ============================================================================

void ChannelManager::recalculate(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return;
    
    // Use pending config for calculations (shows what will happen)
    const ChannelConfig& cfg = _pendingConfig[channel];
    const ChannelDailyState& state = _dailyState[channel];
    ChannelCalculated& calc = _calculated[channel];
    
    // Count events and days
    calc.active_events_count = cfg.getActiveEventsCount();
    calc.active_days_count = cfg.getActiveDaysCount();
    calc.completed_today_count = state.getCompletedCount();
    
    // Calculate doses
    if (calc.active_events_count > 0 && cfg.daily_dose_ml > 0) {
        calc.single_dose_ml = cfg.daily_dose_ml / (float)calc.active_events_count;
        calc.weekly_dose_ml = cfg.daily_dose_ml * (float)calc.active_days_count;
    } else {
        calc.single_dose_ml = 0;
        calc.weekly_dose_ml = 0;
    }
    
    // Calculate remaining
    calc.today_remaining_ml = cfg.daily_dose_ml - state.today_added_ml;
    if (calc.today_remaining_ml < 0) calc.today_remaining_ml = 0;
    
    // Calculate pump duration
    if (cfg.dosing_rate > 0 && calc.single_dose_ml > 0) {
        calc.pump_duration_ms = (uint32_t)((calc.single_dose_ml / cfg.dosing_rate) * 1000.0f);
    } else {
        calc.pump_duration_ms = 0;
    }
    
    // Validate
    ValidationError err;
    calc.is_valid = validateConfig(channel, &err);
    
    // Determine state
    calc.state = getChannelState(channel);
    
    // Next event (simplified - would need current hour)
    calc.next_event_hour = 255;
}

void ChannelManager::recalculateAll() {
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        recalculate(i);
    }
}

// ============================================================================
// CRC HELPERS
// ============================================================================

void ChannelManager::_updateConfigCRC(ChannelConfig* cfg) {
    cfg->crc32 = FramController::calculateCRC32(cfg, sizeof(ChannelConfig) - sizeof(uint32_t));
}

void ChannelManager::_updateDailyStateCRC(ChannelDailyState* state) {
    state->crc32 = FramController::calculateCRC32(state, sizeof(ChannelDailyState) - sizeof(uint32_t));
}

void ChannelManager::_updateContainerVolumeCRC(ContainerVolume* volume) {
    volume->crc32 = FramController::calculateCRC32(volume, sizeof(ContainerVolume) - sizeof(uint32_t));
}

// ============================================================================
// DEBUG
// ============================================================================

void ChannelManager::printChannelInfo(uint8_t channel) const {
    if (channel >= CHANNEL_COUNT) {
        Serial.println(F("[CH_MGR] Invalid channel"));
        return;
    }
    
    const ChannelConfig& cfg = _activeConfig[channel];
    const ChannelConfig& pending = _pendingConfig[channel];
    const ChannelDailyState& state = _dailyState[channel];
    const ChannelCalculated& calc = _calculated[channel];
    
    const char* stateNames[] = {"INACTIVE", "INCOMPLETE", "INVALID", "CONFIGURED", "PENDING"};
    
    Serial.printf("\n=== Channel %d ===\n", channel);
    Serial.printf("State: %s\n", stateNames[calc.state]);
    Serial.printf("Enabled: %s\n", cfg.enabled ? "YES" : "NO");
    Serial.printf("Has pending: %s\n", pending.has_pending ? "YES" : "NO");
    
    Serial.println(F("\nActive Config:"));
    Serial.printf("  Events bitmask: 0x%06X (%d events)\n", cfg.events_bitmask, calc.active_events_count);
    Serial.printf("  Days bitmask:   0x%02X (%d days)\n", cfg.days_bitmask, calc.active_days_count);
    Serial.printf("  Daily dose:     %.2f ml\n", cfg.daily_dose_ml);
    Serial.printf("  Dosing rate:    %.3f ml/s\n", cfg.dosing_rate);
    
    Serial.println(F("\nCalculated:"));
    Serial.printf("  Single dose:    %.2f ml\n", calc.single_dose_ml);
    Serial.printf("  Weekly dose:    %.2f ml\n", calc.weekly_dose_ml);
    Serial.printf("  Pump duration:  %lu ms\n", calc.pump_duration_ms);
    Serial.printf("  Valid:          %s\n", calc.is_valid ? "YES" : "NO");
    
    Serial.println(F("\nToday:"));
    Serial.printf("  Completed:      %d events\n", calc.completed_today_count);
    Serial.printf("  Dosed:          %.2f ml\n", state.today_added_ml);
    Serial.printf("  Remaining:      %.2f ml\n", calc.today_remaining_ml);

    const ContainerVolume& vol = _containerVolume[channel];
    Serial.println(F("\nContainer:"));
    Serial.printf("  Capacity:     %.1f ml\n", vol.getContainerMl());
    Serial.printf("  Remaining:    %.1f ml (%d%%)\n", vol.getRemainingMl(), vol.getRemainingPercent());
    Serial.printf("  Low warning:  %s\n", vol.isLowVolume() ? "YES!" : "no");
    Serial.printf("  Days left:    %.1f\n", getDaysRemaining(channel));
    
    Serial.println();
}

void ChannelManager::printAllChannels() const {
    Serial.println(F("\n[CH_MGR] All Channels Summary:"));
    Serial.println(F("┌────┬──────────────┬────────┬────────┬──────────┬────────┐"));
    Serial.println(F("│ CH │    State     │ Events │  Days  │ Dose/day │  Rate  │"));
    Serial.println(F("├────┼──────────────┼────────┼────────┼──────────┼────────┤"));
    
    const char* stateNames[] = {"INACTIVE", "INCOMPLETE", "INVALID", "CONFIGURED", "PENDING"};
    
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        const ChannelConfig& cfg = _activeConfig[i];
        const ChannelCalculated& calc = _calculated[i];
        
        Serial.printf("│ %2d │ %-12s │   %2d   │   %2d   │ %6.1f ml│ %5.2f  │\n",
                      i, 
                      stateNames[calc.state],
                      calc.active_events_count,
                      calc.active_days_count,
                      cfg.daily_dose_ml,
                      cfg.dosing_rate);
    }
}