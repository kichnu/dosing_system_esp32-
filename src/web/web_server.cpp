/**
 * DOZOWNIK - Web Server Implementation
 */

#include <ArduinoJson.h>
#include "web_server.h"
#include "html_pages.h"
#include "../config/config.h"
#include "../security/session_manager.h"
#include "../security/auth_manager.h"
#include "../algorithm/channel_manager.h"
#include "../hardware/dosing_scheduler.h"
#include "../hardware/rtc_controller.h"
#include "daily_log.h"

// ============================================================================
// SERVER INSTANCE
// ============================================================================
AsyncWebServer server(80);
bool serverRunning = false;

// ============================================================================
// SESSION HELPERS
// ============================================================================

String getSessionToken(AsyncWebServerRequest* request) {
    if (request->hasHeader("Cookie")) {
        String cookie = request->header("Cookie");
        int idx = cookie.indexOf("session=");
        if (idx >= 0) {
            int start = idx + 8;
            int end = cookie.indexOf(";", start);
            if (end < 0) end = cookie.length();
            return cookie.substring(start, end);
        }
    }
    return "";
}

bool isAuthenticated(AsyncWebServerRequest* request) {
    String token = getSessionToken(request);
    if (token.length() == 0) return false;
    return validateSession(token, request->client()->remoteIP());
}

// ============================================================================
// ROUTE HANDLERS
// ============================================================================

void handleRoot(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->redirect("/login");
        return;
    }
    request->send(200, "text/html", getDashboardHTML());
}

void handleLogin(AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
        request->redirect("/");
        return;
    }
    request->send(200, "text/html", getLoginHTML());
}

void handleApiLogin(AsyncWebServerRequest* request) {
    if (!request->hasParam("password", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing password\"}");
        return;
    }
    
    String password = request->getParam("password", true)->value();
    
    if (verifyPassword(password)) {
        String token = createSession(request->client()->remoteIP());
        
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", 
            "{\"success\":true}");
        response->addHeader("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
        request->send(response);
        
        Serial.printf("[WEB] Login OK from %s\n", request->client()->remoteIP().toString().c_str());
    } else {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Invalid password\"}");
        Serial.printf("[WEB] Login FAILED from %s\n", request->client()->remoteIP().toString().c_str());
    }
}

void handleApiLogout(AsyncWebServerRequest* request) {
    String token = getSessionToken(request);
    if (token.length() > 0) {
        destroySession(token);
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", 
        "{\"success\":true}");
    response->addHeader("Set-Cookie", "session=; Path=/; HttpOnly; Max-Age=0");
    request->send(response);
    
    Serial.println(F("[WEB] Logout"));
}

// ============================================================================
// API: DOSING STATUS
// ============================================================================

void handleApiDosingStatus(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    JsonDocument doc;
    
    // System status
    doc["systemOk"] = !systemHalted;
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    doc["schedulerEnabled"] = dosingScheduler.isEnabled();
    
    // Active dosing info
    if (relayController.isAnyOn()) {
        doc["activeChannel"] = relayController.getActiveChannel();
        doc["activeEventHour"] = dosingScheduler.getCurrentEvent().hour;
        doc["activeRemainingMs"] = relayController.getRemainingTime();
    } else {
        doc["activeChannel"] = -1;
        doc["activeEventHour"] = -1;
        doc["activeRemainingMs"] = 0;
    }
    
    // Time
    if (rtcController.isReady()) {
        TimeInfo now = rtcController.getTime();
        char timeStr[6];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.hour, now.minute);
        doc["time"] = timeStr;
        doc["dayOfWeek"] = now.dayOfWeek;
    }
    
    // Channels array
    JsonArray channels = doc["channels"].to<JsonArray>();
    
