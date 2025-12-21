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
        const ChannelConfig& cfg = channelManager.getActiveConfig(i);
        const ChannelConfig& pending = channelManager.getPendingConfig(i);
        const ChannelDailyState& daily = channelManager.getDailyState(i);
        const ChannelCalculated& calc = channelManager.getCalculated(i);
        
        JsonObject ch = channels.add<JsonObject>();
        
        ch["id"] = i;
        ch["events"] = cfg.events_bitmask;
        ch["days"] = cfg.days_bitmask;
        ch["dailyDose"] = cfg.daily_dose_ml;
        ch["dosingRate"] = cfg.dosing_rate;
        ch["enabled"] = cfg.enabled ? true : false;
        
        ch["eventsCompleted"] = daily.events_completed;
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
    }
    
    // Serialize and send
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

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
    
    // === 404 HANDLER ===
    server.onNotFound(handleNotFound);
    
    // === START SERVER ===
    server.begin();
    serverRunning = true;
    
    Serial.println(F("[WEB] Server started on port 80"));
    Serial.printf("[WEB] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
}

bool isWebServerRunning() {
    return serverRunning;
}