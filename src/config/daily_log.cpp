/**
 * @file daily_log.cpp
 * @brief Implementacja Daily Log Ring Buffer
 */

#include "daily_log.h"
#include "daily_log_types.h"

#include "fram_layout.h"
#include "fram_controller.h"
#include "rtc_controller.h"
#include <cstring>
#include <Arduino.h>
#include "channel_manager.h"

// Globalna instancja
DailyLogManager* g_dailyLog = nullptr;

// ============================================================================
// Tablica CRC32 (polynomial 0xEDB88320)
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7A9C, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// ============================================================================
// Inicjalizacja
// ============================================================================

DailyLogResult DailyLogManager::init() {
    Serial.println(F("[DailyLog] Initializing..."));
    
    // Wczytaj nagłówki i wybierz aktualny
    auto result = loadHeader();
    if (result != DailyLogResult::OK) {
        Serial.println(F("[DailyLog] Header load failed, creating new buffer"));
        // Tworzymy pusty buffer
        header_ = DailyLogRingHeader();
        result = saveHeader();
        if (result != DailyLogResult::OK) {
            return result;
        }
    }
    
    Serial.printf("[DailyLog] Initialized: %d entries, days %lu-%lu\n", 
                  header_.count, header_.first_day_utc, header_.last_day_utc);
    
    initialized_ = true;
    return DailyLogResult::OK;
}

DailyLogResult DailyLogManager::reset() {
    Serial.println(F("[DailyLog] Resetting buffer..."));
    
    header_ = DailyLogRingHeader();
    current_entry_ = DayLogEntry();
    current_entry_dirty_ = false;
    
    return saveHeader();
}

// ============================================================================
// Zapis zdarzeń
// ============================================================================

DailyLogResult DailyLogManager::recordDosing(uint8_t channel, float dose_ml, bool success) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    if (channel >= CHANNEL_COUNT) return DailyLogResult::ERROR_INVALID_PARAM;
    
    // Pobierz aktualny timestamp z RTC
    
    uint32_t now = rtcController.getUnixTime();
    uint32_t utc_day = timestampToUtcDay(now);
    
    // Upewnij się że mamy wpis dla dzisiejszego dnia
    auto result = ensureCurrentEntry(utc_day);
    if (result != DailyLogResult::OK) return result;
    
    // Aktualizuj dane kanału
    DayChannelData& ch = current_entry_.channels[channel];
    
    if (success) {
        ch.events_completed++;
        ch.addDoseActualMl(dose_ml);
    } else {
        ch.events_failed++;
        // Zapisz czas błędu jeśli to pierwszy błąd
        if (!ch.hasError()) {
            ch.error_hour = (now % 86400) / 3600;
            ch.error_minute = ((now % 86400) % 3600) / 60;
            ch.error_type = DayChannelErrorType::OTHER;
        }
    }

        if (ch.events_failed > 0) {
        ch.status = DayChannelStatus::ERROR;
    } else if (ch.events_completed >= ch.events_planned && ch.events_planned > 0) {
        ch.status = DayChannelStatus::OK;
    } else if (ch.events_completed > 0) {
        ch.status = DayChannelStatus::PARTIAL;
    }
    
    current_entry_dirty_ = true;
    current_entry_.fram_writes++;

    Serial.printf("[DailyLog] CH%d: %.2f ml, %s\n", 
                  channel, dose_ml, success ? "OK" : "FAILED");
    
    // Zapisz do FRAM
    return commitCurrentEntry();
}

DailyLogResult DailyLogManager::recordCriticalError(uint8_t error_type, uint8_t channel) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    
    uint32_t now = rtcController.getUnixTime();
    uint32_t utc_day = timestampToUtcDay(now);
    
    auto result = ensureCurrentEntry(utc_day);
    if (result != DailyLogResult::OK) return result;
    
    // Zapisz błąd krytyczny
    current_entry_.critical_error_type = error_type;
    current_entry_.critical_error_channel = channel;
    current_entry_.critical_error_hour = (now % 86400) / 3600;
    current_entry_.critical_error_minute = ((now % 86400) % 3600) / 60;
    current_entry_.markCriticalError();
    
    // Aktualizuj status kanału jeśli dotyczy konkretnego kanału
    if (channel < CHANNEL_COUNT) {
        DayChannelData& ch = current_entry_.channels[channel];
        ch.status = DayChannelStatus::ERROR;
        ch.error_type = static_cast<DayChannelErrorType>(error_type);
        ch.error_hour = current_entry_.critical_error_hour;
        ch.error_minute = current_entry_.critical_error_minute;
    }
    
    current_entry_dirty_ = true;
    
    // Natychmiast zapisz do FRAM
    return commitCurrentEntry();
}

