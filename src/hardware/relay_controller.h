        /**
         * DOZOWNIK - Relay Controller
         * 
         * Bezpieczne sterowanie przekaźnikami pomp.
         * Gwarantuje że tylko jedna pompa pracuje w danym momencie (mutex).
         */

        #ifndef RELAY_CONTROLLER_H
        #define RELAY_CONTROLLER_H

        #include <Arduino.h>
        #include "config.h"
        #include "dosing_types.h"

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
            ERROR_TIMEOUT
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
            * Aktualizacja stanu - sprawdza timeout, wywołuj w loop()
            */
            void update();
            
            // --- Sterowanie pojedynczym kanałem ---
            
            /**
            * Włącz przekaźnik kanału
            * @param channel Numer kanału (0-5)
            * @param max_duration_ms Maksymalny czas pracy (0 = bez limitu, używa MAX_PUMP_DURATION_MS)
            * @return Wynik operacji
            */
            RelayResult turnOn(uint8_t channel, uint32_t max_duration_ms = 0);
            
            /**
            * Wyłącz przekaźnik kanału
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
            
            // --- Status ---
            
            /**
            * Czy jakikolwiek przekaźnik jest włączony
            */
            bool isAnyOn() const;
            
            /**
            * Który kanał jest aktywny (255 = żaden)
            */
            uint8_t getActiveChannel() const;
            
            /**
            * Czy konkretny kanał jest włączony
            */
            bool isChannelOn(uint8_t channel) const;
            
            /**
            * Pobierz czas pracy aktywnego kanału (ms)
            */
            uint32_t getActiveRuntime() const;
            
            /**
            * Pobierz pozostały czas do timeout (ms)
            */
            uint32_t getRemainingTime() const;
            
            /**
            * Pobierz stan kanału
            */
            const RelayState& getChannelState(uint8_t channel) const;
            
            /**
            * Pobierz łączny czas pracy wszystkich pomp (ms)
            */
            uint32_t getTotalRuntime() const;
            
            // --- Debug ---
            
            /**
            * Wypisz status do Serial
            */
            void printStatus() const;
            
            /**
            * Konwersja RelayResult na string
            */
            static const char* resultToString(RelayResult result);

        private:
            RelayState _channels[CHANNEL_COUNT];
            
            uint8_t  _activeChannel;        // Aktywny kanał (255 = żaden)
            uint32_t _activeMaxDuration;    // Max czas dla aktywnego kanału
            bool     _initialized;
            
            /**
            * Wewnętrzne włączenie GPIO (bez walidacji)
            */
            void _setRelay(uint8_t channel, bool state);
            
            /**
            * Sprawdź i obsłuż timeout
            */
            void _checkTimeout();
        };

        // ============================================================================
        // GLOBAL INSTANCE
        // ============================================================================

        extern RelayController relayController;

        #endif // RELAY_CONTROLLER_H
