/**
 * DOZOWNIK - Dosing Scheduler Implementation
 */

#include "dosing_scheduler.h"

// Global instance
DosingScheduler dosingScheduler;

// ============================================================================
// INITIALIZATION
// ============================================================================

bool DosingScheduler::begin() {
    Serial.println(F("[SCHED] Initializing..."));
    
    _enabled = false;
    _state = SchedulerState::SCHED_DISABLED;
    
    memset(&_currentEvent, 0, sizeof(_currentEvent));
    _currentEvent.channel = 255;
    
    _lastCheckTime = 0;
    _lastUpdateTime = 0;
    _lastHour = 255;
    _lastDay = 255;
    _todayEventCount = 0;
    
    // Get current time
    if (rtcController.isReady() && rtcController.isTimeValid()) {
        TimeInfo now = rtcController.getTime();
        _lastHour = now.hour;
        _lastDay = now.day;
    }
    
    _initialized = true;
    Serial.println(F("[SCHED] Ready (disabled)"));
    
    return true;
}

// ============================================================================
// MAIN UPDATE LOOP
// ============================================================================

void DosingScheduler::update() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    
    // Rate limit updates (every 1 second)
    if (now - _lastUpdateTime < 1000) {
        // But always check dosing progress if pump running
        if (_state == SchedulerState::DOSING || _state == SchedulerState::WAITING_PUMP) {
            _checkDosingProgress();
        }
        return;
    }
    _lastUpdateTime = now;
    
    // Skip if disabled
    if (!_enabled) {
        _state = SchedulerState::SCHED_DISABLED;
        return;
    }
    
    // Skip if system halted
    if (systemHalted) {
        _state = SchedulerState::ERROR;
        return;
    }
    
    // Check RTC
    if (!rtcController.isReady() || !rtcController.isTimeValid()) {
        return;
    }
    
    // Handle current state
    switch (_state) {
        case SchedulerState::IDLE:
        case SchedulerState::CHECKING:
            // Check for daily reset first
            if (_checkDailyReset()) {
                _state = SchedulerState::DAILY_RESET;
                _performDailyReset();
                _state = SchedulerState::IDLE;
            }
            
            // Check schedule
            _state = SchedulerState::CHECKING;
            _checkSchedule();
            
            if (_state != SchedulerState::DOSING) {
                _state = SchedulerState::IDLE;
            }
            break;
            
        case SchedulerState::DOSING:
        case SchedulerState::WAITING_PUMP:
            _checkDosingProgress();
            break;
            
        case SchedulerState::DAILY_RESET:
            // Should not stay here
            _state = SchedulerState::IDLE;
            break;
            
        case SchedulerState::ERROR:
            // Stay in error until cleared
            if (!systemHalted) {
                _state = SchedulerState::IDLE;
            }
            break;
            
        case SchedulerState::SCHED_DISABLED:
            // Do nothing
            break;
    }
}

// ============================================================================
// ENABLE/DISABLE
// ============================================================================

void DosingScheduler::setEnabled(bool enabled) {
    if (_enabled == enabled) return;
    
    _enabled = enabled;
    
    if (enabled) {
        Serial.println(F("[SCHED] Enabled"));
        _state = SchedulerState::IDLE;
        
        // Reset tracking
        if (rtcController.isReady()) {
            TimeInfo now = rtcController.getTime();
            _lastHour = now.hour;
            _lastDay = now.day;
        }
    } else {
        Serial.println(F("[SCHED] Disabled"));
        
        // Stop any current dosing
        if (_state == SchedulerState::DOSING || _state == SchedulerState::WAITING_PUMP) {
            stopCurrentDose();
        }
        
        _state = SchedulerState::SCHED_DISABLED;
    }
}

// ============================================================================
// DAILY RESET
// ============================================================================

bool DosingScheduler::_checkDailyReset() {
    TimeInfo now = rtcController.getTime();
    
    // Check if we're in reset window (hour 0) and day changed
    if (now.hour == DAILY_RESET_HOUR && now.day != _lastDay) {
        return true;
    }
    
    return false;
}

