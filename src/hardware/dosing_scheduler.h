/**
 * DOZOWNIK - Dosing Scheduler
 * 
 * Główna logika harmonogramu dozowania.
 * Sprawdza czas, uruchamia pompy, obsługuje daily reset.
 */

#ifndef DOSING_SCHEDULER_H
#define DOSING_SCHEDULER_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"
#include "channel_manager.h"
#include "relay_controller.h"
#include "rtc_controller.h"
#include "fram_controller.h"


// ============================================================================
// SCHEDULER STATE
// ============================================================================

enum class SchedulerState : uint8_t {
    IDLE,               // Oczekiwanie
    CHECKING,           // Sprawdzanie harmonogramu
    DOSING,             // Dozowanie w trakcie
    VALIDATING,         // Walidacja GPIO w trakcie  <-- DODAJ
    WAITING_PUMP,       // Czekanie na zakończenie pompy
    DAILY_RESET,        // Reset dobowy w trakcie
    ERROR,              // Błąd
    SCHED_DISABLED            // Wyłączony
};

// ============================================================================
// DOSING EVENT (aktualnie wykonywany)
// ============================================================================

struct DosingEvent {
    uint8_t  channel;
    uint8_t  hour;
    float    target_ml;
    uint32_t target_duration_ms;
    uint32_t start_time_ms;
    bool     completed;
    bool     failed;
    bool     gpio_validated;
    bool     validation_started;
};

// ============================================================================
// DOSING SCHEDULER CLASS
// ============================================================================

class DosingScheduler {
public:
    /**
     * Inicjalizacja schedulera
     */
    bool begin();
    
    /**
     * Główna pętla - wywołuj w loop()
     */
    void update();
    
    /**
     * Czy scheduler aktywny
     */
    bool isEnabled() const { return _enabled; }
    
    /**
     * Włącz/wyłącz scheduler
     */
    void setEnabled(bool enabled);

    /**
     * Zaktualizuj _lastDay po zewnętrznej zmianie czasu (NTP sync)
     * Wywołaj po każdym NTP sync żeby uniknąć fałszywych daily resetów
     */
    void syncTimeState();
    
    /**
     * Pobierz aktualny stan
     */
    SchedulerState getState() const { return _state; }
    
    /**
     * Pobierz aktualny event (jeśli dozowanie w trakcie)
     */
    const DosingEvent& getCurrentEvent() const { return _currentEvent; }
    
    // --- Manual control ---
    
    /**
     * Wymuś wykonanie dozowania na kanale
     * @return true jeśli uruchomiono
     */
    bool triggerManualDose(uint8_t channel);
    
    /**
     * Zatrzymaj bieżące dozowanie
     */
    void stopCurrentDose();
    
    /**
     * Wymuś daily reset
     */
    bool forceDailyReset();
    
    // --- Queries ---
    
    /**
     * Ile sekund do następnego eventu
     */
    uint32_t getSecondsToNextEvent() const;
    
    /**
     * Pobierz ostatni czas sprawdzenia
     */
    uint32_t getLastCheckTime() const { return _lastCheckTime; }
    
    /**
     * Pobierz liczbę wykonanych eventów dziś
     */
    uint16_t getTodayEventCount() const { return _todayEventCount; }
    
    // --- Debug ---
    
    void printStatus() const;
    
    static const char* stateToString(SchedulerState state);

private:
    bool _initialized;
    bool _enabled;
    SchedulerState _state;
    
    DosingEvent _currentEvent;
    
    uint32_t _lastCheckTime;
    uint32_t _lastUpdateTime;
    uint8_t  _lastHour;
    uint8_t  _lastDay;
    uint16_t _todayEventCount;
    
    /**
     * Sprawdź czy trzeba wykonać daily reset
     */
    bool _checkDailyReset();
    
    /**
     * Wykonaj daily reset
     */
    bool _performDailyReset();
    
    /**
     * Sprawdź harmonogram i uruchom eventy
     */
    void _checkSchedule();
    
    /**
     * Znajdź następny event do wykonania
     * @return channel (0-5) lub 255 jeśli brak
     */
    uint8_t _findNextEvent(uint8_t hour, uint8_t dayOfWeek);
    
    /**
     * Uruchom dozowanie
     */
    bool _startDosing(uint8_t channel, uint8_t hour);
    
    /**
     * Sprawdź status bieżącego dozowania
     */
    void _checkDosingProgress();
    
    /**
     * Zakończ dozowanie
     */
    void _completeDosing(bool success);
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

extern DosingScheduler dosingScheduler;

#endif // DOSING_SCHEDULER_H