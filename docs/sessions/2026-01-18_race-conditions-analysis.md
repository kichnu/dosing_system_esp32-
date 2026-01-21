# Race Conditions & Memory Access Conflicts Analysis

**Date:** 2026-01-18
**Status:** Analysis complete, fixes pending

---

## CRITICAL (Priority 1)

### 1. Global `systemHalted` Variable
- **Location:** `main.cpp:41`
- **Risk:** System halt state not synchronized
- **Competing contexts:**
  - Main loop (reads)
  - SafetyManager (writes via `triggerCriticalError()`)
  - Web server handlers (reads)
  - RelayController (reads)
- **Scenario:** SafetyManager sets `systemHalted = true`, but web server reads stale cached value and approves manual dosing
- **Fix:** Add `volatile`, consider atomic operations

### 2. ChannelManager RAM Cache Concurrent Access
- **Location:** `channel_manager.cpp:296-301`
- **Data at risk:**
  - `_activeConfig[CHANNEL_COUNT]`
  - `_pendingConfig[CHANNEL_COUNT]`
  - `_dailyState[CHANNEL_COUNT]`
  - `_containerVolume[CHANNEL_COUNT]`
  - `_dosedTracker[CHANNEL_COUNT]`
- **Competing contexts:**
  - Main loop scheduler reads configs
  - Web handlers modify pending configs
  - Daily reset writes all configs
  - FRAM operations update RAM cache
- **Scenario:** Web handler modifies `_pendingConfig` while scheduler reads `_activeConfig` during `shouldExecuteEvent()`
- **Fix:** Mutex/critical section for cache access

### 3. DosingScheduler State Modification
- **Location:** `dosing_scheduler.cpp:84-153`
- **Data at risk:**
  - `_currentEvent` (struct)
  - `_state` (enum)
  - `_enabled`
- **Competing contexts:**
  - Main loop calls `update()` every 10ms
  - Web handlers call `triggerManualDose()`, `forceDailyReset()`
  - RelayController reads `getCurrentEvent()`
- **Scenario:** Web calls `triggerManualDose()` while scheduler modifies `_currentEvent` → relay reads partial struct
- **Fix:** Atomic struct copy or snapshot pattern

### 4. RelayController Mutex TOCTOU
- **Location:** `relay_controller.cpp:108-121`
- **Data at risk:** `_activeChannel` (single-pump mutex)
- **Competing contexts:**
  - Scheduler calls `turnOn()` for dosing
  - Web handler calls `turnOn()` for calibration
- **Issue:** Check-then-set without atomic compare-and-swap:
  ```cpp
  if (_activeChannel < CHANNEL_COUNT) { return ERROR; }  // Check
  _activeChannel = channel;  // Set - RACE between check and set!
  ```
- **Scenario:** Both calibration and scheduler pass check, both assign → two pumps running
- **Fix:** Atomic test-and-set or critical section

### 5. Multi-Step Error Handling Race
- **Location:** `relay_controller.cpp:494-506`
- **Data at risk:** Error state, event marking
- **Issue:** Multiple operations without atomicity:
  ```cpp
  uint8_t eventHour = dosingScheduler.getCurrentEvent().hour;  // Read partial
  channelManager.markEventFailed(failedChannel, eventHour);     // Write state
  safetyManager.triggerCriticalError(...);                      // Safety change
  ```
- **Scenario:** Reads uninitialized event hour, marks wrong channel/hour as failed
- **Fix:** Transaction or snapshot before operations

---

## HIGH (Priority 2)

### 8. Dosed Tracker Total Corruption
- **Location:** `channel_manager.cpp:605-616`
- **Issue:** Concurrent `addDosedVolume()` calls:
  ```cpp
  _dosedTracker[channel].addDosed(ml);   // Both read 100.0
  framController.writeDosedTracker(...); // Both write 110.0, one lost
  ```
- **Scenario:** Two events complete simultaneously → one dose value lost
- **Fix:** Atomic increment or mutex

### 9. NTP Sync vs Daily Reset Timing
- **Location:** `main.cpp:238-240`
- **Issue:** `syncTimeState()` called after time already changed
- **Scenario:** Time jumps from 23:59 to 00:01, scheduler detects new day before sync completes
- **Fix:** Call syncTimeState() before scheduler update after NTP sync

---

## MEDIUM (Priority 3)

### 10. GPIO Validation State Machine
- **Location:** `relay_controller.cpp:258-311`
- **Data at risk:** `_validationState`, `_stateStartTime`, `_activeChannel`
- **Issue:** State machine variables modified from main loop and web handlers
- **Fix:** State machine should only be driven from one context

