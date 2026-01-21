/**
 * DOZOWNIK - Data Types & Structures
 * 
 * Definicje struktur danych używanych w całym systemie.
 * Struktury są zaprojektowane do przechowywania w FRAM.
 */

#ifndef DOSING_TYPES_H
#define DOSING_TYPES_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * Stan kanału (do wyświetlania w GUI)
 */
enum ChannelState : uint8_t {
    CH_STATE_INACTIVE   = 0,    // Żaden event nie zaznaczony
    CH_STATE_INCOMPLETE = 1,    // Brak dni lub dawki
    CH_STATE_INVALID    = 2,    // Walidacja nie przeszła
    CH_STATE_CONFIGURED = 3,    // Gotowy do pracy
    CH_STATE_PENDING    = 4     // Zmiany oczekujące (od jutra)
};

// ============================================================================
// CRITICAL ERROR TYPES (rozszerzone)
// ============================================================================

/**
 * Typ błędu krytycznego
 */
enum CriticalErrorType : uint8_t {
    ERROR_NONE                      = 0,
    ERROR_GPIO_PRE_CHECK_FAILED     = 1,    // Przewód urwany przed startem
    ERROR_GPIO_RUN_CHECK_FAILED     = 2,    // Przekaźnik nie zadziałał
    ERROR_GPIO_POST_CHECK_FAILED    = 3,    // Przekaźnik zablokowany ON (krytyczne!)
    ERROR_PUMP_TIMEOUT              = 4,
    ERROR_FRAM_FAILURE              = 5,
    ERROR_RTC_FAILURE               = 6,
    ERROR_RELAY_STUCK               = 7,
    ERROR_UNKNOWN                   = 255
};

/**
 * Faza walidacji GPIO
 */
enum ValidationPhase : uint8_t {
    PHASE_NONE      = 0,
    PHASE_PRE       = 1,    // Przed włączeniem przekaźnika
    PHASE_RUN       = 2,    // Po włączeniu, podczas pracy
    PHASE_POST      = 3     // Po wyłączeniu przekaźnika
};

/**
 * Status eventu
 */
enum EventStatus : uint8_t {
    EVENT_PENDING   = 0,    // Oczekuje na wykonanie
    EVENT_COMPLETED = 1,    // Wykonany pomyślnie
    EVENT_SKIPPED   = 2,    // Pominięty (daily dose osiągnięte)
    EVENT_FAILED    = 3     // Niepowodzenie
};

/**
 * Stan pompy
 */
enum PumpState : uint8_t {
    PUMP_IDLE       = 0,    // Pompa nieaktywna
    PUMP_RUNNING    = 1,    // Pompa pracuje
    PUMP_VALIDATING = 2,    // Oczekiwanie na walidację GPIO
    PUMP_ERROR      = 3     // Błąd pompy
};

// ============================================================================
// CHANNEL CONFIGURATION (przechowywana w FRAM)
// Rozmiar: 32 bajty (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Konfiguracja pojedynczego kanału
 * Przechowywana w FRAM - dwie kopie: active i pending
 */
struct ChannelConfig {
    // === Parametry użytkownika (12 bajtów) ===
    uint32_t events_bitmask;    // Bit 1-23 = godziny 01:00-23:00 (bit 0 unused)
    uint8_t  days_bitmask;      // Bit 0-6 = Pon-Ndz
    uint8_t  _reserved1[3];     // Padding/alignment
    float    daily_dose_ml;     // Dawka dzienna (ml)
    
    // === Parametry kalibracji (4 bajty) ===
    float    dosing_rate;       // Wydajność pompy (ml/s) z kalibracji
    
    // === Flagi (4 bajty) ===
    uint8_t  enabled;           // Czy kanał włączony (0/1)
    uint8_t  has_pending;       // Czy są oczekujące zmiany (0/1)
    uint8_t  _reserved2[2];     // Padding
    
    // === Checksum (4 bajty) ===
    uint32_t crc32;             // CRC32 dla walidacji danych
    
    // === Padding do 32 bajtów ===
    uint8_t  _padding[8];
    
    // ------------------------------------------
    // Metody pomocnicze (inline)
    // ------------------------------------------
    
    inline uint8_t getActiveEventsCount() const {
        return popcount32(events_bitmask);
    }
    
    inline uint8_t getActiveDaysCount() const {
        return popcount8(days_bitmask);
    }
    
    inline float getSingleDose() const {
        uint8_t evCnt = getActiveEventsCount();
        if (evCnt == 0 || daily_dose_ml <= 0) return 0.0f;
        return daily_dose_ml / (float)evCnt;
    }
    
