#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

void initAuthManager();
bool isIPAllowed(IPAddress ip);
bool verifyPassword(const String& password);
String hashPassword(const String& password);

// Trusted IP check for VPS proxy
bool isTrustedProxyIP(IPAddress ip);

#endif