### 11. Daily Reset Non-Atomic
- **Location:** `channel_manager.cpp:389-403`
- **Issue:** Loop resets channels one by one:
  ```cpp
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      _dailyState[i].reset();
      framController.writeDailyState(i, ...);  // Web can read between writes
  }
  ```
- **Fix:** Disable web reads during reset or batch FRAM operations

---

## Summary Table

| # | Location | Severity | Issue Type | Fix Priority |
|---|----------|----------|------------|--------------|
| 1 | main.cpp:41 systemHalted | CRITICAL | No volatile/sync | P1 |
| 2 | channel_manager RAM cache | CRITICAL | Concurrent R/W | P1 |
| 3 | dosing_scheduler state | CRITICAL | TOCTOU | P1 |
| 4 | relay_controller mutex | CRITICAL | TOCTOU | P1 |
| 5 | relay_controller error | CRITICAL | Multi-step race | P1 |
| 6 | web_server config | HIGH | Partial writes | P2 |
| 7 | channel_manager volume | HIGH | R/M/W race | P2 |
| 8 | channel_manager dosed | HIGH | R/M/W race | P2 |
| 9 | main.cpp NTP sync | HIGH | Timing gap | P2 |
| 10 | relay_controller FSM | MEDIUM | State race | P3 |
| 11 | channel_manager reset | MEDIUM | Non-atomic loop | P3 |

---

## Recommended Fix Approaches

### 1. Volatile for Global Flags
```cpp
volatile bool systemHalted = false;
volatile uint8_t activeChannel = 255;
```

### 2. Critical Sections (ESP32)
```cpp
portMUX_TYPE configMux = portMUX_INITIALIZER_UNLOCKED;

void safeWriteConfig(uint8_t ch, const ChannelConfig* cfg) {
    portENTER_CRITICAL(&configMux);
    _activeConfig[ch] = *cfg;
    portEXIT_CRITICAL(&configMux);
}
```

### 3. Mutex for ChannelManager
```cpp
SemaphoreHandle_t channelMutex = xSemaphoreCreateMutex();

bool setEventsBitmask(uint8_t ch, uint32_t mask) {
    if (xSemaphoreTake(channelMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _pendingConfig[ch].events_bitmask = mask;
        // ... FRAM write
        xSemaphoreGive(channelMutex);
        return true;
    }
    return false;
}
```

### 4. Atomic Test-and-Set for Relay Mutex
```cpp
bool tryAcquirePump(uint8_t channel) {
    portENTER_CRITICAL(&relayMux);
    if (_activeChannel < CHANNEL_COUNT) {
        portEXIT_CRITICAL(&relayMux);
        return false;  // Already locked
    }
    _activeChannel = channel;
    portEXIT_CRITICAL(&relayMux);
    return true;
}
```

### 5. Snapshot Pattern for Scheduler
```cpp
struct EventSnapshot {
    uint8_t channel;
    uint8_t hour;
    bool valid;
};

EventSnapshot getEventSnapshot() {
    EventSnapshot snap;
    portENTER_CRITICAL(&schedulerMux);
    snap.channel = _currentEvent.channel;
    snap.hour = _currentEvent.hour;
    snap.valid = (_state == STATE_DOSING);
    portEXIT_CRITICAL(&schedulerMux);
    return snap;
}
```

### 6. Request Queue for Web Handlers
```cpp
enum RequestType { REQ_SET_EVENTS, REQ_SET_DAYS, REQ_SET_DOSE, REQ_REFILL };

struct ConfigRequest {
    RequestType type;
    uint8_t channel;
    uint32_t value;
};

QueueHandle_t requestQueue;

// Web handler adds to queue
void handleApiConfig(...) {
    ConfigRequest req = { REQ_SET_EVENTS, channel, events };
    xQueueSend(requestQueue, &req, 0);
}

// Main loop processes queue
void processRequests() {
    ConfigRequest req;
    while (xQueueReceive(requestQueue, &req, 0) == pdTRUE) {
        // Process safely in main context
    }
}
```

---

## Implementation Order

1. **Phase 1 - Quick wins (volatile, critical sections):**
   - Add volatile to systemHalted
   - Add critical section to relay mutex
   - Add critical section to scheduler state reads

2. **Phase 2 - ChannelManager synchronization:**
   - Add mutex for cache access
   - Implement batch update for web config
   - Fix R/M/W races in volume/dosed tracking

3. **Phase 3 - Architecture improvements:**
   - Implement request queue pattern
   - Add transaction semantics to FRAM
   - Refactor validation FSM to single-context
