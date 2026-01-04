/**
 * @file daily_log.h
 * @brief API dla Daily Log Ring Buffer
 * 
 * Moduł zarządza ring bufferem dobowych logów w FRAM.
 * 
 * Przepływ danych:
 * 1. Po każdym dozowaniu: dailyLogRecordDosing() aktualizuje bieżący wpis
 * 2. Przy błędzie: dailyLogRecordCriticalError() zapisuje błąd
 * 3. O północy: dailyLogFinalizeDay() zamyka dzień, tworzy nowy wpis
 * 4. GUI: dailyLogGetEntry() pobiera sfinalizowane wpisy do wyświetlenia
 * 
 * Bezpieczeństwo:
 * - Podwójny nagłówek (A/B) dla atomowości
 * - CRC32 na każdym wpisie i nagłówku
 * - Wpisy "w trakcie" (niesfinalizowane) nie są widoczne w GUI
 * 
 * Zależności:
 * - framController (globalna instancja FramController)
 * - rtcController (globalna instancja RtcController)
 */

#ifndef DAILY_LOG_H
#define DAILY_LOG_H

#include <cstdint>
#include <functional>
#include "daily_log_types.h"

/**
 * @brief Rezultat operacji na Daily Log
 */
enum class DailyLogResult : uint8_t {
    OK = 0,
    ERROR_NOT_INITIALIZED,
    ERROR_FRAM_READ,
    ERROR_FRAM_WRITE,
    ERROR_CRC_MISMATCH,
    ERROR_ENTRY_NOT_FOUND,
    ERROR_BUFFER_FULL,
    ERROR_INVALID_PARAM,
    ERROR_HEADER_CORRUPT
};

/**
 * @brief Statystyki ring buffera
 */
struct DailyLogStats {
    uint8_t count;              // Liczba wpisów w buforze
    uint8_t capacity;           // Maksymalna pojemność
    uint32_t total_written;     // Łączna liczba zapisanych wpisów (od początku)
    uint32_t first_day;         // Najstarszy dzień (UTC day)
    uint32_t last_day;          // Najnowszy dzień (UTC day)
    bool has_current_day;       // Czy jest wpis dla bieżącego dnia
};

/**
 * @brief Klasa zarządzająca Daily Log Ring Buffer
 * 
 * Używa globalnych instancji:
 * - framController (FramController)
 * - rtcController (RtcController)
 */
class DailyLogManager {
public:
    /**
     * @brief Konstruktor domyślny
     */
    DailyLogManager() = default;
    
    // === INICJALIZACJA ===
    
    /**
     * @brief Inicjalizuje ring buffer
     * 
     * Wczytuje nagłówki A/B, wybiera aktualny, waliduje.
     * Jeśli oba nagłówki uszkodzone - tworzy pusty buffer.
     * 
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult init();
    
    /**
     * @brief Resetuje ring buffer (usuwa wszystkie wpisy)
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult reset();
    
    // === ZAPIS ZDARZEŃ (wywoływane przez system) ===
    
    /**
     * @brief Rejestruje wykonane dozowanie
     * 
     * Aktualizuje wpis bieżącego dnia. Jeśli wpis nie istnieje - tworzy.
     * 
     * @param channel Numer kanału (0-5)
     * @param dose_ml Wydozowana ilość [ml]
     * @param success Czy dozowanie się powiodło
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult recordDosing(uint8_t channel, float dose_ml, bool success);
    
    /**
     * @brief Rejestruje błąd krytyczny
     * 
     * @param error_type Typ błędu (z CriticalErrorType)
     * @param channel Numer kanału (255 = błąd globalny)
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult recordCriticalError(uint8_t error_type, uint8_t channel);
    
    /**
     * @brief Rejestruje restart systemu (power cycle)
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult recordPowerCycle();
    
    /**
     * @brief Rejestruje rozłączenie WiFi
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult recordWifiDisconnect();
    
    /**
     * @brief Rejestruje synchronizację NTP
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult recordNtpSync();
    
    /**
     * @brief Aktualizuje statystyki systemowe (wywoływać co minutę)
     * 
     * @param uptime_seconds Aktualny uptime
     * @param free_heap_kb Wolna pamięć heap [KB]
     * @param temp_celsius Temperatura ESP [°C]
     */
    void updateSystemStats(uint32_t uptime_seconds, uint8_t free_heap_kb, uint8_t temp_celsius);
    
    // === FINALIZACJA DNIA ===
    