DailyLogResult DailyLogManager::recordPowerCycle() {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    Serial.printf("[DailyLog] recordPowerCycle() - current utc_day=%lu, power_cycles=%d\n", 
                  current_entry_.utc_day, current_entry_.power_cycles);
    
    uint32_t now = rtcController.getUnixTime();
    uint32_t utc_day = timestampToUtcDay(now);
    
    Serial.printf("[DailyLog] Target utc_day=%lu\n", utc_day);
    
    auto result = ensureCurrentEntry(utc_day);
    if (result != DailyLogResult::OK) {
        Serial.printf("[DailyLog] ensureCurrentEntry FAILED: %d\n", (int)result);
        return result;
    }
    
    current_entry_.power_cycles++;
    current_entry_.markPowerLost();
    current_entry_dirty_ = true;
    
    Serial.printf("[DailyLog] After increment: power_cycles=%d\n", current_entry_.power_cycles);
    
    return commitCurrentEntry();
}

DailyLogResult DailyLogManager::recordWifiDisconnect() {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    uint32_t utc_day = timestampToUtcDay(rtcController.getUnixTime());
    
    auto result = ensureCurrentEntry(utc_day);
    if (result != DailyLogResult::OK) return result;
    
    current_entry_.wifi_disconnects++;
    current_entry_dirty_ = true;
    
    // Zapisz od razu - disconnect może poprzedzać restart
    return commitCurrentEntry();
}

DailyLogResult DailyLogManager::recordNtpSync() {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    
    uint32_t utc_day = timestampToUtcDay(rtcController.getUnixTime());
    
    auto result = ensureCurrentEntry(utc_day);
    if (result != DailyLogResult::OK) return result;
    
    current_entry_.ntp_syncs++;
    current_entry_.markTimeAdjusted();
    current_entry_dirty_ = true;
    
    return DailyLogResult::OK;
}

void DailyLogManager::updateSystemStats(uint32_t uptime_seconds, uint8_t free_heap_kb, uint8_t temp_celsius) {
    if (!initialized_) return;
    
    current_entry_.uptime_seconds = uptime_seconds;
    
    // Minimum heap (śledzenie najniższej wartości)
    if (free_heap_kb < current_entry_.min_heap_kb) {
        current_entry_.min_heap_kb = free_heap_kb;
    }
    
    // Maximum temperatura
    if (temp_celsius > current_entry_.max_temp_c) {
        current_entry_.max_temp_c = temp_celsius;
    }
    
    current_entry_dirty_ = true;
    // Nie zapisujemy od razu - zbyt częste zapisy
}

// ============================================================================
// Finalizacja dnia
// ============================================================================

DailyLogResult DailyLogManager::finalizeDay() {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    if (!current_entry_dirty_ && current_entry_.utc_day == 0) {
        // Nie ma bieżącego wpisu do finalizacji
        return DailyLogResult::OK;
    }
    
    Serial.printf("[DailyLog] Finalizing day %lu\n", current_entry_.utc_day);
    
    // Oblicz końcowe statusy kanałów
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        DayChannelData& ch = current_entry_.channels[i];
        
        // Jeśli status już ustawiony (ERROR) - nie zmieniaj
        if (ch.status == DayChannelStatus::ERROR) continue;
        
        // Jeśli kanał nieaktywny
        if (ch.events_planned == 0) {
            ch.status = DayChannelStatus::INACTIVE;
            continue;
        }
        
        // Oblicz status na podstawie wykonania
        if (ch.events_completed >= ch.events_planned) {
            ch.status = DayChannelStatus::OK;
        } else if (ch.events_completed > 0) {
            ch.status = DayChannelStatus::PARTIAL;
        } else {
            ch.status = DayChannelStatus::SKIPPED;
        }
    }
    
    // Oznacz jako sfinalizowany
    current_entry_.markFinalized();
    
    // Zapisz do FRAM
    auto result = commitCurrentEntry();
    if (result != DailyLogResult::OK) return result;
    
    // Wyczyść bufor dla następnego dnia
    current_entry_ = DayLogEntry();
    current_entry_dirty_ = false;
    
    return DailyLogResult::OK;
}