    inline float getWeeklyDose() const {
        return daily_dose_ml * (float)getActiveDaysCount();
    }
    
    inline uint32_t getPumpDurationMs() const {
        float single = getSingleDose();
        if (dosing_rate <= 0 || single <= 0) return 0;
        return (uint32_t)((single / dosing_rate) * 1000.0f);
    }
    
    inline bool isEventEnabled(uint8_t hour) const {
        if (hour < FIRST_EVENT_HOUR || hour > LAST_EVENT_HOUR) return false;
        return BIT_CHECK(events_bitmask, hour);
    }
    
    inline bool isDayEnabled(uint8_t dayOfWeek) const {
        if (dayOfWeek > 6) return false;
        return BIT_CHECK(days_bitmask, dayOfWeek);
    }
};

#pragma pack(pop)

// Weryfikacja rozmiaru struktury
static_assert(sizeof(ChannelConfig) == 32, "ChannelConfig must be 32 bytes");

// ============================================================================
// DAILY STATE (resetowany o północy)
// Rozmiar: 16 bajtów (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Stan dzienny kanału - resetowany o 00:00 UTC
 * Rozmiar: 24 bajty (rozszerzone o events_failed)
 */
struct ChannelDailyState {
    uint32_t events_completed;  // Bitmask wykonanych OK (bit 1-23)
    uint32_t events_failed;     // Bitmask failed eventów (bit 1-23) [NOWE]
    float    today_added_ml;    // Suma dozowana dzisiaj (ml)
    uint8_t  last_reset_day;    // Dzień ostatniego resetu (UTC day % 256)
    uint8_t  failed_count;      // Liczba failed dziś [NOWE]
    uint8_t  _reserved[2];      // Padding
    uint32_t crc32;             // CRC32
    uint8_t  _padding[4];
    
    // ------------------------------------------
    // Metody pomocnicze
    // ------------------------------------------
    
    inline uint8_t getCompletedCount() const {
        return popcount32(events_completed);
    }
    
    inline uint8_t getFailedCount() const {
        return popcount32(events_failed);
    }
    
    inline bool isEventCompleted(uint8_t hour) const {
        if (hour < FIRST_EVENT_HOUR || hour > LAST_EVENT_HOUR) return false;
        return BIT_CHECK(events_completed, hour);
    }
    
    inline bool isEventFailed(uint8_t hour) const {
        if (hour < FIRST_EVENT_HOUR || hour > LAST_EVENT_HOUR) return false;
        return BIT_CHECK(events_failed, hour);
    }
    
    inline void markEventCompleted(uint8_t hour) {
        if (hour >= FIRST_EVENT_HOUR && hour <= LAST_EVENT_HOUR) {
            BIT_SET(events_completed, hour);
        }
    }
    
    inline void markEventFailed(uint8_t hour) {
        if (hour >= FIRST_EVENT_HOUR && hour <= LAST_EVENT_HOUR) {
            BIT_SET(events_failed, hour);
            failed_count++;
        }
    }
    
    inline void reset() {
        events_completed = 0;
        events_failed = 0;
        today_added_ml = 0.0f;
        failed_count = 0;
    }
};

#pragma pack(pop)

static_assert(sizeof(ChannelDailyState) == 24, "ChannelDailyState must be 24 bytes");

// ============================================================================
// SYSTEM STATE (globalny stan systemu)
// Rozmiar: 32 bajty (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Globalny stan systemu
 */
struct SystemState {
    uint8_t  system_enabled;        // Scheduler aktywny (0/1)
    uint8_t  system_halted;         // Błąd krytyczny - zatrzymanie (0/1)
    uint8_t  active_channel;        // Aktualnie aktywny kanał (255 = żaden)
    uint8_t  active_pump_state;     // PumpState aktywnej pompy
    uint32_t last_daily_reset_day;  // UTC day ostatniego daily reset
    uint32_t boot_count;            // Licznik restartów
    uint8_t  pending_changes_mask;  // Bitmask kanałów z pending changes
    uint8_t  _reserved[3];          // Padding
    uint32_t last_event_timestamp;  // Unix timestamp ostatniego eventu
    uint32_t crc32;                 // CRC32
    uint8_t  _padding[8];           // Padding do 32 bajtów
};

#pragma pack(pop)

static_assert(sizeof(SystemState) == 32, "SystemState must be 32 bytes");

