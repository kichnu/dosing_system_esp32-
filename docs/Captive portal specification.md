📋 Captive Portal - Specyfikacja implementacyjna
🏗️ Architektura systemu
Boot Flow:
├─ verifyCredentialsInFRAM()
│  ├─ Valid + WiFi Connect Success → Production Mode
│  ├─ Valid + WiFi Fail (3x) → AP Mode (auto-recovery)
│  └─ Invalid → AP Mode (first setup)
│
├─ Production Mode
│  ├─ main loop (water system)
│  ├─ Web Dashboard (port 80)
│  └─ Manual AP trigger: POST /api/enter-ap-mode
│
└─ AP Mode (blocking)
   ├─ WiFi.softAP("ESP32-WATER-SETUP", "setup12345")
   ├─ DNSServer (captive portal detection)
   ├─ AP Web Server (port 80)
   └─ loop() until configured

📂 Struktura plików
Nowe moduły
src/ap_mode/
├── ap_portal.cpp/h          # AP mode main controller
├── ap_server.cpp/h          # Web server dla AP
├── ap_handlers.cpp/h        # Request handlers
├── ap_html.cpp/h            # HTML pages (mobile-first)
└── wifi_scanner.cpp/h       # WiFi network scanning

src/utils/
└── credentials_validator.cpp/h  # Walidacja input

Modyfikowane:
├── src/main.cpp             # Boot decision logic
├── src/hardware/fram_controller.cpp  # WiFi fail counter
├── src/config/config.cpp    # AP trigger from web
└── platformio.ini           # Usunięcie dual-mode

## ⚠️ Critical Implementation Notes

| Kwestia | Rozwiązanie |
|---------|-------------|
| **HTTPS niemożliwe** | Accept risk, display warning in UI |
| **iOS captive portal** | Implement ALL detection endpoints (apple.com, hotspot-detect.html, etc) |
| **DNS timeout** | dnsServer.setErrorReplyCode(DNSReplyCode::NoError) |
| **Form validation** | Client-side + server-side (both!) |
| **WiFi test timeout** | Max 15s, return to AP mode immediately |
| **Credentials encryption** | Use existing AES-256 from fram_encryption.h |
| **Button debouncing** | 50ms debounce + 5s long-press threshold |
| **RAM monitoring** | Log ESP.getFreeHeap() in AP mode loop |
| **Restart safety** | Always delay(5000) before ESP.restart() to allow HTTP response |
| **Error recovery** | If AP mode crashes → hardware watchdog reboot → AP mode again |

---

## 🎯 Testing Checklist
```
[ ] Boot bez credentials → AP Mode auto-start
[ ] Boot z credentials → Production Mode
[ ] 3x WiFi fail → AP Mode fallback
[ ] Button 5s → AP Mode entry
[ ] Web dashboard → AP Mode trigger
[ ] iOS captive portal auto-open
[ ] Android captive portal notification
[ ] WiFi scan returns networks
[ ] WiFi test przed save
[ ] Save credentials → FRAM write success
[ ] Restart → Production Mode connect
[ ] Invalid credentials → proper error messages
[ ] Form validation (device name pattern)
[ ] LED patterns czytelne
[ ] Serial logs informative