DailyLogResult DailyLogManager::initializeNewDay(uint32_t current_timestamp) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    uint32_t utc_day = timestampToUtcDay(current_timestamp);
    
    // Sprawdź czy ten dzień już jest w RAM
    if (current_entry_.utc_day == utc_day) {
        Serial.printf("[DailyLog] Day %lu already active in RAM, skipping init\n", utc_day);
        return DailyLogResult::OK;
    }
    
    // Sprawdź czy ten dzień już istnieje w FRAM (np. po restarcie)
    if (header_.count > 0 && header_.last_day_utc == utc_day) {
        DayLogEntry existing;
        if (loadEntry(header_.head_index, existing) == DailyLogResult::OK) {
            if (!existing.isFinalized()) {
                current_entry_ = existing;
                current_entry_dirty_ = false;
                Serial.printf("[DailyLog] Resumed existing entry for day %lu (power_cycles=%d)\n", 
                              utc_day, current_entry_.power_cycles);
                return DailyLogResult::OK;
            }
        }
    }
    
    // Jeśli jest niezapisany poprzedni dzień - najpierw sfinalizuj
    if (current_entry_dirty_ && current_entry_.utc_day != 0 && current_entry_.utc_day != utc_day) {
        auto result = finalizeDay();
        if (result != DailyLogResult::OK) {
            Serial.println(F("[DailyLog] Warning: Failed to finalize previous day"));
        }
    }
    
    // Oblicz dzień tygodnia
    uint8_t day_of_week = (4 + utc_day) % 7;
    
    // Inicjalizuj nowy wpis
    initEmptyEntry(current_entry_, utc_day, day_of_week);

    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
    const ChannelConfig& cfg = channelManager.getActiveConfig(ch);
    const ChannelCalculated& calc = channelManager.getCalculated(ch);
        
        if (cfg.enabled && calc.is_valid) {
            // Sprawdź czy dzisiaj jest aktywny dzień
            // day_of_week: 0=Nd, 1=Pn, ... 6=So
            // days_bitmask: bit 0=Pn, bit 1=Wt, ... bit 6=Nd
            uint8_t dayBit = (day_of_week == 0) ? 6 : (day_of_week - 1);
            
            if (cfg.days_bitmask & (1 << dayBit)) {
                current_entry_.channels[ch].events_planned = calc.active_events_count;
                current_entry_.channels[ch].days_active = cfg.days_bitmask;
                current_entry_.channels[ch].setDosePlannedMl(cfg.daily_dose_ml);
                current_entry_.channels[ch].status = DayChannelStatus::SKIPPED; // Domyślnie - zmieni się po dozowaniu
            }
        }
    }

    // Debug - sprawdź czy plan został wypełniony
    Serial.println(F("[DailyLog] Plan for today:"));
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        Serial.printf("  CH%d: planned=%d, dose=%.1f ml\n", 
                      ch, 
                      current_entry_.channels[ch].events_planned,
                      current_entry_.channels[ch].getDosePlannedMl());
    }
    
    current_entry_dirty_ = true;
    
    Serial.printf("[DailyLog] New day initialized: %lu (DoW: %d)\n", utc_day, day_of_week);
    
    return commitCurrentEntry();
}

