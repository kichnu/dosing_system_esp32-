/**
 * DOZOWNIK - Safety Manager
 * 
 * Zarządzanie bezpieczeństwem systemu:
 * - Master relay (GPIO38) - główne odcięcie zasilania pomp
 * - Buzzer (GPIO39) - sygnalizacja błędu
 * - Reset button (GPIO40) - reset błędu krytycznego
 * - Persystencja błędów w FRAM
 */

#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"

// ============================================================================
// SAFETY MANAGER CLASS
// ============================================================================

class SafetyManager {
public:
    /**
     * Inicjalizacja - wywoływana bardzo wcześnie w setup()
     * UWAGA: GPIO38 startuje jako LOW (bezpieczny stan)
     */
    void begin();
    
    /**
     * Sprawdź FRAM i włącz master relay jeśli brak błędu
     * Wywoływane PO inicjalizacji FRAM
     * @return true jeśli system OK, false jeśli błąd aktywny
     */
    bool enableIfSafe();
    
    /**
     * Główna pętla - obsługa buzzera i przycisku reset
     * Wywoływane w loop()
     */
    void update();
    
    // --- Triggery błędów ---
    
    /**
     * Zgłoś błąd krytyczny - NATYCHMIAST wyłącza master relay
     */
    void triggerCriticalError(CriticalErrorType type, 
                               uint8_t channel,
                               ValidationPhase phase = PHASE_NONE,
                               uint32_t errorData = 0);
    
    /**
     * Reset błędu krytycznego (wywoływane przez przycisk)
     */
    bool resetCriticalError();
    
    // --- Gettery ---
    
    bool isCriticalErrorActive() const { return _errorActive; }
    bool isMasterRelayEnabled() const { return _masterRelayEnabled; }
    CriticalErrorType getErrorType() const { return _currentError.error_type; }
    uint8_t getErrorChannel() const { return _currentError.channel; }
    const CriticalErrorState& getErrorState() const { return _currentError; }
    
    // --- Status ---
    
    void printStatus() const;
    
private:
    // Stan
    bool _initialized = false;
    bool _errorActive = false;
    bool _masterRelayEnabled = false;
    CriticalErrorState _currentError;
    
    // Buzzer timing
    unsigned long _buzzerLastToggle = 0;
    bool _buzzerState = false;
    
    // Button handling
    unsigned long _buttonPressStart = 0;
    bool _buttonWasPressed = false;
    bool _resetInProgress = false;
    
    // Metody prywatne
    void _setMasterRelay(bool enabled);
    void _setBuzzer(bool on);
    void _updateBuzzerPattern();
    void _handleResetButton();
    void _saveErrorToFRAM();
    void _loadErrorFromFRAM();
    void _clearErrorInFRAM();
    void _takeGpioSnapshot();
    void _confirmResetBeep();
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern SafetyManager safetyManager;

#endif // SAFETY_MANAGER_H