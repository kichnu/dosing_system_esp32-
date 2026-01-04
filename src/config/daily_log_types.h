/**
 * @file daily_log_types.h
 * @brief Struktury danych dla Daily Log Ring Buffer
 * 
 * System przechowuje dobowe podsumowania dozowania w FRAM jako ring buffer.
 * Każdy wpis (192 B) zawiera dane wszystkich 6 kanałów + metadane systemowe.
 */

#ifndef DAILY_LOG_TYPES_H
#define DAILY_LOG_TYPES_H

#include <cstdint>
#include "config.h"

// ============================================================================
// Stałe Daily Log
// ============================================================================

// Maksymalna liczba kanałów w strukturze (stała dla kompatybilności FRAM)
// Niezależna od aktualnego CHANNEL_COUNT - zawsze rezerwujemy miejsce na 6
#define DAILY_LOG_MAX_CHANNELS          6

#ifndef FRAM_SIZE_DAILY_LOG_ENTRY
#define FRAM_SIZE_DAILY_LOG_ENTRY       192
#endif

#ifndef FRAM_DAILY_LOG_CAPACITY
#define FRAM_DAILY_LOG_CAPACITY         100
#endif

#ifndef FRAM_MAGIC_DAILY_LOG
#define FRAM_MAGIC_DAILY_LOG            0x444C4F47  // "DLOG"
#endif

#ifndef DAILY_LOG_VERSION_CURRENT
#define DAILY_LOG_VERSION_CURRENT       1
#endif

// ============================================================================
// Enumy statusów
// ============================================================================

enum class DayChannelStatus : uint8_t {
    INACTIVE = 0,
    OK = 1,
    PARTIAL = 2,
    SKIPPED = 3,
    ERROR = 4
};

enum class DayChannelErrorType : uint8_t {
    NONE = 0,
    PUMP_STUCK = 1,
    RELAY_FAILURE = 2,
    TIMEOUT = 3,
    VALIDATION_FAILED = 4,
    OTHER = 255
};

namespace DayFlags {
    constexpr uint8_t COMPLETE        = 0x01;
    constexpr uint8_t POWER_LOST      = 0x02;
    constexpr uint8_t CRITICAL_ERROR  = 0x04;
    constexpr uint8_t MANUAL_DOSE     = 0x08;
    constexpr uint8_t CONFIG_CHANGED  = 0x10;
    constexpr uint8_t TIME_ADJUSTED   = 0x20;
    constexpr uint8_t INCOMPLETE      = 0x40;
    constexpr uint8_t RESERVED        = 0x80;
}

// ============================================================================
// WSZYSTKIE STRUKTURY W JEDNYM BLOKU PRAGMA PACK
// ============================================================================

#pragma pack(push, 1)

// ----------------------------------------------------------------------------
// DayChannelData (24 bajty)
// ----------------------------------------------------------------------------
struct DayChannelData {
    uint8_t events_planned;
    uint8_t days_active;
    uint16_t dose_planned_ml;
    uint8_t events_completed;
    uint8_t events_failed;
    uint16_t dose_actual_ml;
    DayChannelStatus status;
    DayChannelErrorType error_type;
    uint8_t error_hour;
    uint8_t error_minute;
    uint8_t reserved[12];
    
    DayChannelData() :
        events_planned(0), days_active(0), dose_planned_ml(0),
        events_completed(0), events_failed(0), dose_actual_ml(0),
        status(DayChannelStatus::INACTIVE), error_type(DayChannelErrorType::NONE),
        error_hour(255), error_minute(255), reserved{0} {}
    
    float getDosePlannedMl() const { return dose_planned_ml / 100.0f; }
    float getDoseActualMl() const { return dose_actual_ml / 100.0f; }
    void setDosePlannedMl(float ml) { dose_planned_ml = static_cast<uint16_t>(ml * 100); }
    void addDoseActualMl(float ml) { dose_actual_ml += static_cast<uint16_t>(ml * 100); }
    bool hasError() const { return error_type != DayChannelErrorType::NONE; }
    bool isActive() const { return status != DayChannelStatus::INACTIVE; }
};

// ----------------------------------------------------------------------------
// DayLogEntry (192 bajty)
// Uwaga: zawsze 6 kanałów dla kompatybilności FRAM, nieaktywne mają status INACTIVE
// ----------------------------------------------------------------------------
struct DayLogEntry {
    // HEADER (8B)
    uint32_t utc_day;
    uint8_t day_of_week;
    uint8_t flags;
    uint8_t version;
    uint8_t channel_count;      // Aktualny CHANNEL_COUNT w momencie zapisu
    
    // CHANNELS (144B) - zawsze 6 slotów
    DayChannelData channels[DAILY_LOG_MAX_CHANNELS];
    