DailyLogResult DailyLogManager::fillTodayPlan() {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    if (current_entry_.utc_day == 0) return DailyLogResult::ERROR_ENTRY_NOT_FOUND;
    
    uint8_t day_of_week = current_entry_.day_of_week;
    
    Serial.println(F("[DailyLog] Filling today's plan..."));
    
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ch++) {
        const ChannelConfig& cfg = channelManager.getActiveConfig(ch);
        const ChannelCalculated& calc = channelManager.getCalculated(ch);
        
        if (cfg.enabled && calc.is_valid) {
            uint8_t dayBit = (day_of_week == 0) ? 6 : (day_of_week - 1);
            
            if (cfg.days_bitmask & (1 << dayBit)) {
                // Zachowaj istniejące dane jeśli są
                uint8_t existingCompleted = current_entry_.channels[ch].events_completed;
                uint8_t existingFailed = current_entry_.channels[ch].events_failed;
                float existingDose = current_entry_.channels[ch].getDoseActualMl();
                
                current_entry_.channels[ch].events_planned = calc.active_events_count;
                current_entry_.channels[ch].days_active = cfg.days_bitmask;
                current_entry_.channels[ch].setDosePlannedMl(cfg.daily_dose_ml);
                
                // Przywróć istniejące dane
                current_entry_.channels[ch].events_completed = existingCompleted;
                current_entry_.channels[ch].events_failed = existingFailed;
                current_entry_.channels[ch].dose_actual_ml = static_cast<uint16_t>(existingDose * 100);
                
                if (existingCompleted == 0 && existingFailed == 0) {
                    current_entry_.channels[ch].status = DayChannelStatus::SKIPPED;
                }
                
                Serial.printf("  CH%d: %d events, %.1f ml planned (existing: %d done)\n", 
                              ch + 1, calc.active_events_count, cfg.daily_dose_ml, existingCompleted);
            }
        }
    }
    
    current_entry_dirty_ = true;
    return commitCurrentEntry();
}

// ============================================================================
// Odczyt wpisów
// ============================================================================

DailyLogResult DailyLogManager::getEntry(uint8_t index, DayLogEntry& entry) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    // Liczymy tylko sfinalizowane wpisy
    uint8_t finalized_count = getFinalizedCount();
    
    if (index >= finalized_count) {
        return DailyLogResult::ERROR_ENTRY_NOT_FOUND;
    }
    
    // Konwertuj indeks logiczny na indeks w ring buffer
    // index=0 to najnowszy sfinalizowany
    uint8_t ring_index = indexToRingIndex(index);
    
    return loadEntry(ring_index, entry);
}

DailyLogResult DailyLogManager::getEntryByDay(uint32_t utc_day, DayLogEntry& entry) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    // Sprawdź zakres
    if (header_.count == 0 || utc_day < header_.first_day_utc || utc_day > header_.last_day_utc) {
        return DailyLogResult::ERROR_ENTRY_NOT_FOUND;
    }
    
    // Szukaj wpisu
    for (uint8_t i = 0; i < header_.count; i++) {
        uint8_t ring_index = (header_.tail_index + i) % header_.capacity;
        DayLogEntry temp;
        
        if (loadEntry(ring_index, temp) == DailyLogResult::OK) {
            if (temp.utc_day == utc_day) {
                entry = temp;
                return DailyLogResult::OK;
            }
        }
    }
    
    return DailyLogResult::ERROR_ENTRY_NOT_FOUND;
}

DailyLogResult DailyLogManager::getCurrentEntry(DayLogEntry& entry) {
    if (!initialized_) return DailyLogResult::ERROR_NOT_INITIALIZED;
    
    if (current_entry_.utc_day == 0) {
        return DailyLogResult::ERROR_ENTRY_NOT_FOUND;
    }
    
    entry = current_entry_;
    return DailyLogResult::OK;
}

DailyLogStats DailyLogManager::getStats() const {
    DailyLogStats stats;
    stats.count = header_.count;
    stats.capacity = header_.capacity;
    stats.total_written = header_.total_entries_written;
    stats.first_day = header_.first_day_utc;
    stats.last_day = header_.last_day_utc;
    stats.has_current_day = (current_entry_.utc_day != 0);
    return stats;
}

uint8_t DailyLogManager::getFinalizedCount() const {
    // Jeśli mamy bieżący niesfinalizowany wpis, odejmujemy 1
    if (current_entry_.utc_day != 0 && !current_entry_.isFinalized()) {
        return (header_.count > 0) ? header_.count - 1 : 0;
    }
    return header_.count;
}

