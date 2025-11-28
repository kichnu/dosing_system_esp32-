DOSING SYSTEM - Technical Specification
HARDWARE
MCU: ESP32-S3-ZERO
Storage: FRAM 32kB (I2C)
RTC: DS3231 (I2C, UTC)
Channels: 1-6 (hardcoded per hardware version)
Pumps: Relay-controlled, ONE active at a time
CONSTANTS & LIMITS
cpp#define CHANNEL_COUNT 6                     // 1-6 depending on hardware
#define MAX_WEEKLY_DOSING_VALUE 1000        // ml
#define MAX_DOSING_DURATION 120             // seconds
#define MAX_SINGLE_DOSE_VOLUME 50           // ml
#define DOSE_TOLERANCE_WINDOW 1800          // seconds (30 min)
#define CALIBRATION_DURATION 30             // seconds
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_HALF_DAY 43200
#define DEVICE_ID "DOLEWKA"
PARAMETERS & VARIABLES
Per-channel user configuration
cppuint8_t  WEEKLY_SCHEDULE;        // bitmask [0-127], bit0=Monday...bit6=Sunday
uint8_t  DAILY_SCHEDULE;         // 1 or 2 (doses per day)
uint16_t WEEKLY_DOSING_VALUE;    // 0-1000 ml
float    DOSING_RATE;            // ml/s (from calibration)
bool     ENABLED;                // channel on/off
Calculated per-channel
cppfloat    SINGLE_DOSING_VOLUME;   // ml
uint16_t DOSING_DURATION;        // seconds
uint32_t DOSING_TIME[2];         // UTC seconds from midnight [first, second]
Runtime state per-channel
cppuint32_t last_dose_timestamp[2]; // UTC timestamp for [first_dose, second_dose]
uint8_t  doses_completed_today;  // 0-2
uint32_t current_utc_day;        // for day rollover detection
ALGORITHMS
1. Calculate single dose volume
cppuint8_t days_active = popcount(WEEKLY_SCHEDULE);
SINGLE_DOSING_VOLUME = WEEKLY_DOSING_VALUE / (days_active * DAILY_SCHEDULE);
2. Calculate pump duration
cppDOSING_DURATION = SINGLE_DOSING_VOLUME / DOSING_RATE;
3. Validation
cppif (DOSING_DURATION > MAX_DOSING_DURATION || 
    SINGLE_DOSING_VOLUME > MAX_SINGLE_DOSE_VOLUME) {
    return ERROR_INVALID_CONFIGURATION;
}
4. Calculate dosing times
cpp// channel_index = 0-based (0-5)
uint32_t interval = SECONDS_PER_HALF_DAY / CHANNEL_COUNT;

if (DAILY_SCHEDULE == 1) {
    DOSING_TIME[0] = channel_index * interval;
    DOSING_TIME[1] = 0;  // unused
}
else if (DAILY_SCHEDULE == 2) {
    DOSING_TIME[0] = channel_index * interval;
    DOSING_TIME[1] = SECONDS_PER_HALF_DAY + (channel_index * interval);
}
5. Execution logic
cppuint32_t current_utc_seconds = getCurrentUTCSecondsFromMidnight();
uint8_t current_day_of_week = getCurrentDayOfWeek();  // 0=Mon, 6=Sun (ISO 8601)

bool is_active_today = (WEEKLY_SCHEDULE & (1 << current_day_of_week)) != 0;

if (is_active_today && ENABLED) {
    for (int dose_slot = 0; dose_slot < DAILY_SCHEDULE; dose_slot++) {
        uint32_t scheduled_time = DOSING_TIME[dose_slot];
        int32_t time_diff = current_utc_seconds - scheduled_time;
        
        if (time_diff >= 0 && 
            time_diff <= DOSE_TOLERANCE_WINDOW && 
            !dose_already_executed[dose_slot]) {
            
            executeDose(channel_index, dose_slot);
            markDoseCompleted(channel_index, dose_slot);
            logToVPS("DOSE_EXECUTED", channel_index, SINGLE_DOSING_VOLUME);
        }
    }
}
6. Day rollover detection
cppuint32_t new_utc_day = getCurrentUTCDay();
if (new_utc_day != current_utc_day) {
    doses_completed_today = 0;
    current_utc_day = new_utc_day;
    resetDailyFlags();
}
FRAM STRUCTURE
cppstruct ChannelConfig {
    // User settings (24 bytes)
    uint8_t  weekly_schedule;        // bitmask
    uint8_t  daily_schedule;         // 1 or 2
    uint16_t weekly_dosing_value;    // ml
    float    dosing_rate;            // ml/s
    uint8_t  enabled;                // 0/1
    uint8_t  reserved[3];            // alignment
    
    // Runtime state (20 bytes)
    uint32_t last_dose_timestamp[2]; // [first, second]
    uint8_t  doses_completed_today;
    uint8_t  padding[3];
    uint32_t current_utc_day;
    
    // Checksum (4 bytes)
    uint32_t crc32;
};

