

        /**
 * DOZOWNIK - Relay Controller
 * 
 * Bezpieczne sterowanie przekaźnikami pomp z walidacją GPIO.
 * Gwarantuje że tylko jedna pompa pracuje w danym momencie (mutex).
 * 
 * Walidacja GPIO (3-fazowa):
 * - PRE-CHECK:  Sprawdź LOW przed włączeniem (wykrycie urwanego przewodu)
 * - RUN-CHECK:  Sprawdź HIGH po włączeniu (potwierdzenie działania)
 * - POST-CHECK: Sprawdź LOW po wyłączeniu (wykrycie zablokowanego przekaźnika)
 */

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"

// ============================================================================
// VALIDATION STATE MACHINE
// ============================================================================

/**
 * Stan maszyny walidacji GPIO
 */
enum class GpioValidationState : uint8_t {
    IDLE = 0,
    
    // PRE-CHECK (przed włączeniem przekaźnika)
    PRE_CHECK_START,            // Rozpoczęcie pre-check
    PRE_CHECK_DEBOUNCE,         // Debounce odczytu
    PRE_CHECK_VERIFY,           // Weryfikacja stanu LOW
    
    // Przekaźnik włączony, RUN-CHECK
    RELAY_ON_DELAY,             // Czekanie GPIO_CHECK_DELAY_MS
    RUN_CHECK_DEBOUNCE,         // Debounce odczytu
    RUN_CHECK_VERIFY,           // Weryfikacja stanu HIGH
    
    // Praca pompy
    RUNNING,                    // Pompa pracuje normalnie
    
    // POST-CHECK (po wyłączeniu przekaźnika)
    POST_CHECK_DELAY,           // Czekanie GPIO_POST_CHECK_DELAY_MS
    POST_CHECK_DEBOUNCE,        // Debounce odczytu
    POST_CHECK_VERIFY,          // Weryfikacja powrotu do LOW
    
    // Stany końcowe
    VALIDATION_OK,              // Cały cykl OK
    VALIDATION_FAILED_PRE,      // Błąd PRE-CHECK
    VALIDATION_FAILED_RUN,      // Błąd RUN-CHECK
    VALIDATION_FAILED_POST      // Błąd POST-CHECK (krytyczny!)
};

// ============================================================================
// RELAY STATE
// ============================================================================

/**
 * Stan pojedynczego przekaźnika
 */
struct RelayState {
    bool     is_on;             // Czy przekaźnik włączony
    uint32_t on_since_ms;       // millis() włączenia
    uint32_t total_on_time_ms;  // Suma czasu pracy (od boot)
    uint32_t activation_count;  // Licznik aktywacji
};

/**
 * Wynik operacji na relay
 */
enum class RelayResult : uint8_t {
    OK = 0,
    ERROR_INVALID_CHANNEL,
    ERROR_MUTEX_LOCKED,
    ERROR_SYSTEM_HALTED,
    ERROR_ALREADY_ON,
    ERROR_ALREADY_OFF,
    ERROR_TIMEOUT,
    ERROR_GPIO_PRE_CHECK,       // NOWE
    ERROR_GPIO_RUN_CHECK,       // NOWE
    ERROR_GPIO_POST_CHECK       // NOWE
};

/**
 * Wynik walidacji GPIO (dla callbacka)
 */
enum class GpioValidationResult : uint8_t {
    PENDING = 0,        // W trakcie walidacji
    OK,                 // Walidacja OK
    FAILED_PRE,         // Błąd pre-check
    FAILED_RUN,         // Błąd run-check
    FAILED_POST         // Błąd post-check
};

// ============================================================================
// RELAY CONTROLLER CLASS
// ============================================================================

class RelayController {
public:
    /**
     * Inicjalizacja kontrolera - konfiguruje GPIO
     */
    void begin();
    
    /**
     * Aktualizacja stanu - sprawdza timeout i walidację, wywołuj w loop()
     */
    void update();
    
    // --- Sterowanie pojedynczym kanałem ---
    