void DailyLogManager::iterateEntries(std::function<bool(const DayLogEntry&, uint8_t)> callback, 
                                      uint8_t max_entries) {
    if (!initialized_ || header_.count == 0) return;
    
    uint8_t count = getFinalizedCount();
    if (max_entries > 0 && max_entries < count) {
        count = max_entries;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        DayLogEntry entry;
        if (getEntry(i, entry) == DailyLogResult::OK) {
            if (!callback(entry, i)) {
                break;  // Przerwij jeśli callback zwrócił false
            }
        }
    }
}

// ============================================================================
// Diagnostyka
// ============================================================================

uint8_t DailyLogManager::validateBuffer() {
    if (!initialized_) return 255;
    
    uint8_t corrupted = 0;
    
    for (uint8_t i = 0; i < header_.count; i++) {
        uint8_t ring_index = (header_.tail_index + i) % header_.capacity;
        DayLogEntry entry;
        
        if (loadEntry(ring_index, entry) != DailyLogResult::OK) {
            corrupted++;
        }
    }
    
    return corrupted;
}

const char* DailyLogManager::getLastErrorString() const {
    switch (last_error_) {
        case DailyLogResult::OK: return "OK";
        case DailyLogResult::ERROR_NOT_INITIALIZED: return "Not initialized";
        case DailyLogResult::ERROR_FRAM_READ: return "FRAM read error";
        case DailyLogResult::ERROR_FRAM_WRITE: return "FRAM write error";
        case DailyLogResult::ERROR_CRC_MISMATCH: return "CRC mismatch";
        case DailyLogResult::ERROR_ENTRY_NOT_FOUND: return "Entry not found";
        case DailyLogResult::ERROR_BUFFER_FULL: return "Buffer full";
        case DailyLogResult::ERROR_INVALID_PARAM: return "Invalid parameter";
        case DailyLogResult::ERROR_HEADER_CORRUPT: return "Header corrupt";
        default: return "Unknown error";
    }
}

// ============================================================================
// Metody wewnętrzne - Nagłówek
// ============================================================================

DailyLogResult DailyLogManager::loadHeader() {
    DailyLogRingHeader headerA, headerB;
    bool validA = false, validB = false;
    
    // Wczytaj nagłówek A
    if (framController.readBytes(FRAM_ADDR_DAILY_LOG_HEADER_A, (uint8_t*)&headerA, sizeof(headerA))) {
        validA = validateHeaderCRC(headerA);
    }
    
    // Wczytaj nagłówek B
    if (framController.readBytes(FRAM_ADDR_DAILY_LOG_HEADER_B, (uint8_t*)&headerB, sizeof(headerB))) {
        validB = validateHeaderCRC(headerB);
    }
    
    // Wybierz aktualny nagłówek
    if (validA && validB) {
        // Oba ważne - wybierz nowszy (wyższy write_count)
        header_ = (headerA.write_count >= headerB.write_count) ? headerA : headerB;
        Serial.printf("[DailyLog] Both headers valid, using %c (write_count=%lu)\n",
                     (headerA.write_count >= headerB.write_count) ? 'A' : 'B',
                     header_.write_count);
    } else if (validA) {
        header_ = headerA;
        Serial.println(F("[DailyLog] Using header A"));
    } else if (validB) {
        header_ = headerB;
        Serial.println(F("[DailyLog] Using header B"));
    } else {
        Serial.println(F("[DailyLog] No valid header found"));
        last_error_ = DailyLogResult::ERROR_HEADER_CORRUPT;
        return DailyLogResult::ERROR_HEADER_CORRUPT;
    }
    
    return DailyLogResult::OK;
}

DailyLogResult DailyLogManager::saveHeader() {
    header_.write_count++;
    calculateHeaderCRC(header_);
    
    // Zapisz najpierw B, potem A (dla atomowości)
    if (!framController.writeBytes(FRAM_ADDR_DAILY_LOG_HEADER_B, (uint8_t*)&header_, sizeof(header_))) {
        last_error_ = DailyLogResult::ERROR_FRAM_WRITE;
        return DailyLogResult::ERROR_FRAM_WRITE;
    }
    
    if (!framController.writeBytes(FRAM_ADDR_DAILY_LOG_HEADER_A, (uint8_t*)&header_, sizeof(header_))) {
        last_error_ = DailyLogResult::ERROR_FRAM_WRITE;
        return DailyLogResult::ERROR_FRAM_WRITE;
    }
    
    return DailyLogResult::OK;
}