// ============================================================================
// ERROR STATE (stan błędu krytycznego)
// Rozmiar: 16 bajtów (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Informacje o błędzie krytycznym
 */
struct ErrorState {
    CriticalErrorType error_type;   // Typ błędu
    uint8_t  affected_channel;      // Kanał, którego dotyczy błąd (255 = system)
    uint8_t  error_count;           // Licznik błędów (od ostatniego resetu)
    uint8_t  _reserved;             // Padding
    
    uint32_t error_timestamp;       // Unix timestamp błędu
    uint32_t error_data;            // Dodatkowe dane (zależne od typu błędu)
    uint32_t crc32;                 // CRC32
};

#pragma pack(pop)

static_assert(sizeof(ErrorState) == 16, "ErrorState must be 16 bytes");

// ============================================================================
// RUNTIME DATA (tylko w RAM, nie zapisywane do FRAM)
// ============================================================================

/**
 * Obliczone wartości kanału (dla GUI i logiki)
 */
struct ChannelCalculated {
    float    single_dose_ml;        // Pojedyncza dawka (ml)
    float    weekly_dose_ml;        // Tygodniowa suma (ml)
    float    today_remaining_ml;    // Pozostało na dziś (ml)
    uint32_t pump_duration_ms;      // Czas pracy pompy (ms)
    
    uint8_t  active_events_count;   // Liczba aktywnych eventów
    uint8_t  active_days_count;     // Liczba aktywnych dni
    uint8_t  completed_today_count; // Liczba wykonanych dziś
    uint8_t  next_event_hour;       // Następny event (255 = brak)
    
    ChannelState state;             // Stan kanału
    bool     is_valid;              // Czy konfiguracja poprawna
    bool     is_active_today;       // Czy kanał aktywny dzisiaj
    uint8_t  _padding;
};

/**
 * Stan aktualnego dozowania (gdy pompa pracuje)
 */
struct DosingContext {
    uint8_t  channel;               // Aktywny kanał
    uint8_t  event_hour;            // Godzina eventu
    PumpState pump_state;           // Stan pompy
    uint8_t  _reserved;
    
    uint32_t start_time_ms;         // millis() startu
    uint32_t target_duration_ms;    // Planowany czas pracy
    float    target_volume_ml;      // Planowana objętość
    
    bool     gpio_validated;        // Czy GPIO zwalidowane
    bool     completed;             // Czy zakończone
    uint8_t  _padding[2];
};

// ============================================================================
// CRITICAL ERROR STATE (32 bajty) - rozszerzona struktura
// ============================================================================

#pragma pack(push, 1)

/**
 * Rozszerzony stan błędu krytycznego
 * Przechowywany w FRAM - przetrwa reset/zanik zasilania
 */
struct CriticalErrorState {
    // === Aktywny błąd (12 bajtów) ===
    uint8_t  active_flag;           // 1 = błąd aktywny, 0 = brak
    CriticalErrorType error_type;   // Typ błędu
    uint8_t  channel;               // Kanał (255 = system)
    ValidationPhase phase;          // Faza walidacji
    uint32_t timestamp;             // Unix timestamp wystąpienia
    uint32_t error_data;            // Dodatkowe dane (np. stan GPIO)
    
    // === Snapshot stanu (4 bajty) ===
    uint8_t  gpio_state_snapshot;   // Stan wszystkich GPIO validation pins
    uint8_t  relay_state_snapshot;  // Stan wszystkich relay pins
    uint8_t  pump_was_running;      // Czy pompa pracowała (0/1)
    uint8_t  _reserved1;
    
    // === Historia (8 bajtów) ===
    uint16_t total_critical_errors; // Łączna liczba błędów (od factory reset)
    uint16_t reset_count;           // Ile razy resetowano błędy
    uint32_t last_reset_timestamp;  // Kiedy ostatnio zresetowano
    
    // === Integralność (8 bajtów) ===
    uint32_t write_count;           // Licznik zapisów (debug)
    uint32_t crc32;                 // CRC32 całości
};

#pragma pack(pop)

static_assert(sizeof(CriticalErrorState) == 32, "CriticalErrorState must be 32 bytes");

// ============================================================================
// CONTAINER VOLUME (pojemność pojemnika per kanał)
// Rozmiar: 8 bajtów (packed)
// ============================================================================

#pragma pack(push, 1)

// ============================================================================
// CONTAINER VOLUME (pojemność pojemnika per kanał)
// Rozmiar: 8 bajtów (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Pojemność i pozostała ilość płynu w pojemniku
 * Przechowywana w FRAM per kanał
 * Wartości przechowywane jako uint16_t × 10 (0.1ml precision)
 */