    /**
     * Włącz przekaźnik kanału (z walidacją GPIO jeśli włączona)
     * @param channel Numer kanału (0-5)
     * @param max_duration_ms Maksymalny czas pracy
     * @param validate Czy wykonać walidację GPIO (domyślnie z ustawień)
     * @return Wynik operacji
     */
    RelayResult turnOn(uint8_t channel, uint32_t max_duration_ms = 0, bool validate = GPIO_VALIDATION_DEFAULT);
    
    /**
     * Wyłącz przekaźnik kanału (z walidacją POST jeśli włączona)
     * @param channel Numer kanału (0-5)
     * @return Wynik operacji
     */
    RelayResult turnOff(uint8_t channel);
    
    /**
     * Wyłącz przekaźnik i zwróć czas pracy
     * @param channel Numer kanału
     * @param actual_duration_ms [out] Rzeczywisty czas pracy
     * @return Wynik operacji
     */
    RelayResult turnOffWithDuration(uint8_t channel, uint32_t* actual_duration_ms);
    
    // --- Sterowanie globalne ---
    
    /**
     * Wyłącz wszystkie przekaźniki (emergency stop)
     */
    void allOff();
    
    /**
     * Wyłącz wszystkie i zablokuj system (critical error)
     */
    void emergencyStop();
    
    /**
     * Natychmiastowe wyłączenie bez walidacji POST (dla błędów)
     */
    void forceOffImmediate(uint8_t channel);
    
    // --- Status ---
    
    bool isAnyOn() const;
    uint8_t getActiveChannel() const;
    bool isChannelOn(uint8_t channel) const;
    uint32_t getActiveRuntime() const;
    uint32_t getRemainingTime() const;
    const RelayState& getChannelState(uint8_t channel) const;
    uint32_t getTotalRuntime() const;
    
    // --- Walidacja GPIO ---
    
    /**
     * Pobierz aktualny stan walidacji
     */
    GpioValidationState getValidationState() const { return _validationState; }
    
    /**
     * Pobierz wynik walidacji (dla odpytywania)
     */
    GpioValidationResult getValidationResult() const;
    
    /**
     * Czy walidacja jest w toku
     */
    bool isValidating() const;
    
    /**
     * Czy pompa pracuje (po przejściu walidacji RUN)
     */
    bool isPumpRunning() const { return _validationState == GpioValidationState::RUNNING; }
    
    /**
     * Pobierz ostatni odczytany stan GPIO
     */
    int getLastGpioReading() const { return _lastGpioReading; }
    
    // --- Debug ---
    
    void printStatus() const;
    static const char* resultToString(RelayResult result);
    static const char* validationStateToString(GpioValidationState state);

private:
    RelayState _channels[CHANNEL_COUNT];
    
    uint8_t  _activeChannel;        // Aktywny kanał (255 = żaden)
    uint32_t _activeMaxDuration;    // Max czas dla aktywnego kanału
    bool     _initialized;
    
    // --- Walidacja GPIO ---
    GpioValidationState _validationState;
    bool     _validationEnabled;    // Czy walidacja włączona dla tego cyklu
    uint32_t _stateStartTime;       // millis() wejścia w aktualny stan
    int      _lastGpioReading;      // Ostatni odczyt GPIO (dla debug)
    uint32_t _pumpStartTime;        // millis() rozpoczęcia właściwej pracy pompy
    
    // Metody prywatne
    void _setRelay(uint8_t channel, bool state);
    void _checkTimeout();
    
    // Walidacja GPIO
    void _updateValidation();
    void _startPreCheck();
    void _handlePreCheckDebounce();
    void _handlePreCheckVerify();
    void _handleRelayOnDelay();
    void _handleRunCheckDebounce();
    void _handleRunCheckVerify();
    void _handleRunning();
    void _handlePostCheckDelay();
    void _handlePostCheckDebounce();
    void _handlePostCheckVerify();
    void _validationSuccess();
    void _validationFailed(GpioValidationState failState, CriticalErrorType errorType, ValidationPhase phase);
    
    int _readGpioWithDebounce();
    void _transitionTo(GpioValidationState newState);
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern RelayController relayController;

#endif // RELAY_CONTROLLER_H