for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
        const ChannelConfig& active = channelManager.getActiveConfig(i);
        const ChannelConfig& pending = channelManager.getPendingConfig(i);
        const ChannelDailyState& daily = channelManager.getDailyState(i);
        const ChannelCalculated& calc = channelManager.getCalculated(i);
        
        // Use pending config if has changes, otherwise active
        const ChannelConfig& cfg = pending.has_pending ? pending : active;
        
        JsonObject ch = channels.add<JsonObject>();
        
        ch["id"] = i;
        ch["events"] = cfg.events_bitmask;
        ch["days"] = cfg.days_bitmask;
        ch["dailyDose"] = cfg.daily_dose_ml;
        ch["dosingRate"] = cfg.dosing_rate;
        ch["enabled"] = cfg.enabled ? true : false;
        
        ch["eventsCompleted"] = daily.events_completed;
        ch["eventsFailed"] = daily.events_failed;
        ch["failedToday"] = daily.failed_count;
        ch["todayDosed"] = daily.today_added_ml;
        
        ch["singleDose"] = calc.single_dose_ml;
        ch["pumpDurationMs"] = calc.pump_duration_ms;
        ch["weeklyDose"] = calc.weekly_dose_ml;
        ch["activeEvents"] = calc.active_events_count;
        ch["activeDays"] = calc.active_days_count;
        ch["completedToday"] = calc.completed_today_count;
        ch["isValid"] = calc.is_valid;
        
        ch["hasPending"] = pending.has_pending ? true : false;
        
        // State string for GUI
        const char* stateStr;
        switch (calc.state) {
            case CH_STATE_INACTIVE:   stateStr = "inactive"; break;
            case CH_STATE_INCOMPLETE: stateStr = "incomplete"; break;
            case CH_STATE_INVALID:    stateStr = "invalid"; break;
            case CH_STATE_CONFIGURED: stateStr = "configured"; break;
            case CH_STATE_PENDING:    stateStr = "pending"; break;
            default:                  stateStr = "unknown"; break;
        }
        ch["state"] = stateStr;

                // Container volume
        const ContainerVolume& vol = channelManager.getContainerVolume(i);
        ch["containerMl"] = vol.getContainerMl();
        ch["remainingMl"] = vol.getRemainingMl();
        ch["remainingPct"] = vol.getRemainingPercent();
        ch["lowVolume"] = vol.isLowVolume();
        ch["daysRemaining"] = channelManager.getDaysRemaining(i);
    }
    
    // Serialize and send
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: DOSING CONFIG (POST) - Save channel configuration
// ============================================================================

void handleApiDosingConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    // Accumulate body data
    static String bodyBuffer;
    
    if (index == 0) {
        bodyBuffer = "";
    }
    
    bodyBuffer += String((char*)data).substring(0, len);
    
    // Wait for complete body
    if (index + len < total) {
        return;
    }
    
    // Auth check
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        bodyBuffer = "";
        return;
    }
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, bodyBuffer);
    bodyBuffer = "";
    
    if (err) {
        Serial.printf("[WEB] JSON parse error: %s\n", err.c_str());
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate channel
    if (!doc.containsKey("channel")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing channel\"}");
        return;
    }
    
    uint8_t channel = doc["channel"].as<uint8_t>();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid channel\"}");
        return;
    }
    
    Serial.printf("[WEB] Config update CH%d\n", channel);
    
    // Apply configuration to PENDING
    bool success = true;
    
    if (doc.containsKey("events")) {
        uint32_t events = doc["events"].as<uint32_t>();
        success &= channelManager.setEventsBitmask(channel, events);
        Serial.printf("  Events: 0x%06X\n", events);
    }
    
    if (doc.containsKey("days")) {
        uint8_t days = doc["days"].as<uint8_t>();
        success &= channelManager.setDaysBitmask(channel, days);
        Serial.printf("  Days: 0x%02X\n", days);
    }
    
    if (doc.containsKey("dailyDose")) {
        float dose = doc["dailyDose"].as<float>();
        success &= channelManager.setDailyDose(channel, dose);
        Serial.printf("  Dose: %.2f ml\n", dose);
    }
    
    if (doc.containsKey("dosingRate")) {
        float rate = doc["dosingRate"].as<float>();
        success &= channelManager.setDosingRate(channel, rate);
        Serial.printf("  Rate: %.3f ml/s\n", rate);
    }
    
    // Validate config
    ValidationError valErr;
    bool valid = channelManager.validateConfig(channel, &valErr);
    
    if (!valid) {
        Serial.printf("[WEB] Validation: %s\n", valErr.message);
    }
    
    // Build response
    JsonDocument resp;
    resp["success"] = success;
    resp["valid"] = valid;
    resp["hasPending"] = channelManager.hasPendingChanges(channel);
    
    if (!valid && valErr.has_error) {
        resp["validationError"] = valErr.message;
    }
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
    
    Serial.printf("[WEB] Config saved: success=%d valid=%d pending=%d\n", 
                  success, valid, channelManager.hasPendingChanges(channel));
}