    /**
     * @brief Finalizuje bieżący dzień (wywoływać o północy UTC)
     * 
     * Ustawia flagę COMPLETE, oblicza końcowe statusy kanałów.
     * Po finalizacji wpis jest tylko do odczytu.
     * 
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult finalizeDay();
    
    /**
     * @brief Inicjalizuje nowy dzień
     * 
     * Tworzy nowy wpis dla bieżącego dnia z planem dozowań.
     * Wywoływać po finalizeDay() lub przy starcie systemu.
     * 
     * @param current_timestamp Aktualny Unix timestamp
     * @return DailyLogResult::OK lub kod błędu
     */
    DailyLogResult initializeNewDay(uint32_t current_timestamp);
    
    // === ODCZYT (dla GUI) ===
    
    /**
     * @brief Pobiera wpis po indeksie (0 = najnowszy sfinalizowany)
     * 
     * @param index Indeks od najnowszego (0, 1, 2, ...)
     * @param entry [out] Struktura do wypełnienia
     * @return DailyLogResult::OK lub ERROR_ENTRY_NOT_FOUND
     */
    DailyLogResult getEntry(uint8_t index, DayLogEntry& entry);
    
    /**
     * @brief Pobiera wpis po dacie (UTC day)
     * 
     * @param utc_day Dzień (Unix timestamp / 86400)
     * @param entry [out] Struktura do wypełnienia
     * @return DailyLogResult::OK lub ERROR_ENTRY_NOT_FOUND
     */
    DailyLogResult getEntryByDay(uint32_t utc_day, DayLogEntry& entry);
    
    /**
     * @brief Pobiera bieżący (niesfinalizowany) wpis - tylko do podglądu
     * 
     * @param entry [out] Struktura do wypełnienia
     * @return DailyLogResult::OK lub ERROR_ENTRY_NOT_FOUND
     */
    DailyLogResult getCurrentEntry(DayLogEntry& entry);
    
    /**
     * @brief Pobiera statystyki ring buffera
     */
    DailyLogStats getStats() const;
    
    /**
     * @brief Zwraca liczbę sfinalizowanych wpisów (do przeglądania)
     */
    uint8_t getFinalizedCount() const;
    
    /**
     * @brief Iteruje po sfinalizowanych wpisach (od najnowszego)
     * 
     * @param callback Funkcja wywoływana dla każdego wpisu
     *                 Zwraca false aby przerwać iterację
     * @param max_entries Maksymalna liczba wpisów (0 = wszystkie)
     */
    void iterateEntries(std::function<bool(const DayLogEntry&, uint8_t index)> callback, 
                        uint8_t max_entries = 0);
    
    // === DIAGNOSTYKA ===
    
    /**
     * @brief Sprawdza integralność całego ring buffera
     * @return Liczba uszkodzonych wpisów
     */
    uint8_t validateBuffer();
    
    /**
     * @brief Zwraca tekstowy opis ostatniego błędu
     */
    const char* getLastErrorString() const;
    
    /**
     * @brief Czy moduł jest zainicjalizowany
     */
    bool isInitialized() const { return initialized_; }

private:
    bool initialized_ = false;
    DailyLogRingHeader header_;
    DayLogEntry current_entry_;   // Bufor dla bieżącego dnia (w RAM)
    bool current_entry_dirty_ = false;
    DailyLogResult last_error_ = DailyLogResult::OK;
    
    // === Metody wewnętrzne ===
    
    // Nagłówek
    DailyLogResult loadHeader();
    DailyLogResult saveHeader();
    bool validateHeaderCRC(const DailyLogRingHeader& h);
    void calculateHeaderCRC(DailyLogRingHeader& h);
    
    // Wpisy
    DailyLogResult loadEntry(uint8_t ring_index, DayLogEntry& entry);
    DailyLogResult saveEntry(uint8_t ring_index, DayLogEntry& entry);
    bool validateEntryCRC(const DayLogEntry& e);
    void calculateEntryCRC(DayLogEntry& e);
    
    // Ring buffer
    uint8_t nextIndex(uint8_t current) const;
    uint8_t prevIndex(uint8_t current) const;
    uint8_t indexToRingIndex(uint8_t logical_index) const;
    
    // Pomocnicze
    DailyLogResult ensureCurrentEntry(uint32_t utc_day);
    DailyLogResult commitCurrentEntry();
    void initEmptyEntry(DayLogEntry& entry, uint32_t utc_day, uint8_t day_of_week);
    
    // CRC
    uint32_t calculateCRC32(const uint8_t* data, size_t length);
};

// ============================================================================
// Globalna instancja (extern - definicja w .cpp)
// ============================================================================

extern DailyLogManager* g_dailyLog;

/**
 * @brief Inicjalizuje globalną instancję DailyLogManager
 * @return true jeśli sukces
 */
bool dailyLogInit();

#endif // DAILY_LOG_H