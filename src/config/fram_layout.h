/**
 * DOZOWNIK - FRAM Memory Layout
 * 
 * Mapa adresów pamięci FRAM MB85RC256V (32 kB).
 * Wszystkie struktury wyrównane do 16 bajtów dla łatwiejszego debugowania.
 */

#ifndef FRAM_LAYOUT_H
#define FRAM_LAYOUT_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"

// ============================================================================
// FRAM CHIP PARAMETERS
// ============================================================================
#define FRAM_SIZE_BYTES         32768   // 32 kB
#define FRAM_PAGE_SIZE          16      // Alignment boundary

// ============================================================================
// MAGIC NUMBERS & VERSION
// ============================================================================
#define FRAM_MAGIC_NUMBER       0x444F5A41  // "DOZA" in ASCII
#define FRAM_LAYOUT_VERSION     1

// ============================================================================
// MEMORY MAP
// ============================================================================
// Sekcja          | Adres      | Rozmiar   | Opis
// ----------------|------------|-----------|----------------------------------
// HEADER          | 0x0000     | 32 B      | Magic, version, checksum
// CREDENTIALS     | 0x0020     | 480 B     | Encrypted WiFi/VPS credentials
// SYSTEM_STATE    | 0x0200     | 32 B      | Globalny stan systemu
// ACTIVE_CONFIG   | 0x0220     | 192 B     | Aktywna konfiguracja (6 × 32 B)
// PENDING_CONFIG  | 0x02E0     | 192 B     | Oczekująca konfiguracja (6 × 32 B)
// DAILY_STATE     | 0x03A0     | 96 B      | Stan dzienny (6 × 16 B)
// ERROR_STATE     | 0x0400     | 16 B      | Stan błędu krytycznego
// AUTH_DATA       | 0x0410     | 64 B      | Hash hasła admin + salt
// SESSION_DATA    | 0x0450     | 128 B     | Dane sesji
// VPS_LOG_BUFFER  | 0x04D0     | 256 B     | Bufor logu VPS (retry)
// RESERVED        | 0x05D0     | ...       | Zarezerwowane na przyszłość
// ============================================================================

// ----------------------------------------------------------------------------
// HEADER SECTION (0x0000 - 0x001F)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_HEADER            0x0000
#define FRAM_SIZE_HEADER            32

#pragma pack(push, 1)

struct FramHeader {
    uint32_t magic;             // FRAM_MAGIC_NUMBER
    uint16_t layout_version;    // FRAM_LAYOUT_VERSION
    uint16_t channel_count;     // Aktywna liczba kanałów
    uint32_t init_timestamp;    // Timestamp pierwszej inicjalizacji
    uint32_t last_write;        // Timestamp ostatniego zapisu
    uint8_t  flags;             // Flagi systemowe
    uint8_t  _reserved[11];     // Padding
    uint32_t header_crc;        // CRC32 headera
};

#pragma pack(pop)

static_assert(sizeof(FramHeader) == FRAM_SIZE_HEADER, "FramHeader size mismatch");

// ----------------------------------------------------------------------------
// CREDENTIALS SECTION (0x0020 - 0x01FF)
// Kompatybilne z bazowym projektem dolewki
// ----------------------------------------------------------------------------
#define FRAM_ADDR_CREDENTIALS       0x0020
#define FRAM_SIZE_CREDENTIALS       480

// Struktura credentials zdefiniowana w credentials_manager.h (z bazowego projektu)

// ----------------------------------------------------------------------------
// SYSTEM STATE (0x0200 - 0x021F)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_SYSTEM_STATE      0x0200
#define FRAM_SIZE_SYSTEM_STATE      32

// Używa struct SystemState z dosing_types.h

// ----------------------------------------------------------------------------
// ACTIVE CONFIG (0x0220 - 0x02DF)
// Obecnie działająca konfiguracja kanałów
// ----------------------------------------------------------------------------
#define FRAM_ADDR_ACTIVE_CONFIG     0x0220
#define FRAM_SIZE_ACTIVE_CONFIG     (CHANNEL_COUNT * sizeof(ChannelConfig))

// Makro do obliczenia adresu kanału
#define FRAM_ADDR_ACTIVE_CH(n)      (FRAM_ADDR_ACTIVE_CONFIG + ((n) * sizeof(ChannelConfig)))

// ----------------------------------------------------------------------------
// PENDING CONFIG (0x02E0 - 0x039F)
// Konfiguracja oczekująca na aktywację (od następnej doby)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_PENDING_CONFIG    0x02E0
#define FRAM_SIZE_PENDING_CONFIG    (CHANNEL_COUNT * sizeof(ChannelConfig))

#define FRAM_ADDR_PENDING_CH(n)     (FRAM_ADDR_PENDING_CONFIG + ((n) * sizeof(ChannelConfig)))

// ----------------------------------------------------------------------------
// DAILY STATE (0x03A0 - 0x03FF)
// Stan dzienny kanałów - resetowany o północy
// ----------------------------------------------------------------------------
#define FRAM_ADDR_DAILY_STATE       0x03A0
#define FRAM_SIZE_DAILY_STATE       (CHANNEL_COUNT * sizeof(ChannelDailyState))

#define FRAM_ADDR_DAILY_CH(n)       (FRAM_ADDR_DAILY_STATE + ((n) * sizeof(ChannelDailyState)))

// ----------------------------------------------------------------------------
// ERROR STATE (0x0400 - 0x040F)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_ERROR_STATE       0x0400
#define FRAM_SIZE_ERROR_STATE       16

