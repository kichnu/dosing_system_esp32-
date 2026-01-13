/**
 * DOZOWNIK - FRAM Memory Layout
 * 
 * Mapa adresów pamięci FRAM MB85RC256V (32 kB).
 * Wszystkie struktury wyrównane do 16 bajtów dla łatwiejszego debugowania.
 * 
 * UWAGA: Sekcja CREDENTIALS kompatybilna z projektem DOLEWKA (1024 B)
 */

#ifndef FRAM_LAYOUT_H
#define FRAM_LAYOUT_H

#include <Arduino.h>
#include "config.h"
#include "dosing_types.h"

// ============================================================================
// FRAM CHIP PARAMETERS
// ============================================================================
#define FRAM_SIZE_BYTES         32768   // 32 kB (0x8000)
#define FRAM_PAGE_SIZE          16      // Alignment boundary

// ============================================================================
// MAGIC NUMBERS & VERSION
// ============================================================================
#define FRAM_MAGIC_NUMBER       0x444F5A41  // "DOZA" in ASCII
#define FRAM_LAYOUT_VERSION     6           // v6: Corrected memory map

// ============================================================================
// FRAM MEMORY LAYOUT v6 - CORRECTED
// MB85RC256V: 32KB (32,768 bytes = 0x8000)
// ============================================================================
// Section             | Address    | Size      | Description
// --------------------|------------|-----------|--------------------------------
// HEADER              | 0x0000     | 32 B      | Magic, version, checksum
// CREDENTIALS         | 0x0020     | 1024 B    | Encrypted WiFi credentials
// SYSTEM_STATE        | 0x0420     | 32 B      | Global system state
// ACTIVE_CONFIG       | 0x0440     | 192 B     | Active config (6 × 32B max)
// PENDING_CONFIG      | 0x0500     | 192 B     | Pending config (6 × 32B max)
// DAILY_STATE         | 0x05C0     | 144 B     | Daily state (6 × 24B max)
// CRITICAL_ERROR      | 0x0650     | 32 B      | Critical error state
// AUTH_DATA           | 0x0670     | 64 B      | Admin password hash
// SESSION_DATA        | 0x06B0     | 128 B     | Session data
// CONTAINER_VOLUME    | 0x0730     | 48 B      | Container volumes (6 × 8B)
// (free)              | 0x0760     | 160 B     | Reserved for future use
// DAILY_LOG_HEADER_A  | 0x0800     | 32 B      | Ring buffer header (primary)
// DAILY_LOG_HEADER_B  | 0x0820     | 32 B      | Ring buffer header (backup)
// DAILY_LOG_ENTRIES   | 0x0840     | 19,200 B  | Ring buffer (100 × 192B)
// (end of daily log)  | 0x5340     |           |
// RESERVED            | 0x5340     | 11,200 B  | Future expansion (~11KB free)
// (end of FRAM)       | 0x8000     |           |
// ============================================================================

// ----------------------------------------------------------------------------
// HEADER (0x0000 - 0x001F)
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
// CREDENTIALS SECTION (0x0020 - 0x041F)
// Kompatybilne z projektem DOLEWKA - NIE ZMIENIAĆ ROZMIARU!
// ----------------------------------------------------------------------------
#define FRAM_ADDR_CREDENTIALS       0x0020
#define FRAM_SIZE_CREDENTIALS       1024

// Struktura credentials zdefiniowana w fram_encryption.h

// ----------------------------------------------------------------------------
// SYSTEM STATE (0x0420 - 0x043F)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_SYSTEM_STATE      0x0420
#define FRAM_SIZE_SYSTEM_STATE      32

// Używa struct SystemState z dosing_types.h

// ----------------------------------------------------------------------------
// ACTIVE CONFIG (0x0440 - 0x04FF)
// Obecnie działająca konfiguracja kanałów
// ----------------------------------------------------------------------------
#define FRAM_ADDR_ACTIVE_CONFIG     0x0440
#define FRAM_SIZE_ACTIVE_CONFIG     (CHANNEL_COUNT * sizeof(ChannelConfig))

// Makro do obliczenia adresu kanału
#define FRAM_ADDR_ACTIVE_CH(n)      (FRAM_ADDR_ACTIVE_CONFIG + ((n) * sizeof(ChannelConfig)))

// ----------------------------------------------------------------------------
// PENDING CONFIG (0x0500 - 0x05BF)
// Konfiguracja oczekująca na aktywację (od następnej doby)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_PENDING_CONFIG    0x0500
#define FRAM_SIZE_PENDING_CONFIG    (CHANNEL_COUNT * sizeof(ChannelConfig))

#define FRAM_ADDR_PENDING_CH(n)     (FRAM_ADDR_PENDING_CONFIG + ((n) * sizeof(ChannelConfig)))

