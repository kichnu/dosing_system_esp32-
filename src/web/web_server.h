/**
 * DOZOWNIK - Web Server
 * 
 * Serwer HTTP z API dla GUI dozownika.
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/**
 * Inicjalizacja serwera (wywołaj po WiFi.begin)
 */
void initWebServer();

/**
 * Czy serwer działa
 */
bool isWebServerRunning();

#endif // WEB_SERVER_H