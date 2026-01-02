// /**
//  * DOZOWNIK - GPIO Validator
//  * 
//  * Walidacja pracy pomp przez odczyt GPIO feedback.
//  * OPCJONALNE - można wyłączyć przez GPIO_VALIDATION_ENABLED = false
//  * 
//  * Logika:
//  * - Po włączeniu pompy, czekamy GPIO_CHECK_DELAY_MS (2s)
//  * - Odczytujemy stan GPIO z debounce GPIO_DEBOUNCE_MS (1s)
//  * - Jeśli stan != GPIO_EXPECTED_STATE -> BŁĄD KRYTYCZNY
//  * - Jedna próba, brak retry!
//  */

// #ifndef GPIO_VALIDATOR_H
// #define GPIO_VALIDATOR_H

// #include <Arduino.h>
// #include "config.h"
// #include "dosing_types.h"

// // ============================================================================
// // VALIDATION RESULT
// // ============================================================================

// enum class ValidationResult : uint8_t {
//     OK = 0,                     // Walidacja przeszła
//     PENDING,                    // Walidacja w toku
//     SKIPPED,                    // Walidacja wyłączona
//     FAILED_NO_SIGNAL,           // Brak sygnału z GPIO
//     FAILED_WRONG_STATE,         // Zły stan GPIO
//     FAILED_TIMEOUT,             // Timeout walidacji
//     ERROR_NO_PUMP_ACTIVE        // Żadna pompa nie pracuje
// };

// // ============================================================================
// // GPIO VALIDATOR CLASS
// // ============================================================================

// class GpioValidator {
// public:
//     /**
//      * Inicjalizacja - konfiguruje GPIO jako INPUT
//      */
//     void begin();
    
//     /**
//      * Rozpocznij walidację dla kanału
//      * Wywołaj po włączeniu pompy
//      * @param channel Numer kanału (0-5)
//      */
//     void startValidation(uint8_t channel);
    
//     /**
//      * Aktualizacja stanu walidacji
//      * Wywołuj w loop() gdy walidacja aktywna
//      * @return Aktualny wynik walidacji
//      */
//     ValidationResult update();
    
//     /**
//      * Anuluj bieżącą walidację
//      */
//     void cancel();
    
//     /**
//      * Czy walidacja jest w toku
//      */
//     bool isValidating() const;
    
//     /**
//      * Pobierz wynik ostatniej walidacji
//      */
//     ValidationResult getLastResult() const;
    
//     /**
//      * Pobierz kanał walidacji
//      */
//     uint8_t getValidatingChannel() const;
    
//     // --- Bezpośredni odczyt GPIO ---
    
//     /**
//      * Odczytaj surowy stan GPIO dla kanału
//      */
//     bool readGpioRaw(uint8_t channel) const;
    
//     /**
//      * Odczytaj stan GPIO z debounce
//      * @param channel Numer kanału
//      * @param debounce_ms Czas debounce (default 100ms)
//      */
//     bool readGpioDebounced(uint8_t channel, uint32_t debounce_ms = 100) const;
    
//     /**
//      * Odczytaj wszystkie GPIO (debug)
//      */
//     void printAllGpio() const;
    
//     // --- Statystyki ---
    
//     /**
//      * Pobierz liczbę udanych walidacji
//      */
//     uint32_t getSuccessCount() const { return _successCount; }
    
//     /**
//      * Pobierz liczbę nieudanych walidacji
//      */
//     uint32_t getFailCount() const { return _failCount; }
    
//     /**
//      * Resetuj statystyki
//      */
//     void resetStats();
    
//     // --- Konwersje ---
    
//     static const char* resultToString(ValidationResult result);

// private:
//     enum class State : uint8_t {
//         IDLE,
//         DELAY_WAIT,     // Czekamy GPIO_CHECK_DELAY_MS
//         DEBOUNCE,       // Odczyt z debounce
//         COMPLETE
//     };
    
//     State    _state;
//     uint8_t  _channel;
//     uint32_t _startTime;
//     uint32_t _debounceStart;
//     bool     _debounceFirstRead;
//     ValidationResult _lastResult;
    
//     uint32_t _successCount;
//     uint32_t _failCount;
//     bool     _initialized;
// };

// // ============================================================================
// // GLOBAL INSTANCE
// // ============================================================================

// extern GpioValidator gpioValidator;

// #endif // GPIO_VALIDATOR_H