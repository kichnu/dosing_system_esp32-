DOSING SYSTEM - CONDENSED SPECIFICATION v1.0
1. SYSTEM OVERVIEW
Target Platform: Seeed XIAO ESP32-S3-ZERO
Integration: Multi-device IoT platform (extends existing Water System)
Core Concept: 6-channel automated dosing with staggered scheduling, FRAM persistence, UTC timing
Key Features

Hardware mutex: Only ONE relay active at any time
Persistence: All config/state in FRAM (survives power loss)
Time accuracy: RTC + NTP sync, UTC internal / CET display
Security: Shared authentication with Water System
Recovery: ±30min tolerance window for missed doses
Calibration: Per-channel ml/s calibration with 30s test run


2. HARDWARE CONFIGURATION
Pin Assignment (ESP32-S3-ZERO)
RELAY_1-6: GPIO 1-6 (D0-D5) - Active HIGH
I2C_SDA:   GPIO 7  (D8)
I2C_SCL:   GPIO 8  (D9)
FRAM:      0x50 (MB85RC256V, 32KB)
RTC:       0x68 (DS3231M)
Critical Limits
MAX_WEEKLY_DOSING_VALUE: 1000 ml
MAX_DOSING_DURATION:     120 seconds
MAX_SINGLE_DOSE_VOLUME:  50 ml
DOSE_TOLERANCE_WINDOW:   1800 seconds (±30 min)
CALIBRATION_DURATION:    30 seconds
CHANNEL_COUNT:           6 (compile-time configurable 1-6)

3. FRAM MEMORY LAYOUT
CRITICAL: Credentials zone (0x0018-0x0418) shared with Water System - DO NOT MODIFY
0x0000: Magic + Version (6B)           [shared]
0x0018: Encrypted Credentials (1024B)  [shared - READ ONLY]
0x0500: SystemConfig (64B)             [dosing new]
0x0540: ChannelConfig[6] (288B)        [dosing new]
0x0660: DailyVolume[6] (96B)           [dosing new]
Data Structures (sizes critical for memory layout)
ChannelConfig:  48 bytes (includes CRC32)
  - User settings: schedule, dosing_value, rate, enabled
  - Runtime state: last_dose_timestamp[2], doses_today
  - Checksum: crc32

DailyVolume:    16 bytes
  - volume_ml, last_reset_utc_day, checksum

SystemConfig:   64 bytes
  - channel_count, system_enabled, version

4. CORE ALGORITHM LOGIC
Calculation Formulas
single_dose_ml = weekly_dosing_value / (days_active × daily_schedule)
duration_sec = single_dose_ml / dosing_rate
days_active = popcount(weekly_schedule_bitmask)
Staggered Dosing Times (prevents relay conflicts)
interval = SECONDS_PER_HALF_DAY / CHANNEL_COUNT  // 43200/6 = 7200s (2h)

If daily_schedule == 1:
  morning = channel_index × interval
  evening = UNUSED

If daily_schedule == 2:
  morning = channel_index × interval
  evening = 43200 + (channel_index × interval)

Example for 6 channels (2x/day):
  CH0: 00:00 + 12:00
  CH1: 02:00 + 14:00
  CH2: 04:00 + 16:00
  CH3: 06:00 + 18:00
  CH4: 08:00 + 20:00
  CH5: 10:00 + 22:00
State Machine
States: IDLE → SCHEDULED → DOSING → COMPLETED
        └─────────────────────────────┘
                    (day rollover resets)

IDLE:      Waiting for scheduled time
SCHEDULED: Within ±30min tolerance window
DOSING:    Relay active, pump running
COMPLETED: Dose finished for today
ERROR:     Config or hardware issue
Critical Events
Day Rollover (midnight UTC):
  - Reset dose_executed[0,1] flags
  - Reset doses_completed_today counter
  - Update current_utc_day in FRAM
  - Trigger: current_utc_day != last_utc_day

Relay Mutex Enforcement (CRITICAL):
  - Before activating relay: check ALL other channels
  - If ANY channel.state == DOSING: BLOCK and WAIT
  - Only ONE relay can be HIGH at any time