// ============================================================================
// API: CALIBRATE (POST) - Run pump for calibration
// ============================================================================

void handleApiCalibrate(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Get channel from query param: /api/calibrate?channel=0
    if (!request->hasParam("channel")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing channel\"}");
        return;
    }
    
    uint8_t channel = request->getParam("channel")->value().toInt();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid channel\"}");
        return;
    }
    
    Serial.printf("[WEB] Calibration request CH%d\n", channel);
    
    // Check if any pump already running
    if (relayController.isAnyOn()) {
        request->send(409, "application/json", "{\"success\":false,\"error\":\"Pump busy\"}");
        return;
    }
    
    // Run pump for 30 seconds
    const uint32_t CALIB_DURATION_MS = 30000;
    
    RelayResult res = relayController.turnOn(channel, CALIB_DURATION_MS);
    
    if (res != RelayResult::OK) {
        String errJson = "{\"success\":false,\"error\":\"";
        errJson += RelayController::resultToString(res);
        errJson += "\"}";
        request->send(500, "application/json", errJson);
        return;
    }
    
    Serial.printf("[WEB] Calibration started CH%d for %lu ms\n", channel, CALIB_DURATION_MS);
    
    // Success response
    JsonDocument resp;
    resp["success"] = true;
    resp["channel"] = channel;
    resp["durationMs"] = CALIB_DURATION_MS;
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: SCHEDULER (POST) - Enable/disable scheduler
// ============================================================================

void handleApiScheduler(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Check for 'enabled' param
    if (!request->hasParam("enabled", true)) {
        // No param = GET current state
        JsonDocument resp;
        resp["enabled"] = dosingScheduler.isEnabled();
        resp["state"] = DosingScheduler::stateToString(dosingScheduler.getState());
        
        String response;
        serializeJson(resp, response);
        request->send(200, "application/json", response);
        return;
    }
    
    // Set enabled state
    String val = request->getParam("enabled", true)->value();
    bool enabled = (val == "true" || val == "1");
    
    dosingScheduler.setEnabled(enabled);
    
    Serial.printf("[WEB] Scheduler %s\n", enabled ? "ENABLED" : "DISABLED");
    
    JsonDocument resp;
    resp["success"] = true;
    resp["enabled"] = dosingScheduler.isEnabled();
    resp["state"] = DosingScheduler::stateToString(dosingScheduler.getState());
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: MANUAL DOSE (POST) - Trigger manual dosing
// ============================================================================

void handleApiManualDose(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Get channel
    if (!request->hasParam("channel", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing channel\"}");
        return;
    }
    
    uint8_t channel = request->getParam("channel", true)->value().toInt();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid channel\"}");
        return;
    }
    
    Serial.printf("[WEB] Manual dose request CH%d\n", channel);
    
    // Check scheduler
    if (!dosingScheduler.isEnabled()) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Scheduler disabled\"}");
        return;
    }
    
    // Check if pump busy
    if (relayController.isAnyOn()) {
        request->send(409, "application/json", "{\"success\":false,\"error\":\"Pump busy\"}");
        return;
    }
    
    // Trigger dose
    if (!dosingScheduler.triggerManualDose(channel)) {
        request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to start\"}");
        return;
    }
    
    const ChannelCalculated& calc = channelManager.getCalculated(channel);
    
    JsonDocument resp;
    resp["success"] = true;
    resp["channel"] = channel;
    resp["doseMl"] = calc.single_dose_ml;
    resp["durationMs"] = calc.pump_duration_ms;
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
    
    Serial.printf("[WEB] Manual dose started CH%d: %.2f ml\n", channel, calc.single_dose_ml);
}