// ----------------------------------------------------------------------------
// DAILY STATE (0x05C0 - 0x064F)
// Stan dzienny kanałów - resetowany o północy
// ----------------------------------------------------------------------------
#define FRAM_ADDR_DAILY_STATE       0x05C0
#define FRAM_SIZE_DAILY_STATE_MAX   (6 * sizeof(ChannelDailyState))  // Zawsze rezerwuj na 6 kanałów
#define FRAM_SIZE_DAILY_STATE       (CHANNEL_COUNT * sizeof(ChannelDailyState))

#define FRAM_ADDR_DAILY_CH(n)       (FRAM_ADDR_DAILY_STATE + ((n) * sizeof(ChannelDailyState)))

// ----------------------------------------------------------------------------
// CRITICAL ERROR STATE (0x0650 - 0x066F)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_CRITICAL_ERROR    0x0650
#define FRAM_SIZE_CRITICAL_ERROR    32

// Używa struct CriticalErrorState z dosing_types.h (32 bajty)

// ----------------------------------------------------------------------------
// AUTH DATA (0x0670 - 0x06AF)
// Hash hasła administratora + salt
// ----------------------------------------------------------------------------
#define FRAM_ADDR_AUTH_DATA         0x0670
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
// SESSION DATA (0x06B0 - 0x072F)
// Dane sesji (persistence między restartami)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_SESSION_DATA      0x06B0
#define FRAM_SIZE_SESSION_DATA      128

// ----------------------------------------------------------------------------
// CONTAINER VOLUME (0x0730 - 0x075F)
// Pojemność i pozostała ilość płynu w pojemnikach (6 kanałów × 8B)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_CONTAINER_VOLUME      0x0730
#define FRAM_SIZE_CONTAINER_VOLUME      48      // 6 kanałów × 8B

#define FRAM_ADDR_CONTAINER_CH(n)       (FRAM_ADDR_CONTAINER_VOLUME + ((n) * 8))

// ----------------------------------------------------------------------------
// FREE SPACE (0x0760 - 0x07FF)
// Zarezerwowane na przyszłość (160B)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_FREE_SPACE            0x0760
#define FRAM_SIZE_FREE_SPACE            160

// ----------------------------------------------------------------------------
// DAILY LOG RING BUFFER (0x0800 - 0x533F)
// Lokalny zapis dziennych logów (zastępuje VPS logging)
// ----------------------------------------------------------------------------
#define FRAM_ADDR_DAILY_LOG_HEADER_A    0x0800
#define FRAM_ADDR_DAILY_LOG_HEADER_B    0x0820
#define FRAM_ADDR_DAILY_LOG_ENTRIES     0x0840

#define FRAM_SIZE_DAILY_LOG_HEADER      32
#define FRAM_SIZE_DAILY_LOG_ENTRY       192
#define FRAM_DAILY_LOG_CAPACITY         100     // 100 dni historii (~3.3 miesiąca)
#define FRAM_SIZE_DAILY_LOG_ENTRIES     (FRAM_SIZE_DAILY_LOG_ENTRY * FRAM_DAILY_LOG_CAPACITY)

#define FRAM_MAGIC_DAILY_LOG            0x444C4F47  // "DLOG"
#define DAILY_LOG_VERSION_CURRENT       1

// Makro do obliczania adresu wpisu
#define FRAM_DAILY_LOG_ENTRY_ADDR(index) \
    (FRAM_ADDR_DAILY_LOG_ENTRIES + ((index) * FRAM_SIZE_DAILY_LOG_ENTRY))

// Koniec Daily Log
#define FRAM_ADDR_DAILY_LOG_END         (FRAM_ADDR_DAILY_LOG_ENTRIES + FRAM_SIZE_DAILY_LOG_ENTRIES)
// = 0x0840 + 19200 = 0x5340

// ----------------------------------------------------------------------------
// RESERVED (0x5340 - 0x7FFF)
// Wolna przestrzeń (~11KB) na przyszłe rozszerzenia
// ----------------------------------------------------------------------------
#define FRAM_ADDR_RESERVED              0x5340
#define FRAM_SIZE_RESERVED              (FRAM_SIZE_BYTES - FRAM_ADDR_RESERVED)  // 11,200B

// ============================================================================
// COMPILE-TIME VALIDATION
// ============================================================================

// Weryfikacja że nie przekraczamy FRAM
static_assert(FRAM_ADDR_DAILY_LOG_END <= FRAM_SIZE_BYTES, 
              "Daily Log exceeds FRAM size!");
static_assert(FRAM_ADDR_RESERVED + FRAM_SIZE_RESERVED == FRAM_SIZE_BYTES,
              "Reserved section calculation error!");

// Weryfikacja alignmentu Daily Log
static_assert((FRAM_ADDR_DAILY_LOG_ENTRIES % FRAM_SIZE_DAILY_LOG_ENTRY) == 0,
              "Daily Log entries must be aligned to entry size!");

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