/**
 * DOZOWNIK - Dosing Scheduler Implementation
 */

#include "dosing_scheduler.h"
#include "daily_log_types.h"
#include "daily_log.h"

// Global instance
DosingScheduler dosingScheduler;

// ============================================================================
// INITIALIZATION
// ============================================================================

bool DosingScheduler::begin() {
    Serial.println(F("[SCHED] Initializing..."));
    
    _state = SchedulerState::SCHED_DISABLED;
    
    memset(&_currentEvent, 0, sizeof(_currentEvent));
    _currentEvent.channel = 255;
    
    _lastCheckTime = 0;
    _lastUpdateTime = 0;
    _lastHour = 255;
    _lastDay = 255;
    _todayEventCount = 0;
    
    // Load state from FRAM
    SystemState sysState;
    if (framController.readSystemState(&sysState)) {
        _enabled = (sysState.system_enabled != 0);
        Serial.printf("[SCHED] Loaded state from FRAM: %s\n", _enabled ? "ENABLED" : "DISABLED");
        
        // Increment boot count
        sysState.boot_count++;
        framController.writeSystemState(&sysState);
        Serial.printf("[SCHED] Boot count: %lu\n", sysState.boot_count);
    } else {
        Serial.println(F("[SCHED] Failed to load FRAM state, defaulting to ENABLED"));
        _enabled = true;
    }
    
    // Get current time
    if (rtcController.isReady() && rtcController.isTimeValid()) {
        TimeInfo now = rtcController.getTime();
        _lastHour = 255;
        _lastDay = now.day;
        
        // Calculate current UTC day (simple: year*366 + month*31 + day)
        uint32_t currentUtcDay = (uint32_t)now.year * 366 + (uint32_t)now.month * 31 + now.day;
        
        // Check if daily reset needed (different day than last reset)
        if (framController.readSystemState(&sysState)) {
            if (sysState.last_daily_reset_day != currentUtcDay) {
                Serial.println(F("[SCHED] New day detected - performing startup daily reset"));
                _performDailyReset();
                
                // Save new reset day
                sysState.last_daily_reset_day = currentUtcDay;
                framController.writeSystemState(&sysState);
            } else {
                Serial.println(F("[SCHED] Same day - no reset needed"));
            }
        }
    } else {
        Serial.println(F("[SCHED] WARNING: RTC not ready, cannot check daily reset"));
    }
    
    // Set initial state based on enabled
    if (_enabled) {
        _state = SchedulerState::IDLE;
    }
    
    _initialized = true;
    Serial.printf("[SCHED] Ready (%s)\n", _enabled ? "ENABLED" : "DISABLED");
    
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
        // But always check dosing/validation progress if active
        if (_state == SchedulerState::VALIDATING || 
            _state == SchedulerState::DOSING || 
            _state == SchedulerState::WAITING_PUMP) {
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
            
            if (_state != SchedulerState::DOSING && _state != SchedulerState::VALIDATING) {
                _state = SchedulerState::IDLE;
            }
            break;
            
        case SchedulerState::VALIDATING:
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
    
    // Save to FRAM
    SystemState sysState;
    if (framController.readSystemState(&sysState)) {
        sysState.system_enabled = enabled ? 1 : 0;
        if (framController.writeSystemState(&sysState)) {
            Serial.printf("[SCHED] State saved to FRAM: %s\n", enabled ? "ENABLED" : "DISABLED");
        }
    }
    
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

    // Sprawdź czy jesteśmy w oknie resetu (godzina 0)
    if (now.hour != DAILY_RESET_HOUR) {
        return false;  // Nie godzina 0 = nie robimy resetu
    }
    
    // Sprawdź czy dzień się zmienił względem FRAM (nie tylko RAM!)
    uint32_t currentUtcDay = (uint32_t)now.year * 366 + (uint32_t)now.month * 31 + now.day;
    
    SystemState sysState;
    if (framController.readSystemState(&sysState)) {
        if (sysState.last_daily_reset_day == currentUtcDay) {
            return false;  // Już robiliśmy reset dziś
        }
    }
    
    // Aktualizuj też _lastDay dla spójności
    _lastDay = now.day;
    
    return true;
}

bool DosingScheduler::_performDailyReset() {
    TimeInfo now = rtcController.getTime();
    Serial.printf("[SCHED] _performDailyReset called at %02d:%02d UTC!\n", now.hour, now.minute);
    
    // SAFETY: Blokuj reset stanów jeśli nie jest północ
    bool isMidnight = (now.hour == DAILY_RESET_HOUR);
    
    Serial.println(F("[SCHED] === DAILY RESET ==="));
    
    // Daily Log - zawsze aktualizuj plan
    if (g_dailyLog) {
        uint32_t currentTimestamp = rtcController.getUnixTime();
        uint32_t currentUtcDay = currentTimestamp / 86400;
        
        DayLogEntry currentEntry;
        if (g_dailyLog->getCurrentEntry(currentEntry) == DailyLogResult::OK) {
            if (currentEntry.utc_day != currentUtcDay) {
                g_dailyLog->finalizeDay();
                g_dailyLog->initializeNewDay(currentTimestamp);
            }
        }
        g_dailyLog->fillTodayPlan();
    }
    
    _lastDay = now.day;
    _todayEventCount = 0;
    
    // Apply pending changes - zawsze OK
    if (channelManager.hasAnyPendingChanges()) {
        Serial.println(F("[SCHED] Applying pending config changes..."));
        channelManager.applyAllPendingChanges();
    }
    
    // Reset daily states - TYLKO O PÓŁNOCY!
    if (isMidnight) {
        Serial.println(F("[SCHED] Resetting daily states..."));
        channelManager.resetDailyStates();
    } else {
        Serial.println(F("[SCHED] NOT resetting daily states (not midnight)"));
    }
    
    // Save reset day to FRAM
    uint32_t currentUtcDay = (uint32_t)now.year * 366 + (uint32_t)now.month * 31 + now.day;
    SystemState sysState;
    if (framController.readSystemState(&sysState)) {
        sysState.last_daily_reset_day = currentUtcDay;
        framController.writeSystemState(&sysState);
        Serial.printf("[SCHED] Reset day saved: %lu\n", currentUtcDay);
    }
    
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
    
    // Update last check
    _lastCheckTime = millis();
    
    // Log only on hour change
    if (now.hour != _lastHour) {
        _lastHour = now.hour;
        Serial.printf("[SCHED] New hour %02d (day %d)\n", now.hour, now.dayOfWeek);
    }
    
    uint8_t currentMinute = now.minute;
    
    // Find events to execute
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        uint8_t channelOffset = ch * CHANNEL_OFFSET_MINUTES;
        
        // Check if we're in the window for this channel (:00-:04, :10-:14, etc.)
        if (currentMinute >= channelOffset && currentMinute < channelOffset + 5) {
            // Check if this channel should execute (includes "not completed" check)
            if (channelManager.shouldExecuteEvent(ch, now.hour, now.dayOfWeek)) {
                Serial.printf("[SCHED] Event due: CH%d at %02d:%02d (now %02d:%02d)\n", 
                              ch, now.hour, channelOffset, now.hour, currentMinute);
                
                if (_startDosing(ch, now.hour)) {
                    return; // One at a time
                }
            }
        }
    }
}