    // SYSTEM INFO (24B)
    uint32_t uptime_seconds;
    uint16_t power_cycles;
    uint16_t wifi_disconnects;
    uint16_t ntp_syncs;
    uint16_t fram_writes;
    uint8_t min_heap_kb;
    uint8_t max_temp_c;
    uint8_t system_reserved[10];
    
    // CRITICAL ERROR (8B)
    uint8_t critical_error_type;
    uint8_t critical_error_channel;
    uint8_t critical_error_hour;
    uint8_t critical_error_minute;
    uint8_t critical_reserved[4];
    
    // INTEGRITY (8B)
    uint32_t write_timestamp;
    uint32_t crc32;
    
    DayLogEntry() :
        utc_day(0), day_of_week(0), flags(0),
        version(DAILY_LOG_VERSION_CURRENT), channel_count(CHANNEL_COUNT),
        channels{},
        uptime_seconds(0), power_cycles(0), wifi_disconnects(0),
        ntp_syncs(0), fram_writes(0), min_heap_kb(255), max_temp_c(0),
        system_reserved{0},
        critical_error_type(0), critical_error_channel(255),
        critical_error_hour(255), critical_error_minute(255),
        critical_reserved{0},
        write_timestamp(0), crc32(0) {}
    
    bool isFinalized() const { return flags & DayFlags::COMPLETE; }
    bool hasCriticalError() const { return flags & DayFlags::CRITICAL_ERROR; }
    bool hadPowerLoss() const { return flags & DayFlags::POWER_LOST; }
    
    void markFinalized() { flags |= DayFlags::COMPLETE; }
    void markPowerLost() { flags |= DayFlags::POWER_LOST; }
    void markCriticalError() { flags |= DayFlags::CRITICAL_ERROR; }
    void markManualDose() { flags |= DayFlags::MANUAL_DOSE; }
    void markConfigChanged() { flags |= DayFlags::CONFIG_CHANGED; }
    void markTimeAdjusted() { flags |= DayFlags::TIME_ADJUSTED; }
    
    DayChannelStatus getAggregatedStatus() const {
        bool hasError = false, hasPartial = false, hasOk = false;
        for (int i = 0; i < DAILY_LOG_MAX_CHANNELS; i++) {
            if (channels[i].status == DayChannelStatus::INACTIVE) continue;
            switch (channels[i].status) {
                case DayChannelStatus::ERROR: hasError = true; break;
                case DayChannelStatus::PARTIAL: hasPartial = true; break;
                case DayChannelStatus::OK: hasOk = true; break;
                default: break;
            }
        }
        if (hasError) return DayChannelStatus::ERROR;
        if (hasPartial) return DayChannelStatus::PARTIAL;
        if (hasOk) return DayChannelStatus::OK;
        return DayChannelStatus::INACTIVE;
    }
};

// ----------------------------------------------------------------------------
// DailyLogRingHeader (32 bajty)
// ----------------------------------------------------------------------------
struct DailyLogRingHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t entry_size;
    uint8_t capacity;
    uint8_t count;
    uint8_t head_index;
    uint8_t tail_index;
    uint8_t reserved1[2];
    uint32_t total_entries_written;
    uint32_t first_day_utc;
    uint32_t last_day_utc;
    uint32_t write_count;
    uint32_t crc32;
    
    DailyLogRingHeader() :
        magic(FRAM_MAGIC_DAILY_LOG), version(DAILY_LOG_VERSION_CURRENT),
        entry_size(FRAM_SIZE_DAILY_LOG_ENTRY), capacity(FRAM_DAILY_LOG_CAPACITY),
        count(0), head_index(0), tail_index(0), reserved1{0},
        total_entries_written(0), first_day_utc(0), last_day_utc(0),
        write_count(0), crc32(0) {}
    
    bool isValid() const {
        return magic == FRAM_MAGIC_DAILY_LOG &&
               version <= DAILY_LOG_VERSION_CURRENT &&
               entry_size == FRAM_SIZE_DAILY_LOG_ENTRY &&
               capacity == FRAM_DAILY_LOG_CAPACITY;
    }
    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count >= capacity; }
};

#pragma pack(pop)

// ============================================================================
// Static asserts (PO zamknięciu pragma pack)
// ============================================================================

static_assert(sizeof(DayChannelData) == 24, "DayChannelData must be 24 bytes!");
static_assert(sizeof(DayLogEntry) == 192, "DayLogEntry must be 192 bytes!");
static_assert(sizeof(DailyLogRingHeader) == 32, "DailyLogRingHeader must be 32 bytes!");

// ============================================================================
// Funkcje pomocnicze
// ============================================================================

inline uint32_t timestampToUtcDay(uint32_t timestamp) {
    return timestamp / 86400;
}

inline uint32_t utcDayToTimestamp(uint32_t utcDay) {
    return utcDay * 86400;
}

#endif // DAILY_LOG_TYPES_H