struct ContainerVolume {
    uint16_t container_ml;      // Pojemność pojemnika × 10 (max 6553.5ml)
    uint16_t remaining_ml;      // Pozostała ilość × 10
    uint32_t crc32;             // CRC32 validation
    
    // ------------------------------------------
    // Metody pomocnicze (inline)
    // ------------------------------------------
    
    inline float getContainerMl() const {
        return container_ml / 10.0f;
    }
    
    inline float getRemainingMl() const {
        return remaining_ml / 10.0f;
    }
    
    inline void setContainerMl(float ml) {
        container_ml = (uint16_t)(ml * 10.0f);
    }
    
    inline void setRemainingMl(float ml) {
        remaining_ml = (uint16_t)(ml * 10.0f);
    }
    
    inline uint8_t getRemainingPercent() const {
        if (container_ml == 0) return 0;
        return (uint8_t)((remaining_ml * 100UL) / container_ml);
    }
    
    inline bool isLowVolume(uint8_t threshold_pct = LOW_VOLUME_THRESHOLD_PCT) const {
        return getRemainingPercent() < threshold_pct;
    }
    
    inline void refill() {
        remaining_ml = container_ml;
    }
    
    inline void deduct(float ml) {
        uint16_t deduct_val = (uint16_t)(ml * 10.0f);
        if (deduct_val >= remaining_ml) {
            remaining_ml = 0;
        } else {
            remaining_ml -= deduct_val;
        }
    }
    
    inline void reset() {
        container_ml = CONTAINER_DEFAULT_ML * 10;
        remaining_ml = container_ml;
    }
};

#pragma pack(pop)

static_assert(sizeof(ContainerVolume) == 8, "ContainerVolume must be 8 bytes");

// ============================================================================
// DOSED TRACKER (suma dozowana od ostatniego resetu)
// Rozmiar: 8 bajtów (packed)
// ============================================================================

#pragma pack(push, 1)

/**
 * Tracker sumy dozowanej od ostatniego ręcznego resetu
 * Przechowywana w FRAM per kanał
 * Wartość przechowywana jako uint16_t × 10 (0.1ml precision)
 */
struct DosedTracker {
    uint16_t total_dosed_ml;    // Suma dozowana × 10 (max 6553.5ml)
    uint16_t _reserved;         // Padding for alignment
    uint32_t crc32;             // CRC32 validation

    // ------------------------------------------
    // Metody pomocnicze (inline)
    // ------------------------------------------

    inline float getTotalDosedMl() const {
        return total_dosed_ml / 10.0f;
    }

    inline void addDosed(float ml) {
        uint32_t newVal = total_dosed_ml + (uint16_t)(ml * 10.0f);
        // Cap at max uint16_t value
        if (newVal > 65535) newVal = 65535;
        total_dosed_ml = (uint16_t)newVal;
    }

    inline void reset() {
        total_dosed_ml = 0;
        _reserved = 0;
    }

    inline uint8_t getPercentOfWeekly(float weekly_ml) const {
        if (weekly_ml <= 0) return 0;
        float pct = (getTotalDosedMl() / weekly_ml) * 100.0f;
        if (pct > 100.0f) return 100;  // Cap at 100% for display
        return (uint8_t)pct;
    }
};

#pragma pack(pop)

static_assert(sizeof(DosedTracker) == 8, "DosedTracker must be 8 bytes");

// ============================================================================
// UTILITY FUNCTIONS (deklaracje)
// ============================================================================

/**
 * Oblicz CRC32 dla bloku danych
 */
uint32_t calculateCRC32(const void* data, size_t length);

/**
 * Waliduj strukturę przez CRC32
 */
bool validateCRC32(const void* data, size_t length, uint32_t expected_crc);

/**
 * Konwersja ChannelState na string
 */
const char* channelStateToString(ChannelState state);

/**
 * Konwersja CriticalErrorType na string
 */
const char* errorTypeToString(CriticalErrorType error);

/**
 * Pobierz aktualny dzień tygodnia (0=Pon, 6=Ndz) z UTC timestamp
 */
uint8_t getDayOfWeek(uint32_t unixTimestamp);

/**
 * Pobierz godzinę UTC z timestamp
 */
uint8_t getHourUTC(uint32_t unixTimestamp);

/**
 * Pobierz UTC day (dni od epoch)
 */
uint32_t getUTCDay(uint32_t unixTimestamp);

#endif // DOSING_TYPES_H