// ============================================================================
// API: DAILY RESET (POST) - Force daily reset
// ============================================================================

void handleApiDailyReset(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    Serial.println(F("[WEB] Forcing daily reset..."));
    
    bool success = dosingScheduler.forceDailyReset();
    
    JsonDocument resp;
    resp["success"] = success;
    resp["message"] = success ? "Daily reset complete" : "Reset failed";
    resp["pendingApplied"] = !channelManager.hasAnyPendingChanges();
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
    
    Serial.printf("[WEB] Daily reset: %s\n", success ? "OK" : "FAILED");
}

// ============================================================================
// API: DAILY LOGS - Lista wpisów historii
// ============================================================================

// void handleApiDailyLogs(AsyncWebServerRequest* request) {
//     if (!isAuthenticated(request)) {
//         request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
//         return;
//     }
    
//     if (!g_dailyLog || !g_dailyLog->isInitialized()) {
//         request->send(503, "application/json", "{\"error\":\"Daily log not initialized\"}");
//         return;
//     }
    
//     JsonDocument doc;
    
//     // Stats
//     DailyLogStats stats = g_dailyLog->getStats();
//     doc["count"] = stats.count;
//     doc["capacity"] = stats.capacity;
//     doc["totalWritten"] = stats.total_written;
//     doc["hasCurrentDay"] = stats.has_current_day;
    
//     // Lista wpisów (ostatnie 30 sfinalizowanych)
//     JsonArray entries = doc["entries"].to<JsonArray>();
    
//     uint8_t maxEntries = 30;
//     uint8_t finalizedCount = g_dailyLog->getFinalizedCount();
//     uint8_t toFetch = (finalizedCount < maxEntries) ? finalizedCount : maxEntries;
    
//     for (uint8_t i = 0; i < toFetch; i++) {
//         DayLogEntry entry;
//         if (g_dailyLog->getEntry(i, entry) == DailyLogResult::OK) {
//             JsonObject e = entries.add<JsonObject>();
//             e["index"] = i;
//             e["utcDay"] = entry.utc_day;
//             e["dayOfWeek"] = entry.day_of_week;
//             e["flags"] = entry.flags;
//             e["channelCount"] = entry.channel_count;
            
//             // Agregowany status
//             DayChannelStatus aggStatus = entry.getAggregatedStatus();
//             const char* statusStr;
//             switch (aggStatus) {
//                 case DayChannelStatus::OK: statusStr = "ok"; break;
//                 case DayChannelStatus::PARTIAL: statusStr = "partial"; break;
//                 case DayChannelStatus::ERROR: statusStr = "error"; break;
//                 default: statusStr = "inactive"; break;
//             }
//             e["status"] = statusStr;
            
//             // Podsumowanie kanałów
//             uint8_t completedChannels = 0;
//             uint8_t errorChannels = 0;
//             float totalDosed = 0;
            
//             for (int ch = 0; ch < DAILY_LOG_MAX_CHANNELS; ch++) {
//                 if (entry.channels[ch].status == DayChannelStatus::OK) completedChannels++;
//                 if (entry.channels[ch].status == DayChannelStatus::ERROR) errorChannels++;
//                 totalDosed += entry.channels[ch].getDoseActualMl();
//             }
            
//             e["completedChannels"] = completedChannels;
//             e["errorChannels"] = errorChannels;
//             e["totalDosedMl"] = totalDosed;
//             e["powerCycles"] = entry.power_cycles;
//         }
//     }
    