bool DosingScheduler::_performDailyReset() {
    Serial.println(F("[SCHED] === DAILY RESET ==="));
    
    TimeInfo now = rtcController.getTime();
    _lastDay = now.day;
    _todayEventCount = 0;
    
    // Apply pending changes
    if (channelManager.hasAnyPendingChanges()) {
        Serial.println(F("[SCHED] Applying pending config changes..."));
        channelManager.applyAllPendingChanges();
    }
    
    // Reset daily states
    Serial.println(F("[SCHED] Resetting daily states..."));
    channelManager.resetDailyStates();
    
    Serial.println(F("[SCHED] Daily reset complete"));
    
    return true;
}

bool DosingScheduler::forceDailyReset() {
    Serial.println(F("[SCHED] Forced daily reset"));
    return _performDailyReset();
}

// ============================================================================
// SCHEDULE CHECKING
// ============================================================================

void DosingScheduler::_checkSchedule() {
    TimeInfo now = rtcController.getTime();
    
    // Skip hour 0 (reserved for reset)
    if (now.hour == RESERVED_HOUR) {
        return;
    }
    
    // Check if hour changed
    if (now.hour == _lastHour) {
        return; // Already checked this hour
    }
    
    _lastHour = now.hour;
    _lastCheckTime = millis();
    
    Serial.printf("[SCHED] Checking hour %02d (day %d)\n", now.hour, now.dayOfWeek);
    
    // Calculate minute window for this check
    // Events should run at channel offset: CH0=:00, CH1=:10, etc.
    uint8_t currentMinute = now.minute;
    
    // Find events to execute
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        uint8_t channelOffset = ch * CHANNEL_OFFSET_MINUTES;
        
        // Check if we're in the window for this channel
        if (currentMinute >= channelOffset && currentMinute < channelOffset + 5) {
            // Check if this channel should execute
            if (channelManager.shouldExecuteEvent(ch, now.hour, now.dayOfWeek)) {
                Serial.printf("[SCHED] Event due: CH%d at %02d:%02d\n", 
                              ch, now.hour, channelOffset);
                
                if (_startDosing(ch, now.hour)) {
                    return; // One at a time
                }
            }
        }
    }
}

uint8_t DosingScheduler::_findNextEvent(uint8_t hour, uint8_t dayOfWeek) {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        if (channelManager.shouldExecuteEvent(ch, hour, dayOfWeek)) {
            return ch;
        }
    }
    return 255;
}

// ============================================================================
// DOSING EXECUTION
// ============================================================================

bool DosingScheduler::_startDosing(uint8_t channel, uint8_t hour) {
    if (channel >= CHANNEL_COUNT) return false;
    
    // Check if pump already running
    if (relayController.isAnyOn()) {
        Serial.println(F("[SCHED] Pump busy, skipping"));
        return false;
    }
    
    const ChannelCalculated& calc = channelManager.getCalculated(channel);
    
    // Validate
    if (!calc.is_valid || calc.single_dose_ml <= 0 || calc.pump_duration_ms == 0) {
        Serial.printf("[SCHED] CH%d invalid config, skipping\n", channel);
        return false;
    }
    
    // Setup event
    _currentEvent.channel = channel;
    _currentEvent.hour = hour;
    _currentEvent.target_ml = calc.single_dose_ml;
    _currentEvent.target_duration_ms = calc.pump_duration_ms;
    _currentEvent.start_time_ms = millis();
    _currentEvent.completed = false;
    _currentEvent.failed = false;
    
    Serial.printf("[SCHED] Starting CH%d: %.2f ml, %lu ms\n",
                  channel, _currentEvent.target_ml, _currentEvent.target_duration_ms);
    
    // Start pump
    RelayResult res = relayController.turnOn(channel, _currentEvent.target_duration_ms);
    
    if (res != RelayResult::OK) {
        Serial.printf("[SCHED] Failed to start pump: %s\n", 
                      RelayController::resultToString(res));
        _currentEvent.failed = true;
        return false;
    }
    
    _state = SchedulerState::DOSING;
    return true;
}

void DosingScheduler::_checkDosingProgress() {
    if (_currentEvent.channel >= CHANNEL_COUNT) {
        _state = SchedulerState::IDLE;
        return;
    }
    
    // Check if pump still running
    if (!relayController.isChannelOn(_currentEvent.channel)) {
        // Pump stopped (timeout or manual)
        _completeDosing(true);
        return;
    }
    
    // Still running - update state
    _state = SchedulerState::WAITING_PUMP;
}