5. NETWORKING & VPS INTEGRATION
VPS Event Payload (JSON)
json{
  "device_id": "DOSING_UNIT_001",
  "device_type": "dosing_system",
  "unix_time": <timestamp>,
  "channel_id": 1-6,
  "event_type": "DOSE_EXECUTED|DOSE_MISSED|DOSE_MANUAL|CALIBRATION|CONFIG_CHANGED",
  "volume_ml": <float>,
  "dosing_duration": <seconds>,
  "weekly_schedule": <bitmask>,
  "daily_schedule": 1 or 2,
  "system_status": "OK"
}
VPS Database Schema (NEW TABLE)
sqlCREATE TABLE dosing_events (
    id, device_id, device_type, timestamp, unix_time,
    channel_id, event_type, volume_ml, dosing_duration,
    weekly_schedule, daily_schedule,
    system_status, client_ip
);
Device Config Update
pythonDEVICE_TYPES['dosing_system'] = {
    'name': 'Dosing System',
    'icon': '💧',
    'event_types': ['DOSE_EXECUTED', 'DOSE_MISSED', ...]
}
```

---

## 6. WEB API ENDPOINTS

**Authentication:** Session-based (shared with Water System)
```
GET  /api/dosing-config      - Get all channels + today's schedule
POST /api/dosing-config      - Update channel config (requires password)
POST /api/calibrate-channel  - Run 30s calibration (requires password)
POST /api/manual-dose        - Trigger immediate dose (requires password)
GET  /api/dosing-status      - Real-time status (5s refresh)
Response Structure
json{
  "success": true,
  "channels": [
    {
      "id": 1,
      "enabled": true,
      "weekly_schedule": 127,
      "daily_schedule": 2,
      "weekly_dosing_value": 217,
      "dosing_rate": 0.33,
      "single_dose_volume": 15.5,
      "dosing_duration": 47,
      "dosing_times_utc": [0, 43200],
      "dosing_times_cet": ["01:00", "13:00"],
      "status_morning": "completed|pending|active|waiting",
      "status_evening": "..."
    }
  ]
}
```

---

## 7. GUI ARCHITECTURE

### Layout Concept
```
Timeline View with 2 Tracks:
  🌅 Morning Track (00:00-12:00 CET) - 6 tiles
  🌙 Evening Track (12:00-24:00 CET) - 6 tiles (disabled if daily_schedule=1)

Tile States (CSS classes):
  .pending   - Yellow border, scheduled but not executed
  .active    - Blue border, pulsing animation, relay ON
  .completed - Green border, dose finished
  .disabled  - Grey, opacity 60%, no interaction
  .edit-mode - Purple border, expanded with form
```

### Tile Interaction
```
VIEW MODE (default):
  Click → Enter EDIT MODE

EDIT MODE:
  Form fields:
    - Weekly Schedule (dropdown: presets + custom bitmask)
    - Daily Schedule (1x or 2x)
    - Weekly Volume (0-1000 ml)
    - Calculated display: single dose, duration
  
  Actions:
    - Save → POST /api/dosing-config
    - Cancel → Exit edit mode
    - Calibrate → 30s pump test + measure volume
Real-time Updates
javascriptsetInterval(() => {
  fetch('/api/dosing-status')
  updateTileStates()  // Change CSS classes based on state
}, 5000)
```

---

## 8. CLI PROGRAMMING INTERFACE

### Extended Commands (MODE_PROGRAMMING)
```
channels          - Show all channel configs (table view)
channel <N>       - Interactive configuration for channel N
test-relay <N>    - Test relay N for 3 seconds
calibrate <N>     - Run 30s calibration for channel N
set-channels <1-6>- Set hardware channel count
status            - System overview (RTC, WiFi, channels, memory)

Existing commands (inherited):
program, wifi, admin, vps, version, reset, list

9. BUILD CONFIGURATION
PlatformIO Environments
ini[env:production]
build_flags = 
    -DMODE_PRODUCTION=1
    -DENABLE_WEB_SERVER=1
    -DCHANNEL_COUNT=6

[env:programming]
build_flags =
    -DMODE_PROGRAMMING=1
    -DENABLE_CLI_INTERFACE=1
    -DCHANNEL_COUNT=6