//     String response;
//     serializeJson(doc, response);
//     request->send(200, "application/json", response);
// }

// #############################################################################################################################################################

void handleApiDailyLogs(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (!g_dailyLog || !g_dailyLog->isInitialized()) {
        request->send(503, "application/json", "{\"error\":\"Daily log not initialized\"}");
        return;
    }
    
    JsonDocument doc;
    
    // Stats
    DailyLogStats stats = g_dailyLog->getStats();
    doc["count"] = stats.count;
    doc["capacity"] = stats.capacity;
    doc["totalWritten"] = stats.total_written;
    doc["hasCurrentDay"] = stats.has_current_day;
    
    // Lista wpisów (ostatnie 30)
    JsonArray entries = doc["entries"].to<JsonArray>();
    
    uint8_t maxEntries = 30;
    uint8_t totalCount = stats.count;
    uint8_t toFetch = (totalCount < maxEntries) ? totalCount : maxEntries;
    
    // Index 0 = current day, Index 1+ = finalized (zgodnie z handleApiDailyLogEntry)
    for (uint8_t i = 0; i < toFetch; i++) {
        DayLogEntry entry;
        DailyLogResult res;
        
        if (i == 0) {
            res = g_dailyLog->getCurrentEntry(entry);
        } else {
            res = g_dailyLog->getEntry(i - 1, entry);
        }
        
        if (res != DailyLogResult::OK) continue;
        
        JsonObject e = entries.add<JsonObject>();
        e["index"] = i;
        e["utcDay"] = entry.utc_day;
        e["dayOfWeek"] = entry.day_of_week;
        e["flags"] = entry.flags;
        e["channelCount"] = entry.channel_count;
        e["isCurrent"] = (i == 0 && !entry.isFinalized());
        
        // Agregowany status
        DayChannelStatus aggStatus = entry.getAggregatedStatus();
        const char* statusStr;
        switch (aggStatus) {
            case DayChannelStatus::OK: statusStr = "ok"; break;
            case DayChannelStatus::PARTIAL: statusStr = "partial"; break;
            case DayChannelStatus::ERROR: statusStr = "error"; break;
            default: statusStr = "inactive"; break;
        }
        e["status"] = statusStr;
        
        // Podsumowanie kanałów
        uint8_t completedChannels = 0;
        uint8_t errorChannels = 0;
        float totalDosed = 0;
        
        for (int ch = 0; ch < DAILY_LOG_MAX_CHANNELS; ch++) {
            if (entry.channels[ch].status == DayChannelStatus::OK) completedChannels++;
            if (entry.channels[ch].status == DayChannelStatus::ERROR) errorChannels++;
            totalDosed += entry.channels[ch].getDoseActualMl();
        }
        
        e["completedChannels"] = completedChannels;
        e["errorChannels"] = errorChannels;
        e["totalDosedMl"] = totalDosed;
        e["powerCycles"] = entry.power_cycles;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ############################################################################################################

// void handleApiDailyLogs(AsyncWebServerRequest* request) {
//     if (!isAuthenticated(request)) {
//         request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
//         return;
//     }
    
//     if (!g_dailyLog || !g_dailyLog->isInitialized()) {
//         request->send(503, "application/json", "{\"error\":\"Daily log not initialized\"}");
//         return;
//     }
    
//     JsonDocument doc;
    
//     // Stats
//     DailyLogStats stats = g_dailyLog->getStats();
//     doc["count"] = stats.count;
//     doc["capacity"] = stats.capacity;
//     doc["totalWritten"] = stats.total_written;
//     doc["hasCurrentDay"] = stats.has_current_day;
//     doc["debug_finalizedCount"] = g_dailyLog->getFinalizedCount();
    
//     // Lista wpisów
//     JsonArray entries = doc["entries"].to<JsonArray>();
    
//     // Index 0 = current
//     DayLogEntry currentEntry;
//     if (g_dailyLog->getCurrentEntry(currentEntry) == DailyLogResult::OK) {
//         JsonObject e = entries.add<JsonObject>();
//         e["index"] = 0;
//         e["utcDay"] = currentEntry.utc_day;
//         e["dayOfWeek"] = currentEntry.day_of_week;
//         e["isCurrent"] = true;
//         e["status"] = "current";
//         e["totalDosedMl"] = 0;
//         for (int ch = 0; ch < DAILY_LOG_MAX_CHANNELS; ch++) {
//             e["totalDosedMl"] = (float)e["totalDosedMl"] + currentEntry.channels[ch].getDoseActualMl();
//         }
//     }
    
//     // Debug: próbuj pobrać pierwsze 5 wpisów i pokaż wynik
//     JsonArray debug = doc["debug_getEntry"].to<JsonArray>();
//     for (uint8_t i = 0; i < 5; i++) {
//         DayLogEntry entry;
//         DailyLogResult res = g_dailyLog->getEntry(i, entry);
//         JsonObject d = debug.add<JsonObject>();
//         d["i"] = i;
//         d["result"] = static_cast<int>(res);
//         if (res == DailyLogResult::OK) {
//             d["utcDay"] = entry.utc_day;
//             d["isFinalized"] = entry.isFinalized();
//         }
//     }

    
    
//     String response;
//     serializeJson(doc, response);
//     request->send(200, "application/json", response);

    
// }

// ============================================================================
// API: DAILY LOG ENTRY - Szczegóły pojedynczego wpisu
// ============================================================================

void handleApiDailyLogEntry(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (!g_dailyLog || !g_dailyLog->isInitialized()) {
        request->send(503, "application/json", "{\"error\":\"Daily log not initialized\"}");
        return;
    }
    
    // Pobierz index z query param: /api/daily-log?index=0
    uint8_t index = 0;
    if (request->hasParam("index")) {
        index = request->getParam("index")->value().toInt();
    }
    
    DayLogEntry entry;
    DailyLogResult result;
    
    // Index 0 = bieżący dzień (niesfinalizowany)
    // Index 1+ = sfinalizowane wpisy (1 = najnowszy sfinalizowany)
    if (index == 0) {
        result = g_dailyLog->getCurrentEntry(entry);
    } else {
        result = g_dailyLog->getEntry(index - 1, entry);
    }
    
    // if (result != DailyLogResult::OK) {
    //     request->send(404, "application/json", "{\"error\":\"Entry not found\"}");
    //     return;
    // }

    if (result != DailyLogResult::OK) {
    JsonDocument errDoc;
    errDoc["error"] = "Entry not found";
    errDoc["debug_index"] = index;
    errDoc["debug_result"] = static_cast<int>(result);
    errDoc["debug_finalizedCount"] = g_dailyLog->getFinalizedCount();
    String response;
    serializeJson(errDoc, response);
    request->send(404, "application/json", response);
    return;
}
    
    JsonDocument doc;
    
    // Header
    doc["index"] = index;
    doc["utcDay"] = entry.utc_day;
    doc["dayOfWeek"] = entry.day_of_week;
    doc["flags"] = entry.flags;
    doc["version"] = entry.version;
    doc["channelCount"] = entry.channel_count;
    doc["isFinalized"] = entry.isFinalized();
    doc["hasCriticalError"] = entry.hasCriticalError();
    doc["hadPowerLoss"] = entry.hadPowerLoss();
    
    // Channels array
    JsonArray channels = doc["channels"].to<JsonArray>();
    
    for (int i = 0; i < DAILY_LOG_MAX_CHANNELS; i++) {
        const DayChannelData& ch = entry.channels[i];
        JsonObject chObj = channels.add<JsonObject>();
        
        chObj["id"] = i;
        chObj["eventsPlanned"] = ch.events_planned;
        chObj["eventsCompleted"] = ch.events_completed;
        chObj["eventsFailed"] = ch.events_failed;
        chObj["dosePlannedMl"] = ch.getDosePlannedMl();
        chObj["doseActualMl"] = ch.getDoseActualMl();
        
        const char* statusStr;
        switch (ch.status) {
            case DayChannelStatus::OK: statusStr = "ok"; break;
            case DayChannelStatus::PARTIAL: statusStr = "partial"; break;
            case DayChannelStatus::SKIPPED: statusStr = "skipped"; break;
            case DayChannelStatus::ERROR: statusStr = "error"; break;
            default: statusStr = "inactive"; break;
        }
        chObj["status"] = statusStr;
        
        if (ch.hasError()) {
            chObj["errorType"] = static_cast<uint8_t>(ch.error_type);
            chObj["errorHour"] = ch.error_hour;
            chObj["errorMinute"] = ch.error_minute;
        }
    }
    
    // System info
    JsonObject sysInfo = doc["system"].to<JsonObject>();
    sysInfo["uptimeSeconds"] = entry.uptime_seconds;
    sysInfo["powerCycles"] = entry.power_cycles;
    sysInfo["wifiDisconnects"] = entry.wifi_disconnects;
    sysInfo["ntpSyncs"] = entry.ntp_syncs;
    sysInfo["framWrites"] = entry.fram_writes;
    sysInfo["minHeapKb"] = entry.min_heap_kb;
    sysInfo["maxTempC"] = entry.max_temp_c;
    
    // Critical error (if any)
    if (entry.hasCriticalError()) {
        JsonObject critErr = doc["criticalError"].to<JsonObject>();
        critErr["type"] = entry.critical_error_type;
        critErr["channel"] = entry.critical_error_channel;
        critErr["hour"] = entry.critical_error_hour;
        critErr["minute"] = entry.critical_error_minute;
    }
    
    // Integrity
    doc["writeTimestamp"] = entry.write_timestamp;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: CONTAINER VOLUME - Get container status
// ============================================================================

void handleApiContainerVolumeGet(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Get channel from query param: /api/container-volume?channel=0
    if (!request->hasParam("channel")) {
        request->send(400, "application/json", "{\"error\":\"Missing channel\"}");
        return;
    }
    
    uint8_t channel = request->getParam("channel")->value().toInt();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"error\":\"Invalid channel\"}");
        return;
    }
    
    const ContainerVolume& vol = channelManager.getContainerVolume(channel);
    
    JsonDocument doc;
    doc["channel"] = channel;
    doc["container_ml"] = vol.getContainerMl();
    doc["remaining_ml"] = vol.getRemainingMl();
    doc["remaining_pct"] = vol.getRemainingPercent();
    doc["low_warning"] = vol.isLowVolume();
    doc["days_remaining"] = channelManager.getDaysRemaining(channel);
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: CONTAINER VOLUME - Set container capacity (POST with JSON body)
// ============================================================================

void handleApiContainerVolumeSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String bodyBuffer;
    
    if (index == 0) {
        bodyBuffer = "";
    }
    
    bodyBuffer += String((char*)data).substring(0, len);
    
    if (index + len < total) {
        return;
    }
    
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        bodyBuffer = "";
        return;
    }
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, bodyBuffer);
    bodyBuffer = "";
    
    if (err) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("channel") || !doc.containsKey("container_ml")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing channel or container_ml\"}");
        return;
    }
    
    uint8_t channel = doc["channel"].as<uint8_t>();
    float container_ml = doc["container_ml"].as<float>();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid channel\"}");
        return;
    }
    
    if (container_ml < CONTAINER_MIN_ML || container_ml > CONTAINER_MAX_ML) {
        char errMsg[64];
        snprintf(errMsg, sizeof(errMsg), 
                 "{\"success\":false,\"error\":\"Container must be %d-%d ml\"}", 
                 CONTAINER_MIN_ML, CONTAINER_MAX_ML);
        request->send(400, "application/json", errMsg);
        return;
    }
    
    Serial.printf("[WEB] Setting container CH%d to %.1f ml\n", channel, container_ml);
    
    bool success = channelManager.setContainerCapacity(channel, container_ml);
    
    const ContainerVolume& vol = channelManager.getContainerVolume(channel);
    
    JsonDocument resp;
    resp["success"] = success;
    resp["channel"] = channel;
    resp["container_ml"] = vol.getContainerMl();
    resp["remaining_ml"] = vol.getRemainingMl();
    resp["remaining_pct"] = vol.getRemainingPercent();
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
}