bool DailyLogManager::validateHeaderCRC(const DailyLogRingHeader& h) {
    if (h.magic != FRAM_MAGIC_DAILY_LOG) return false;
    if (!h.isValid()) return false;
    
    // Oblicz CRC (bez pola crc32)
    uint32_t calculated = calculateCRC32((uint8_t*)&h, sizeof(h) - sizeof(h.crc32));
    return calculated == h.crc32;
}

void DailyLogManager::calculateHeaderCRC(DailyLogRingHeader& h) {
    h.crc32 = calculateCRC32((uint8_t*)&h, sizeof(h) - sizeof(h.crc32));
}

// ============================================================================
// Metody wewnętrzne - Wpisy
// ============================================================================

DailyLogResult DailyLogManager::loadEntry(uint8_t ring_index, DayLogEntry& entry) {
    uint32_t addr = FRAM_DAILY_LOG_ENTRY_ADDR(ring_index);
    
    if (!framController.readBytes(addr, (uint8_t*)&entry, sizeof(entry))) {
        last_error_ = DailyLogResult::ERROR_FRAM_READ;
        return DailyLogResult::ERROR_FRAM_READ;
    }
    
    if (!validateEntryCRC(entry)) {
        last_error_ = DailyLogResult::ERROR_CRC_MISMATCH;
        return DailyLogResult::ERROR_CRC_MISMATCH;
    }
    
    return DailyLogResult::OK;
}

DailyLogResult DailyLogManager::saveEntry(uint8_t ring_index, DayLogEntry& entry) {
    
    entry.write_timestamp = rtcController.getUnixTime();
    calculateEntryCRC(entry);
    
    uint32_t addr = FRAM_DAILY_LOG_ENTRY_ADDR(ring_index);
    
    if (!framController.writeBytes(addr, (uint8_t*)&entry, sizeof(entry))) {
        last_error_ = DailyLogResult::ERROR_FRAM_WRITE;
        return DailyLogResult::ERROR_FRAM_WRITE;
    }
    
    return DailyLogResult::OK;
}

bool DailyLogManager::validateEntryCRC(const DayLogEntry& e) {
    uint32_t calculated = calculateCRC32((uint8_t*)&e, sizeof(e) - sizeof(e.crc32));
    return calculated == e.crc32;
}

void DailyLogManager::calculateEntryCRC(DayLogEntry& e) {
    e.crc32 = calculateCRC32((uint8_t*)&e, sizeof(e) - sizeof(e.crc32));
}

// ============================================================================
// Metody wewnętrzne - Ring Buffer
// ============================================================================

uint8_t DailyLogManager::nextIndex(uint8_t current) const {
    return (current + 1) % header_.capacity;
}

uint8_t DailyLogManager::prevIndex(uint8_t current) const {
    return (current == 0) ? header_.capacity - 1 : current - 1;
}

uint8_t DailyLogManager::indexToRingIndex(uint8_t logical_index) const {
    // logical_index=0 to najnowszy (head)
    // Dla sfinalizowanych: head wskazuje na ostatni wpis (może być niesfinalizowany)
    uint8_t finalized_head = header_.head_index;
    
    // Jeśli bieżący wpis istnieje i nie jest sfinalizowany, cofamy head
    if (current_entry_.utc_day != 0 && !current_entry_.isFinalized()) {
        finalized_head = prevIndex(finalized_head);
    }
    
    // Cofaj od head o logical_index pozycji
    uint8_t result = finalized_head;
    for (uint8_t i = 0; i < logical_index; i++) {
        result = prevIndex(result);
    }
    
    return result;
}

// ============================================================================
// Metody wewnętrzne - Pomocnicze
// ============================================================================

