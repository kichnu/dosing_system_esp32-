#include "ap_html.h"

// Store HTML in PROGMEM to save RAM
const char SETUP_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Water System Setup</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 600px;
            width: 100%;
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px 20px;
            text-align: center;
        }
        .header h1 {
            font-size: 24px;
            margin-bottom: 8px;
        }
        .header p {
            font-size: 14px;
            opacity: 0.9;
        }
        .warning {
            background: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 15px;
            margin: 20px;
            border-radius: 4px;
        }
        .warning strong {
            color: #856404;
            display: block;
            margin-bottom: 5px;
        }
        .warning p {
            color: #856404;
            font-size: 13px;
            line-height: 1.5;
        }
        .content {
            padding: 30px 20px;
            text-align: center;
        }
        .status {
            background: #e8f5e9;
            border: 2px solid #4caf50;
            border-radius: 8px;
            padding: 20px;
            margin: 20px 0;
        }
        .status-icon {
            font-size: 48px;
            margin-bottom: 10px;
        }
        .status h2 {
            color: #2e7d32;
            font-size: 20px;
            margin-bottom: 10px;
        }
        .status p {
            color: #558b2f;
            font-size: 14px;
            line-height: 1.6;
        }
        .info-box {
            background: #f5f5f5;
            border-radius: 8px;
            padding: 20px;
            margin: 20px 0;
            text-align: left;
        }
        .info-box h3 {
            color: #333;
            font-size: 16px;
            margin-bottom: 15px;
        }
        .info-item {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid #ddd;
        }
        .info-item:last-child {
            border-bottom: none;
        }
        .info-label {
            color: #666;
            font-size: 14px;
        }
        .info-value {
            color: #333;
            font-weight: 600;
            font-size: 14px;
        }
        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 15px 30px;
            font-size: 16px;
            border-radius: 8px;
            cursor: pointer;
            margin-top: 20px;
            transition: transform 0.2s;
        }
        .btn:hover {
            transform: translateY(-2px);
        }
        .footer {
            padding: 20px;
            text-align: center;
            color: #999;
            font-size: 12px;
            border-top: 1px solid #eee;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌊 ESP32 Water System</h1>
            <p>Provisioning Mode</p>
        </div>
        
        <div class="warning">
            <strong>⚠️ Security Notice</strong>
            <p>This connection is not encrypted (HTTP). Only configure this device in a trusted environment. Do not use sensitive passwords that you use elsewhere.</p>
        </div>
        
        <div class="content">
            <div class="status">
                <div class="status-icon">✓</div>
                <h2>Access Point Active</h2>
                <p>You are successfully connected to the ESP32 provisioning network.</p>
            </div>
            
            <div class="info-box">
                <h3>Connection Information</h3>
                <div class="info-item">
                    <span class="info-label">Network:</span>
                    <span class="info-value">ESP32-WATER-SETUP</span>
                </div>
                <div class="info-item">
                    <span class="info-label">IP Address:</span>
                    <span class="info-value">192.168.4.1</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Status:</span>
                    <span class="info-value">Ready for Configuration</span>
                </div>
            </div>
            
            <p style="color: #666; font-size: 14px; margin-top: 20px;">
                Configuration form will be available here soon.<br>
                (ETAP 2: WiFi scan + form will be implemented next)
            </p>
            
            <button class="btn" onclick="location.reload()">Refresh Page</button>
        </div>
        
        <div class="footer">
            ESP32-C3 Water System v1.0 | Provisioning Mode
        </div>
    </div>
</body>
</html>
)rawliteral";

const char SUCCESS_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Saved</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            max-width: 500px;
            width: 100%;
            padding: 40px 20px;
            text-align: center;
        }
        .success-icon {
            font-size: 80px;
            margin-bottom: 20px;
        }
        h1 {
            color: #2e7d32;
            font-size: 28px;
            margin-bottom: 15px;
        }
        p {
            color: #666;
            font-size: 16px;
            line-height: 1.6;
            margin-bottom: 20px;
        }
        .instructions {
            background: #f5f5f5;
            border-radius: 8px;
            padding: 20px;
            margin: 20px 0;
            text-align: left;
        }
        .instructions ol {
            margin-left: 20px;
        }
        .instructions li {
            color: #333;
            font-size: 14px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon">✓</div>
        <h1>Configuration Saved!</h1>
        <p>Your device has been configured successfully.</p>
        
        <div class="instructions">
            <strong>Next Steps:</strong>
            <ol>
                <li>Disconnect power from the device</li>
                <li>Wait 5 seconds</li>
                <li>Reconnect power</li>
                <li>Device will boot in production mode</li>
            </ol>
        </div>
        
        <p style="font-size: 14px; color: #999;">
            This page will remain active until restart.
        </p>
    </div>
</body>
</html>
)rawliteral";

const char* getSetupPageHTML() {
    return SETUP_PAGE_HTML;
}

const char* getSuccessPageHTML() {
    return SUCCESS_PAGE_HTML;
}