// ============================================================================
// API: REFILL - Reset remaining to container capacity
// ============================================================================

void handleApiRefill(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (!request->hasParam("channel")) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing channel\"}");
        return;
    }
    
    uint8_t channel = request->getParam("channel")->value().toInt();
    
    if (channel >= CHANNEL_COUNT) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid channel\"}");
        return;
    }
    
    Serial.printf("[WEB] Refill request CH%d\n", channel);
    
    bool success = channelManager.refillContainer(channel);
    
    const ContainerVolume& vol = channelManager.getContainerVolume(channel);
    
    JsonDocument resp;
    resp["success"] = success;
    resp["channel"] = channel;
    resp["remaining_ml"] = vol.getRemainingMl();
    resp["container_ml"] = vol.getContainerMl();
    resp["message"] = success ? "Container refilled" : "Refill failed";
    
    String response;
    serializeJson(resp, response);
    request->send(200, "application/json", response);
    
    Serial.printf("[WEB] Refill CH%d: %s (%.1f ml)\n", 
                  channel, success ? "OK" : "FAILED", vol.getRemainingMl());
}

// 333###############################################################################################################################################


void handleResetDailyLog(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (g_dailyLog) {
        auto result = g_dailyLog->reset();
        if (result == DailyLogResult::OK) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Daily log reset\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Reset failed\"}");
        }
    } else {
        request->send(503, "application/json", "{\"error\":\"Daily log not available\"}");
    }
}


