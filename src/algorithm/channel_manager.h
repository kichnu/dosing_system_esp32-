/**
 * DOZOWNIK - Channel Manager
 * 
 * Zarządzanie konfiguracją kanałów dozowania.
 * Walidacja, obliczenia, zapis/odczyt z FRAM.
 */

#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"
#include "fram_controller.h"

// ============================================================================
// VALIDATION RESULT
// ============================================================================

struct ValidationError {
    bool     has_error;
    uint8_t  channel;
    char     message[64];
};

// ============================================================================
// CHANNEL MANAGER CLASS
// ============================================================================

class ChannelManager {
public:
    /**
     * Inicjalizacja - ładuje konfigurację z FRAM
     */
    bool begin();
    
    /**
     * Czy manager zainicjalizowany
     */
    bool isReady() const { return _initialized; }
    
    // --- Gettery konfiguracji ---
    
    /**
     * Pobierz aktywną konfigurację kanału (tylko do odczytu)
     */
    const ChannelConfig& getActiveConfig(uint8_t channel) const;
    
    /**
     * Pobierz pending konfigurację kanału
     */
    const ChannelConfig& getPendingConfig(uint8_t channel) const;
    
    /**
     * Pobierz stan dzienny kanału
     */
    const ChannelDailyState& getDailyState(uint8_t channel) const;
    
    /**
     * Pobierz obliczone wartości kanału
     */
    const ChannelCalculated& getCalculated(uint8_t channel) const;
    
    /**
     * Pobierz stan kanału (do GUI)
     */
    ChannelState getChannelState(uint8_t channel) const;
    
    // --- Settery konfiguracji (zapisują do pending) ---
    
    /**
     * Ustaw events bitmask (godziny 1-23)
     */
    bool setEventsBitmask(uint8_t channel, uint32_t bitmask);
    
    /**
     * Ustaw days bitmask (0=Pon, 6=Ndz)
     */
    bool setDaysBitmask(uint8_t channel, uint8_t bitmask);
    
    /**
     * Ustaw dawkę dzienną (ml)
     */
    bool setDailyDose(uint8_t channel, float dose_ml);
    
    /**
     * Ustaw dosing rate z kalibracji (ml/s)
     */
    bool setDosingRate(uint8_t channel, float rate);
    
    /**
     * Włącz/wyłącz kanał
     */
    bool setEnabled(uint8_t channel, bool enabled);
    
    // --- Walidacja ---
    
    /**
     * Waliduj konfigurację kanału
     * @return true jeśli konfiguracja poprawna
     */
    bool validateConfig(uint8_t channel, ValidationError* error = nullptr);
    
    /**
     * Waliduj wszystkie kanały
     */
    bool validateAll(ValidationError* firstError = nullptr);
    
    // --- Pending changes ---
    
    /**
     * Czy kanał ma oczekujące zmiany
     */
    bool hasPendingChanges(uint8_t channel) const;
    
    /**
     * Czy którykolwiek kanał ma pending
     */
    bool hasAnyPendingChanges() const;
    
    /**
     * Zastosuj pending -> active (wywoływane przy resecie dobowym)
     */
    bool applyPendingChanges(uint8_t channel);
    
    /**
     * Zastosuj wszystkie pending
     */
    bool applyAllPendingChanges();
    
    /**
     * Anuluj pending (przywróć active)
     */
    bool revertPendingChanges(uint8_t channel);
    
    // --- Daily State ---
    
    /**
     * Oznacz event jako wykonany
     */
    bool markEventCompleted(uint8_t channel, uint8_t hour, float dosed_ml);

    /**
     * Oznacz event jako nieudany (failed)
     */
    bool markEventFailed(uint8_t channel, uint8_t hour);
    
    /**
     * Czy event się nie powiódł
     */
    bool isEventFailed(uint8_t channel, uint8_t hour) const;
    
    /**
     * Reset stanów dziennych (o północy)
     */
    bool resetDailyStates();
    
    /**
     * Czy event został wykonany
     */
    bool isEventCompleted(uint8_t channel, uint8_t hour) const;
    
    /**
     * Ile dozowano dziś na kanale
     */
    float getTodayDosed(uint8_t channel) const;
    
    // --- Queries ---
    
    /**
     * Czy kanał jest aktywny dzisiaj (dzień tygodnia)
     */
    bool isActiveToday(uint8_t channel, uint8_t dayOfWeek) const;
    
    /**
     * Czy event powinien być wykonany teraz
     */
    bool shouldExecuteEvent(uint8_t channel, uint8_t hour, uint8_t dayOfWeek) const;
    
    /**
     * Pobierz następny zaplanowany event dla kanału
     * @return godzina (1-23) lub 255 jeśli brak
     */
    uint8_t getNextEventHour(uint8_t channel, uint8_t currentHour) const;
    
    // --- Utility ---
    
    /**
     * Przeładuj wszystko z FRAM
     */
    bool reloadFromFRAM();
    
    /**
     * Zapisz wszystko do FRAM
     */
    bool saveToFRAM();
    
    /**
     * Przelicz wartości dla kanału
     */
    void recalculate(uint8_t channel);
    
    /**
     * Przelicz wszystkie kanały
     */
    void recalculateAll();
    
    /**
     * Debug print konfiguracji kanału
     */
    void printChannelInfo(uint8_t channel) const;
    
    /**
     * Debug print wszystkich kanałów
     */
    void printAllChannels() const;

private:
    bool _initialized;
    
    // Cached data (RAM)
    ChannelConfig     _activeConfig[CHANNEL_COUNT];
    ChannelConfig     _pendingConfig[CHANNEL_COUNT];
    ChannelDailyState _dailyState[CHANNEL_COUNT];
    ChannelCalculated _calculated[CHANNEL_COUNT];
    
    // Empty config for invalid channel access
    static ChannelConfig _emptyConfig;
    static ChannelDailyState _emptyDailyState;
    static ChannelCalculated _emptyCalculated;
    
    /**
     * Zapisz pending config do FRAM i oznacz jako has_pending
     */
    bool _savePendingConfig(uint8_t channel);
    
    /**
     * Aktualizuj CRC w strukturze
     */
    void _updateConfigCRC(ChannelConfig* cfg);
    void _updateDailyStateCRC(ChannelDailyState* state);
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern ChannelManager channelManager;

#endif // CHANNEL_MANAGER_H