Compile-time Configuration
cpp// Single point of hardware change:
#define CHANNEL_COUNT 6  // platformio.ini or hardware_config.h

// Mode detection:
#if MODE_PRODUCTION
    // Web server, dosing algorithm, WiFi
#elif MODE_PROGRAMMING
    // CLI interface, FRAM programming
#endif
```

---

## 10. IMPLEMENTATION WORKFLOW (17 DAYS)

### Phase 1: Foundation (Days 1-3)
```
ETAP 0: Project setup (PlatformIO, build environments)
ETAP 1: Hardware layer (FRAM, RTC, Relay controller)
✅ Milestone: Hardware communicates, credentials load
```

### Phase 2: Core Logic (Days 4-7)
```
ETAP 2: Dosing algorithm (calculations, scheduler, day rollover)
✅ Milestone: Scheduled dosing works, state persists
```

### Phase 3: Connectivity (Days 8-10)
```
ETAP 3: Networking (WiFi, VPS logger)
ETAP 4: Web API (endpoints, JSON builders)
✅ Milestone: Web API functional, events logged to VPS
```

### Phase 4: User Interface (Days 11-14)
```
ETAP 5: Frontend GUI (HTML, CSS, JavaScript)
ETAP 6: CLI extensions (new commands)
✅ Milestone: Full UI (web + CLI) functional
```

### Phase 5: Deployment (Days 15-17)
```
ETAP 7: Testing (hardware, timing, network, load)
ETAP 8: VPS integration (database, dashboard)
✅ Milestone: Production-ready system
```

---

## 11. CRITICAL SUCCESS FACTORS

### Must-Haves (Non-Negotiable)
1. **Relay Mutex:** NEVER allow 2 relays active simultaneously
2. **FRAM Persistence:** All state must survive power loss
3. **UTC Timing:** Always store UTC, convert to CET only for display
4. **Day Rollover:** Must trigger exactly at midnight UTC
5. **Tolerance Window:** ±30 min recovery for power outages
6. **Staggered Dosing:** No channel conflicts (2h spacing)

### Testing Checklist
```
[ ] Only ONE relay active (mutex test)
[ ] Config persists across power cycles
[ ] Day rollover triggers at midnight UTC
[ ] Missed doses logged if outside ±30min window
[ ] All 6 relays work individually
[ ] VPS receives events (check database)
[ ] 24-hour stability test passes
[ ] Network dropout recovery works
```

---

## 12. INTEGRATION NOTES

### Files to COPY AS-IS from Water System
```
✅ src/core/logging.*
✅ src/hardware/rtc_controller.*
✅ src/crypto/* (entire folder)
✅ src/network/wifi_manager.*
✅ src/security/* (entire folder)
✅ src/config/credentials_manager.*
```

### Files to ADAPT
```
🔧 src/hardware/fram_controller.* (new memory layout)
🔧 src/network/vps_logger.* (dosing event payload)
🔧 src/web/html_pages.* (dosing GUI)
🔧 src/cli/cli_handler.* (add commands)
```

### Files to CREATE NEW
```
🆕 src/hardware/hardware_pins.h (ESP32-S3 pinout)
🆕 src/hardware/relay_controller.*
🆕 src/algorithm/dosing_scheduler.*
🆕 src/algorithm/channel_controller.*
🆕 src/web/dosing_handlers.* (API endpoints)
```

---

## 13. QUICK REFERENCE

### Weekly Schedule Presets (Bitmask)
```
127 (0b1111111) - Every day
31  (0b0011111) - Workdays (Mon-Fri)
96  (0b1100000) - Weekend (Sat-Sun)
1   (0b0000001) - Sunday only
42  (0b0101010) - Tue/Thu/Sun

Bit mapping: bit0=Monday, bit1=Tuesday, ..., bit6=Sunday
```

### Calculation Example
```
Input: 217ml/week, every day, 2x/day, rate=0.33ml/s
  → days_active = 7
  → single_dose = 217/(7×2) = 15.5 ml
  → duration = 15.5/0.33 = 47 sec
  → CH0 times: 00:00 UTC (01:00 CET), 12:00 UTC (13:00 CET)