// 333###############################################################################################################################################



void handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void initWebServer() {
    Serial.println(F("[WEB] Initializing web server..."));
    
    // Init dependencies
    initSessionManager();
    initAuthManager();
    
    // === PAGE ROUTES ===
    server.on("/", HTTP_GET, handleRoot);
    server.on("/login", HTTP_GET, handleLogin);
    
    // === API ROUTES ===
    server.on("/api/login", HTTP_POST, handleApiLogin);
    server.on("/api/logout", HTTP_POST, handleApiLogout);
    server.on("/api/dosing-status", HTTP_GET, handleApiDosingStatus); 
    server.on("/api/dosing-config", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL, handleApiDosingConfig);
    server.on("/api/calibrate", HTTP_POST, handleApiCalibrate);
    server.on("/api/scheduler", HTTP_POST, handleApiScheduler);
    server.on("/api/manual-dose", HTTP_POST, handleApiManualDose);
    server.on("/api/daily-reset", HTTP_POST, handleApiDailyReset);

    // === CONTAINER VOLUME API ===
    server.on("/api/container-volume", HTTP_GET, handleApiContainerVolumeGet);
    server.on("/api/container-volume", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL, handleApiContainerVolumeSet);
    server.on("/api/refill", HTTP_POST, handleApiRefill);

    // === DAILY LOG API ===
    server.on("/api/daily-logs", HTTP_GET, handleApiDailyLogs);
    server.on("/api/daily-log", HTTP_GET, handleApiDailyLogEntry);
    
    // === 404 HANDLER ===
    server.onNotFound(handleNotFound);
    
    // === START SERVER ===
    server.begin();
    serverRunning = true;
    
    Serial.println(F("[WEB] Server started on port 80"));
    Serial.printf("[WEB] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());











    server.on("/api/reset-daily-log", HTTP_POST, handleResetDailyLog);

}

bool isWebServerRunning() {
    return serverRunning;
}