void DosingScheduler::syncTimeState() {
    if (!rtcController.isReady()) return;
    
    TimeInfo now = rtcController.getTime();
    
    uint8_t oldDay = _lastDay;
    _lastDay = now.day;
    _lastHour = now.hour;
    
    if (oldDay != _lastDay) {
        Serial.printf("[SCHED] Time state synced: day %d -> %d (no reset triggered)\n", 
                      oldDay, _lastDay);
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
    _currentEvent.gpio_validated = false;
    _currentEvent.validation_started = false;
    
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
    
// GPIO validation is now handled inside RelayController (3-phase: PRE/RUN/POST)
    // RelayController::turnOn() already uses GPIO_VALIDATION_DEFAULT
    _currentEvent.validation_started = true;
    _state = SchedulerState::DOSING;  // RelayController handles validation states internally
    
    return true;
}

void DosingScheduler::_checkDosingProgress() {
    if (_currentEvent.channel >= CHANNEL_COUNT) {
        _state = SchedulerState::IDLE;
        return;
    }
    
    // === GPIO validation is now handled by RelayController internally ===
    // Check RelayController's validation state
    GpioValidationResult valResult = relayController.getValidationResult();
    
    // Handle validation failure from RelayController
    if (valResult == GpioValidationResult::FAILED_PRE ||
        valResult == GpioValidationResult::FAILED_RUN ||
        valResult == GpioValidationResult::FAILED_POST) {
        
        Serial.printf("[SCHED] CH%d GPIO VALIDATION FAILED\n", _currentEvent.channel);
        _currentEvent.failed = true;
        _currentEvent.gpio_validated = false;
        _completeDosing(false);
        return;
    }
    
    // Check if validation passed and pump is running
    if (relayController.isPumpRunning()) {
        _currentEvent.gpio_validated = true;
        _state = SchedulerState::DOSING;
    }
    
    // === Handle dosing state ===
    // Check if pump still running
    if (!relayController.isChannelOn(_currentEvent.channel) && 
        !relayController.isValidating()) {
        // Pump stopped (timeout or completed)
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
    
    // ALWAYS mark event as done to prevent retry loop
    // Even failed events should not be retried in the same hour window
    if (success) {
        // Event wykonany pomyślnie
        channelManager.markEventCompleted(
            _currentEvent.channel, 
            _currentEvent.hour,
            _currentEvent.target_ml
        );
    } else {
        // Event nieudany - oznacz jako FAILED (tylko jeśli nie został już oznaczony przez RelayController)
        if (!channelManager.isEventFailed(_currentEvent.channel, _currentEvent.hour)) {
            channelManager.markEventFailed(
                _currentEvent.channel, 
                _currentEvent.hour
            );
        }
    }

        // === DAILY LOG - rejestruj dozowanie ===
    if (g_dailyLog) {
        g_dailyLog->recordDosing(
            _currentEvent.channel,
            _currentEvent.target_ml,
            success
        );
    }
    
    if (success) {
        _todayEventCount++;
        _currentEvent.completed = true;
    } else {
        _currentEvent.failed = true;
        Serial.printf("[SCHED] WARNING: CH%d event marked done despite failure (no retry)\n",
                      _currentEvent.channel);
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
        case SchedulerState::VALIDATING:  return "VALIDATING";
        case SchedulerState::DOSING:      return "DOSING";
        case SchedulerState::WAITING_PUMP: return "WAITING_PUMP";
        case SchedulerState::DAILY_RESET: return "DAILY_RESET";
        case SchedulerState::ERROR:       return "ERROR";
        case SchedulerState::SCHED_DISABLED:    return "SCHED_DISABLED";
        default:                          return "UNKNOWN";
    }
}