void DosingScheduler::_completeDosing(bool success) {
    uint32_t actualDuration = millis() - _currentEvent.start_time_ms;
    
    Serial.printf("[SCHED] CH%d complete: %s, %lu ms\n",
                  _currentEvent.channel,
                  success ? "OK" : "FAILED",
                  actualDuration);
    
    if (success) {
        // Mark event completed
        channelManager.markEventCompleted(
            _currentEvent.channel, 
            _currentEvent.hour,
            _currentEvent.target_ml
        );
        
        _todayEventCount++;
        _currentEvent.completed = true;
    } else {
        _currentEvent.failed = true;
    }
    
    // Clear event
    _currentEvent.channel = 255;
    _state = SchedulerState::IDLE;
}

// ============================================================================
// MANUAL CONTROL
// ============================================================================

bool DosingScheduler::triggerManualDose(uint8_t channel) {
    if (channel >= CHANNEL_COUNT) return false;
    
    if (!_enabled) {
        Serial.println(F("[SCHED] Cannot trigger - scheduler disabled"));
        return false;
    }
    
    if (relayController.isAnyOn()) {
        Serial.println(F("[SCHED] Cannot trigger - pump busy"));
        return false;
    }
    
    TimeInfo now = rtcController.getTime();
    
    Serial.printf("[SCHED] Manual trigger CH%d\n", channel);
    return _startDosing(channel, now.hour);
}

void DosingScheduler::stopCurrentDose() {
    if (_currentEvent.channel < CHANNEL_COUNT) {
        Serial.printf("[SCHED] Stopping CH%d\n", _currentEvent.channel);
        relayController.turnOff(_currentEvent.channel);
        _completeDosing(false);
    }
}

// ============================================================================
// QUERIES
// ============================================================================

uint32_t DosingScheduler::getSecondsToNextEvent() const {
    if (!rtcController.isReady()) return 0;
    
    TimeInfo now = rtcController.getTime();
    
    // Find next event
    for (uint8_t h = now.hour; h <= LAST_EVENT_HOUR; h++) {
        for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
            if (channelManager.shouldExecuteEvent(ch, h, now.dayOfWeek)) {
                uint8_t eventMinute = ch * CHANNEL_OFFSET_MINUTES;
                
                // Calculate seconds
                int32_t targetSec = h * 3600 + eventMinute * 60;
                int32_t nowSec = now.hour * 3600 + now.minute * 60 + now.second;
                
                if (targetSec > nowSec) {
                    return targetSec - nowSec;
                }
            }
        }
    }
    
    return 0; // No more events today
}

// ============================================================================
// DEBUG
// ============================================================================

void DosingScheduler::printStatus() const {
    Serial.println(F("\n[SCHED] Status:"));
    Serial.printf("  State: %s\n", stateToString(_state));
    Serial.printf("  Enabled: %s\n", _enabled ? "YES" : "NO");
    Serial.printf("  Today events: %d\n", _todayEventCount);
    Serial.printf("  Last check: %lu ms ago\n", millis() - _lastCheckTime);
    
    if (_currentEvent.channel < CHANNEL_COUNT) {
        Serial.println(F("  Current event:"));
        Serial.printf("    Channel: %d\n", _currentEvent.channel);
        Serial.printf("    Target: %.2f ml\n", _currentEvent.target_ml);
        Serial.printf("    Duration: %lu ms\n", _currentEvent.target_duration_ms);
        Serial.printf("    Running: %lu ms\n", millis() - _currentEvent.start_time_ms);
    }
    
    uint32_t nextIn = getSecondsToNextEvent();
    if (nextIn > 0) {
        Serial.printf("  Next event in: %lu sec\n", nextIn);
    } else {
        Serial.println(F("  No more events today"));
    }
    
    Serial.println();
}

const char* DosingScheduler::stateToString(SchedulerState state) {
    switch (state) {
        case SchedulerState::IDLE:        return "IDLE";
        case SchedulerState::CHECKING:    return "CHECKING";
        case SchedulerState::DOSING:      return "DOSING";
        case SchedulerState::WAITING_PUMP: return "WAITING_PUMP";
        case SchedulerState::DAILY_RESET: return "DAILY_RESET";
        case SchedulerState::ERROR:       return "ERROR";
        case SchedulerState::SCHED_DISABLED:    return "SCHED_DISABLED";
        default:                          return "UNKNOWN";
    }
}