// Total per channel: 48 bytes
// For 6 channels: 288 bytes
// System config: ~200 bytes
// Total FRAM: <512 bytes
WEEKLY_SCHEDULE PRESETS
cpp#define SCHEDULE_SUNDAY_ONLY      0b0000001   // 1
#define SCHEDULE_WED_SUN          0b0100100   // 36
#define SCHEDULE_TUE_THU_SUN      0b0101010   // 42
#define SCHEDULE_WORKDAYS         0b0011111   // 31 (Mon-Fri)
#define SCHEDULE_WEEKEND          0b1100000   // 96
#define SCHEDULE_EVERY_DAY        0b1111111   // 127
API ENDPOINTS
cpp// Configuration
GET  /api/dosing-config              // All channels + today's schedule
POST /api/dosing-config              // Update channel (requires password)
POST /api/calibrate-channel          // 30s calibration run

// Manual control
POST /api/manual-dose                // Trigger single dose

// Status
GET  /api/dosing-status              // Current state all channels
API Request/Response Examples
json// GET /api/dosing-config
{
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
      "status_morning": "completed",
      "status_evening": "pending"
    }
  ],
  "current_utc_time": 1729512345,
  "current_utc_day": 19996
}

// POST /api/dosing-config
{
  "password": "...",
  "channel_id": 1,
  "config": {
    "enabled": true,
    "weekly_schedule": 127,
    "daily_schedule": 2,
    "weekly_dosing_value": 250
  }
}