DailyLogResult DailyLogManager::ensureCurrentEntry(uint32_t utc_day) {
    // Jeśli mamy wpis dla tego samego dnia - OK
    if (current_entry_.utc_day == utc_day) {
        return DailyLogResult::OK;
    }
    
    // Jeśli mamy wpis dla innego dnia - najpierw sfinalizuj
    if (current_entry_.utc_day != 0 && current_entry_dirty_) {
        auto result = finalizeDay();
        if (result != DailyLogResult::OK) {
            Serial.println(F("[DailyLog] Warning: Failed to finalize previous day"));
        }
    }
    
    // Sprawdź czy ten dzień już istnieje w buforze (np. po restarcie)
    DayLogEntry existing;
    if (header_.count > 0 && header_.last_day_utc == utc_day) {
        // Wczytaj ostatni wpis
        if (loadEntry(header_.head_index, existing) == DailyLogResult::OK) {
            if (!existing.isFinalized()) {
                // Wpis istnieje i nie jest sfinalizowany - wznów
                current_entry_ = existing;
                current_entry_dirty_ = false;
                Serial.printf("[DailyLog] Resumed existing entry for day %lu\n", utc_day);
                return DailyLogResult::OK;
            }
        }
    }
    
    // Tworzymy nowy wpis
    return initializeNewDay(utcDayToTimestamp(utc_day));
}

DailyLogResult DailyLogManager::commitCurrentEntry() {

    Serial.printf("[DailyLog] commitCurrentEntry() - dirty=%d, utc_day=%lu\n", 
                  current_entry_dirty_, current_entry_.utc_day);
    
    if (!current_entry_dirty_) {
        Serial.println("[DailyLog] SKIP - entry not dirty!");
        return DailyLogResult::OK;
    }

    if (!current_entry_dirty_) return DailyLogResult::OK;
    
    uint8_t target_index;
    bool is_new_entry = false;
    
    // Sprawdź czy to nowy wpis czy aktualizacja istniejącego
    if (header_.count == 0 || header_.last_day_utc != current_entry_.utc_day) {
        // Nowy wpis
        is_new_entry = true;
        
        if (header_.count >= header_.capacity) {
            // Buffer pełny - nadpisujemy najstarszy
            target_index = header_.tail_index;
            header_.tail_index = nextIndex(header_.tail_index);
            // first_day_utc zaktualizujemy po zapisie
        } else {
            // Jest miejsce
            target_index = header_.count == 0 ? 0 : nextIndex(header_.head_index);
            header_.count++;
        }
        
        header_.head_index = target_index;
        header_.last_day_utc = current_entry_.utc_day;
        header_.total_entries_written++;
        
        if (header_.first_day_utc == 0) {
            header_.first_day_utc = current_entry_.utc_day;
        }
        
    } else {
        // Aktualizacja istniejącego wpisu (head)
        target_index = header_.head_index;
    }
    
    // Zapisz wpis
    auto result = saveEntry(target_index, current_entry_);
    if (result != DailyLogResult::OK) return result;
    
    // Zapisz nagłówek jeśli nowy wpis
    if (is_new_entry) {
        // Aktualizuj first_day_utc jeśli nadpisaliśmy najstarszy
        if (header_.count == header_.capacity) {
            DayLogEntry oldest;
            if (loadEntry(header_.tail_index, oldest) == DailyLogResult::OK) {
                header_.first_day_utc = oldest.utc_day;
            }
        }
        
        result = saveHeader();
        if (result != DailyLogResult::OK) return result;
    }
    
    current_entry_dirty_ = false;



       Serial.printf("[DailyLog] Committed to FRAM, writes=%d\n", current_entry_.fram_writes);
    return DailyLogResult::OK;
    // return DailyLogResult::OK;
}

void DailyLogManager::initEmptyEntry(DayLogEntry& entry, uint32_t utc_day, uint8_t day_of_week) {
    entry = DayLogEntry();  // Reset do domyślnych wartości
    entry.utc_day = utc_day;
    entry.day_of_week = day_of_week;
    entry.version = DAILY_LOG_VERSION_CURRENT;
    entry.channel_count = CHANNEL_COUNT;
}

// ============================================================================
// CRC32
// ============================================================================

uint32_t DailyLogManager::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// Globalna inicjalizacja
// ============================================================================

bool dailyLogInit() {
    if (g_dailyLog != nullptr) {
        delete g_dailyLog;
    }
    
    g_dailyLog = new DailyLogManager();
    
    if (g_dailyLog->init() != DailyLogResult::OK) {
        Serial.println(F("[DailyLog] Initialization failed!"));
        return false;
    }
    
    return true;
}