// Używa struct ErrorState z dosing_types.h

// ----------------------------------------------------------------------------
// AUTH DATA (0x0410 - 0x044F)
// Hash hasła administratora + salt
// ----------------------------------------------------------------------------
#define FRAM_ADDR_AUTH_DATA         0x0410
#define FRAM_SIZE_AUTH_DATA         64

#pragma pack(push, 1)

struct AuthData {
    uint8_t  password_hash[32];     // SHA-256 hash
    uint8_t  salt[16];              // Random salt
    uint8_t  hash_iterations;       // Liczba iteracji PBKDF2
    uint8_t  password_set;          // Czy hasło zostało ustawione (0/1)
    uint8_t  _reserved[10];         // Padding
    uint32_t crc32;                 // CRC32
};

#pragma pack(pop)

static_assert(sizeof(AuthData) == FRAM_SIZE_AUTH_DATA, "AuthData size mismatch");

// ----------------------------------------------------------------------------
// SESSION DATA (0x0450 - 0x04CF)
// Dane sesji (persistence między restartami)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_SESSION_DATA      0x0450
#define FRAM_SIZE_SESSION_DATA      128

// Struktura sesji zdefiniowana w session_manager.h

// ----------------------------------------------------------------------------
// VPS LOG BUFFER (0x04D0 - 0x05CF)
// Bufor na nieudane logi VPS (do ponowienia)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_VPS_LOG_BUFFER    0x04D0
#define FRAM_SIZE_VPS_LOG_BUFFER    256

#pragma pack(push, 1)

struct VpsLogBuffer {
    uint8_t  pending_logs;          // Liczba oczekujących logów (0-3)
    uint8_t  _reserved[3];
    
    // Maksymalnie 3 zaległe logi
    struct {
        uint32_t utc_day;           // Dzień logu
        uint8_t  payload[72];       // Skompresowany payload
        uint8_t  retry_count;       // Liczba prób
        uint8_t  _pad[3];
    } entries[3];
    
    uint32_t crc32;
};

#pragma pack(pop)

static_assert(sizeof(VpsLogBuffer) <= FRAM_SIZE_VPS_LOG_BUFFER, "VpsLogBuffer too large");

// ----------------------------------------------------------------------------
// RESERVED SPACE (0x05D0 - 0x7FFF)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_RESERVED          0x05D0
#define FRAM_SIZE_RESERVED          (FRAM_SIZE_BYTES - FRAM_ADDR_RESERVED)

// ============================================================================
// FRAM OPERATIONS (deklaracje)
// ============================================================================

/**
 * Inicjalizacja FRAM - sprawdza magic number, inicjalizuje jeśli pusta
 * @return true jeśli FRAM gotowa do użycia
 */
bool framInit();

/**
 * Sprawdź czy FRAM jest zainicjalizowana
 */
bool framIsInitialized();

/**
 * Wyczyść całą FRAM i zainicjalizuj od nowa
 */
bool framFactoryReset();

/**
 * Odczytaj header FRAM
 */
bool framReadHeader(FramHeader* header);

/**
 * Zapisz header FRAM
 */
bool framWriteHeader(const FramHeader* header);

// --- Channel Config ---

/**
 * Odczytaj aktywną konfigurację kanału
 */
bool framReadActiveConfig(uint8_t channel, ChannelConfig* config);

/**
 * Zapisz aktywną konfigurację kanału
 */
bool framWriteActiveConfig(uint8_t channel, const ChannelConfig* config);

/**
 * Odczytaj pending konfigurację kanału
 */
bool framReadPendingConfig(uint8_t channel, ChannelConfig* config);

/**
 * Zapisz pending konfigurację kanału
 */
bool framWritePendingConfig(uint8_t channel, const ChannelConfig* config);

/**
 * Skopiuj pending do active (podczas resetu dobowego)
 */
bool framApplyPendingConfig(uint8_t channel);

/**
 * Skopiuj active do pending (cancel changes)
 */
bool framRevertPendingConfig(uint8_t channel);

// --- Daily State ---

/**
 * Odczytaj stan dzienny kanału
 */
bool framReadDailyState(uint8_t channel, ChannelDailyState* state);

/**
 * Zapisz stan dzienny kanału
 */
bool framWriteDailyState(uint8_t channel, const ChannelDailyState* state);

/**
 * Resetuj stan dzienny wszystkich kanałów
 */
bool framResetAllDailyStates(uint8_t currentDay);

// --- System State ---

/**
 * Odczytaj stan systemu
 */
bool framReadSystemState(SystemState* state);

/**
 * Zapisz stan systemu
 */
bool framWriteSystemState(const SystemState* state);

// --- Error State ---

/**
 * Odczytaj stan błędu
 */
bool framReadErrorState(ErrorState* state);

/**
 * Zapisz stan błędu
 */
bool framWriteErrorState(const ErrorState* state);

/**
 * Wyczyść stan błędu
 */
bool framClearErrorState();

// --- Low-level ---

/**
 * Odczytaj surowe bajty z FRAM
 */
bool framReadBytes(uint16_t address, void* buffer, size_t length);

/**
 * Zapisz surowe bajty do FRAM
 */
bool framWriteBytes(uint16_t address, const void* data, size_t length);

/**
 * Sprawdź obecność FRAM na I2C
 */
bool framProbe();

/**
 * Dump sekcji FRAM do Serial (debug)
 */
void framDumpSection(uint16_t address, size_t length);

#endif // FRAM_LAYOUT_H