// POST /api/calibrate-channel
{
  "password": "...",
  "channel_id": 1,
  "measured_ml": 9.8
}
// Response: { "success": true, "dosing_rate": 0.327 }
VPS LOGGING EVENTS
cpp"DOSE_EXECUTED"    // Normal scheduled dose
"DOSE_MISSED"      // Tolerance window exceeded
"DOSE_MANUAL"      // Manual trigger from GUI
"CALIBRATION"      // Calibration completed
"CONFIG_CHANGED"   // User modified settings
```

## GUI ARCHITECTURE

### Layout Structure
```
- Two horizontal tracks (morning 00:00-12:00 CET, evening 12:00-24:00 CET)
- Each track: 1-6 tiles (fixed count = CHANNEL_COUNT)
- Tile states: view (default), edit (on click), active (pumping)
Tile States & Colors
css.pending    { border: #ffc107; background: #fffbf0; }  /* yellow */
.completed  { border: #28a745; background: #f0f9f4; }  /* green */
.active     { border: #667eea; background: #f0f3ff; animation: pulse; }  /* blue */
.disabled   { border: #ccc; background: #f5f5f5; opacity: 0.6; }  /* gray */
.edit-mode  { border: #667eea 3px; box-shadow: 0 8px 30px rgba(102,126,234,0.3); }
```

### Tile Content (View Mode)
```
┌─────────────────────────┐
│ Channel 1      [✓ Done] │
│                         │
│ Weekly: 217 ml          │
│ Single: 15.5 ml         │
│ Duration: 47 sec        │
│ Schedule: Every day     │
│ ─────────────────────   │
│       01:00 CET         │
└─────────────────────────┘
```

### Tile Content (Edit Mode)
```
┌─────────────────────────┐
│ Channel 1          [⚙️] │
│                         │
│ [Enabled ▼]             │
│ [Every day ▼]           │
│ [2x per day ▼]          │
│ [217 ml]                │
│                         │
│ Calculated:             │
│ Single: 15.5 ml         │
│ Duration: 47 sec        │
│                         │
│ [💾 Save] [✖️ Cancel]   │
│ [🔧 Calibrate (30s)]    │
└─────────────────────────┘
```

### Evening Tile Behavior
```
if (DAILY_SCHEDULE == 1) {
    evening_tile.status = "disabled";
    evening_tile.classList.add("disabled");
    evening_tile.onclick = null;  // cannot edit
}
GUI CODE SNIPPET
html<!-- Minimal functional example -->
<style>
.timeline-track { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
.channel-tile { border: 2px solid #ddd; border-radius: 10px; padding: 15px; cursor: pointer; }
.channel-tile.pending { border-color: #ffc107; background: #fffbf0; }
.channel-tile.completed { border-color: #28a745; background: #f0f9f4; }
.channel-tile.active { border-color: #667eea; animation: pulse 2s infinite; }
.channel-tile.disabled { border-color: #ccc; background: #f5f5f5; opacity: 0.6; }
.channel-tile.edit-mode { border-width: 3px; box-shadow: 0 8px 30px rgba(102,126,234,0.3); }
.tile-view-content { display: block; }
.tile-edit-content { display: none; }
.channel-tile.edit-mode .tile-view-content { display: none; }
.channel-tile.edit-mode .tile-edit-content { display: flex; flex-direction: column; gap: 10px; }
</style>

<div class="dose-timeline">
    <div class="timeline-label">🌅 Morning (00:00-12:00 CET)</div>
    <div class="timeline-track" id="morningTrack">
        <!-- 6 tiles generated dynamically -->
    </div>
</div>

<div class="dose-timeline">
    <div class="timeline-label">🌙 Evening (12:00-24:00 CET)</div>
    <div class="timeline-track" id="eveningTrack">
        <!-- 6 tiles, some disabled if DAILY_SCHEDULE=1 -->
    </div>
</div>

<script>
function renderTile(channel, period, channelIndex) {
    const interval = 43200 / CHANNEL_COUNT;
    const baseTime = period === 'morning' ? 0 : 43200;
    const timeUTC = baseTime + (channelIndex * interval);
    const timeCET = formatTime(timeUTC + 3600);  // +1h for CET
    
    const tile = document.createElement('div');
    tile.className = `channel-tile ${channel.status}`;
    if (period === 'evening' && channel.daily_schedule === 1) {
        tile.classList.add('disabled');
    }
    
    tile.innerHTML = `
        <div class="tile-view-content">
            <div>Channel ${channel.id} <span class="badge">${channel.status}</span></div>
            <div>Weekly: ${channel.weekly_dosing_value} ml</div>
            <div>Single: ${channel.single_dose_volume.toFixed(1)} ml</div>
            <div>Duration: ${channel.dosing_duration} sec</div>
            <div class="time">${timeCET}</div>
        </div>
        <div class="tile-edit-content">
            <select id="weekly_${channel.id}">
                <option value="127">Every day</option>
                <option value="31">Workdays</option>
            </select>
            <select id="daily_${channel.id}">
                <option value="1">1x/day</option>
                <option value="2">2x/day</option>
            </select>
            <input type="number" id="volume_${channel.id}" value="${channel.weekly_dosing_value}">
            <button onclick="save(${channel.id})">Save</button>
            <button onclick="cancel()">Cancel</button>
        </div>
    `;
    
    tile.onclick = () => enterEditMode(channel.id);
    return tile;
}
</script>
```

## AUTHENTICATION
```
- Shared password with existing water system (ADMIN_PASSWORD_HASH in FRAM)
- Modal popup before saving config changes
- Session-based (reuse existing session_manager)
TIME HANDLING
cpp// All internal: UTC
// Display to user: CET (Poland) = UTC+1 (or UTC+2 in DST)
// Week starts: Monday (ISO 8601)
// Day rollover: 00:00:00 UTC

uint32_t getCurrentUTCDay() {
    return rtc.getEpoch() / 86400;
}

uint8_t getCurrentDayOfWeek() {
    // 0=Monday, 6=Sunday (ISO 8601)
    return ((getCurrentUTCDay() + 3) % 7);  // epoch started Thursday
}

uint32_t getCurrentUTCSecondsFromMidnight() {
    return rtc.getEpoch() % 86400;
}
```

## INTEGRATION WITH EXISTING SYSTEM
```
Reuse from water system:
- WiFi manager
- VPS logger
- Auth manager (ADMIN_PASSWORD_HASH)
- Session manager
- Rate limiter
- FRAM controller
- RTC controller
- Web server (AsyncWebServer)
- HTML/CSS framework

New modules needed:
- dosing_config.cpp/h
- dosing_scheduler.cpp/h
- channel_controller.cpp/h
- dosing_algorithm.cpp/h
- dosing_web_handlers.cpp/h
```

## STATE MACHINE (Simplified)
```
States per channel:
- IDLE: waiting for scheduled time
- SCHEDULED: within tolerance window
- PUMPING: actively dosing
- COMPLETED: dose finished
- ERROR: validation failed

Global state:
- Check every 10 seconds if any channel needs to execute
- Only ONE channel can be PUMPING at a time
- Queue system if multiple channels scheduled at same time
KEY DESIGN DECISIONS

Fixed layout: Always show CHANNEL_COUNT tiles (no dynamic hiding)
Evening disabled not hidden: When DAILY_SCHEDULE=1, evening tile gray but visible
In-place editing: Click tile → edit mode (no separate config page)
Real-time recalculation: Every config change instantly updates all future doses
Tolerance window: ±30min for missed doses due to restart/offline
Sequential execution: Never run 2 pumps simultaneously (hardware constraint)
UTC internally, CET for display: All timestamps UTC, convert only for GUI
Shared auth: Same password system as water system


END OF SPECIFICATION
