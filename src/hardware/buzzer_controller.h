#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

#include <Arduino.h>

// Tryby ciągłego wzorca (sterowane przez FreeRTOS task)
enum class BuzzerMode : uint8_t {
    OFF     = 0,
    WARNING = 1,  // 1 krótki pik co 12s (np. info systemowe)
    ALARM   = 2,  // 2 piki co 12s (błąd krytyczny)
};

// Inicjalizacja: GPIO + xTaskCreate. Wywołaj raz w setup(), przed safetyManager.begin().
void initBuzzer();

// Ustaw tryb ciągłego wzorca (bezpieczne z dowolnego miejsca kodu)
void setBuzzerMode(BuzzerMode mode);

// Sygnały jednorazowe (blokujące delay) — bezpieczne w setup() i w runtime
// W runtime tymczasowo wstrzymują task buzzera na czas odtwarzania.
void buzzerPowerOn();   // 1x 300ms — potwierdzenie startu
void buzzerOK();        // 2x 100ms — inicjalizacja OK / reset błędu
void buzzerCritical();  // 3x 50ms  — błąd krytyczny podczas bootu

#endif // BUZZER_CONTROLLER_H
