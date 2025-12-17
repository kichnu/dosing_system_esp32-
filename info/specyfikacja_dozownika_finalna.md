# DOZOWNIK - SPECYFIKACJA SYSTEMU
## Dokument referencyjny v1.0

> **Projekt bazowy:** System dolewki wody (DOLEWKA)  
> **Metoda rozwoju:** Nadpisywanie modułów bazowego projektu  
> **Data utworzenia:** Grudzień 2024

---

## 1. INFORMACJE OGÓLNE

| Parametr | Wartość |
|----------|---------|
| Nazwa urządzenia | DOZOWNIK |
| Device ID | `DOZOWNIK` |
| Przeznaczenie | Automatyczne dozowanie nawozów do akwarium |
| Liczba kanałów | 1-6 (hardcoded w `#define CHANNEL_COUNT`) |
| Platforma MCU | ESP32-S3-ZERO (Seeed Studio XIAO) |
| Pamięć nieulotna | FRAM MB85RC256V (32 kB, I2C @ 0x50) |
| Zegar RTC | DS3231M (I2C @ 0x68, UTC) |
| Projekt bazowy | System dolewki wody - współdzielone moduły |

---

## 2. PINOUT SPRZĘTOWY

| Funkcja | GPIO | Uwagi |
|---------|------|-------|
| RELAY CH0 | GPIO4 | Wyjście, aktywne HIGH |
| RELAY CH1 | GPIO5 | Wyjście, aktywne HIGH |
| RELAY CH2 | GPIO6 | Wyjście, aktywne HIGH |
| RELAY CH3 | GPIO7 | Wyjście, aktywne HIGH |
| RELAY CH4 | GPIO8 | Wyjście, aktywne HIGH |
| RELAY CH5 | GPIO9 | Wyjście, aktywne HIGH |
| VALIDATE CH0 | GPIO13 | Wejście, walidacja pracy pompy |
| VALIDATE CH1 | GPIO14 | Wejście, walidacja pracy pompy |
| VALIDATE CH2 | GPIO15 | Wejście, walidacja pracy pompy |
| VALIDATE CH3 | GPIO16 | Wejście, walidacja pracy pompy |
| VALIDATE CH4 | GPIO17 | Wejście, walidacja pracy pompy |
| VALIDATE CH5 | GPIO18 | Wejście, walidacja pracy pompy |
| I2C SDA | GPIO11 | FRAM + RTC |
| I2C SCL | GPIO12 | FRAM + RTC |

---

## 3. STAŁE KONFIGURACYJNE

| Stała | Wartość | Opis |
|-------|---------|------|
| `CHANNEL_COUNT` | 6 | Maksymalna liczba kanałów |
| `EVENTS_PER_DAY` | 23 | Godziny 01:00-23:00 |
| `FIRST_EVENT_HOUR` | 1 | Pierwsza godzina eventów (00:00 zarezerwowana) |
| `CHANNEL_OFFSET_MINUTES` | 10 | Offset czasowy między kanałami |
| `EVENT_WINDOW_SECONDS` | 300 | Okno czasowe eventu (5 min) |
| `MAX_PUMP_DURATION_SECONDS` | 180 | Maksymalny czas pracy pompy (3 min) |
| `MIN_SINGLE_DOSE_ML` | 1.0 | Minimalna pojedyncza dawka |
| `CALIBRATION_DURATION_SECONDS` | 30 | Czas kalibracji pompy |
| `GPIO_VALIDATION_ENABLED` | true/false | Globalne włączenie walidacji GPIO |
| `GPIO_CHECK_DELAY_MS` | 2000 | Opóźnienie przed sprawdzeniem GPIO |
| `GPIO_DEBOUNCE_MS` | 1000 | Czas debounce dla GPIO |

---

## 4. HARMONOGRAM CZASOWY

### 4.1 Struktura czasowa doby

| Godzina | Przeznaczenie |
|---------|---------------|
| 00:00 - 00:59 | ZAREZERWOWANE: reset dobowy + logowanie VPS |
| 01:00 - 23:59 | Okna czasowe eventów (23 sloty) |

### 4.2 Offsety kanałów

| Kanał | Offset | Przykładowe czasy |
|-------|--------|-------------------|
| CH0 | :00 | 01:00, 02:00, ..., 23:00 |
| CH1 | :10 | 01:10, 02:10, ..., 23:10 |
| CH2 | :20 | 01:20, 02:20, ..., 23:20 |
| CH3 | :30 | 01:30, 02:30, ..., 23:30 |
| CH4 | :40 | 01:40, 02:40, ..., 23:40 |
| CH5 | :50 | 01:50, 02:50, ..., 23:50 |

### 4.3 Okno czasowe eventu

| Element | Wartość |
|---------|---------|
| Długość okna | 300 sekund (5 minut) |
| Max czas pompy | 180 sekund (3 minuty) |
| Bufor bezpieczeństwa | 120 sekund (2 minuty) |

---

## 5. PARAMETRY KANAŁU

### 5.1 Dane wejściowe (wprowadzane przez użytkownika)

| Parametr | Typ | Zakres | Opis |
|----------|-----|--------|------|
| `events_bitmask` | uint32_t | bit 1-23 | Zaznaczone godziny eventów |
| `days_bitmask` | uint8_t | bit 0-6 | Aktywne dni tygodnia (0=Pon, 6=Ndz) |
| `daily_dose_ml` | float | >0 | Dzienna dawka w ml |
| `dosing_rate_ml_s` | float | >0 | Wydajność pompy z kalibracji (ml/s) |

### 5.2 Dane obliczane automatycznie

| Parametr | Formuła | Opis |
|----------|---------|------|
| `active_events` | `popcount(events_bitmask)` | Liczba zaznaczonych eventów |
| `active_days` | `popcount(days_bitmask)` | Liczba aktywnych dni |
| `single_dose_ml` | `daily_dose / active_events` | Pojedyncza dawka |
| `pump_duration_ms` | `(single_dose / dosing_rate) × 1000` | Czas pracy pompy |
| `weekly_dose_ml` | `daily_dose × active_days` | Tygodniowa suma (wyświetlana) |

### 5.3 Stan dzienny (resetowany o północy)

| Parametr | Typ | Opis |
|----------|-----|------|
| `events_completed` | uint32_t (bitmask) | Flagi wykonanych eventów |
| `today_added_ml` | float | Suma dozowana dziś |

---

## 6. WALIDACJA KONFIGURACJI

| Warunek | Wynik | Stan karty |
|---------|-------|------------|
| `events_bitmask == 0` | Kanał nieaktywny | INACTIVE |
| `days_bitmask == 0` | Brak dni | INCOMPLETE |
| `daily_dose <= 0` | Brak dawki | INCOMPLETE |
| `single_dose < MIN_SINGLE_DOSE_ML` | Dawka za mała | INVALID |
| `pump_duration > MAX_PUMP_DURATION` | Czas za długi | INVALID |
| Wszystko OK | Konfiguracja poprawna | CONFIGURED |
| Oczekujące zmiany | Aktywne od jutra | PENDING |

---

## 7. ALGORYTM WYKONANIA EVENTU

### 7.1 Pętla główna (co 10 sekund)

| Krok | Działanie |
|------|-----------|
| 1 | Sprawdź aktualny czas UTC |
| 2 | Czy jesteśmy w oknie czasowym eventu? |
| 3 | Czy dzień tygodnia jest aktywny dla kanału? |
| 4 | Czy flaga eventu nie jest ustawiona? |
| 5 | Czy `today_added < daily_dose`? |
| 6 | Wykonaj dozowanie |

### 7.2 Sekwencja dozowania

| Krok | Działanie | Uwagi |
|------|-----------|-------|
| 1 | Sprawdź mutex (żadna pompa nie działa) | - |
| 2 | Włącz przekaźnik pompy | GPIO HIGH |
| 3 | Odczekaj `GPIO_CHECK_DELAY_MS` (2s) | - |
| 4 | Sprawdź GPIO walidacji (jeśli włączone) | Debounce 1s |
| 5a | GPIO OK → kontynuuj pompowanie | - |
| 5b | GPIO FAIL → BŁĄD KRYTYCZNY | Zatrzymaj system |
| 6 | Odczekaj pozostały czas pompy | - |
| 7 | Wyłącz przekaźnik | GPIO LOW |
| 8 | Ustaw flagę eventu | - |
| 9 | Dodaj `single_dose` do `today_added` | - |

### 7.3 Walidacja GPIO

| Parametr | Wartość |
|----------|---------|
| Opóźnienie przed sprawdzeniem | 2000 ms |
| Czas debounce | 1000 ms |
| Liczba prób | 1 (bez powtórzeń!) |
| Przy niepowodzeniu | BŁĄD KRYTYCZNY |

---

## 8. BŁĄD KRYTYCZNY

| Aspekt | Działanie |
|--------|-----------|
| Wyzwalacz | Niepowodzenie walidacji GPIO |
| Reakcja natychmiastowa | Zatrzymanie WSZYSTKICH pomp |
| Zapis do FRAM | Typ błędu, kanał, timestamp |
| Stan systemu | `system_halted = true` |
| Wymagane działanie | Ręczna interwencja użytkownika |
| Przyszłe rozszerzenie | Powiadomienie VPS, alarm |

---

## 9. RESET DOBOWY (00:00-00:59 UTC)

| Krok | Działanie |
|------|-----------|
| 1 | Aplikuj `pending_config` → `active_config` dla każdego kanału |
| 2 | Wyzeruj `events_completed` dla każdego kanału |
| 3 | Wyzeruj `today_added_ml` dla każdego kanału |
| 4 | Wyślij log dzienny do VPS |

---

## 10. MECHANIZM PENDING CHANGES

| Aspekt | Opis |
|--------|------|
| Cel | Uniknięcie edge case'ów przy zmianach w trakcie dnia |
| Działanie | Zmiany zapisywane do `pending_config` |
| Aktywacja | O północy UTC (podczas resetu dobowego) |
| Informacja dla użytkownika | Banner "Changes pending – active from tomorrow" |
| Przechowywanie | Dwa zestawy w FRAM: `active_config` + `pending_config` |

---

## 11. LOGOWANIE VPS

### 11.1 Parametry

| Parametr | Wartość |
|----------|---------|
| Okno czasowe | 00:00 - 00:59 UTC |
| Częstotliwość | Raz na dobę |
| Typ logu | `DAILY_SUMMARY` |

### 11.2 Zawartość payloadu

| Pole | Opis |
|------|------|
| `device_id` | "DOZOWNIK" |
| `log_type` | "DAILY_SUMMARY" |
| `utc_date` | Data w formacie YYYY-MM-DD |
| `channels[]` | Tablica z danymi każdego kanału |
| `.configured_events` | Liczba skonfigurowanych eventów |
| `.configured_dose_ml` | Skonfigurowana dawka pojedyncza |
| `.completed_events` | Liczba wykonanych eventów |
| `.actual_dose_ml` | Rzeczywista dawka pojedyncza |
| `.status` | OK / PARTIAL / ERROR |

### 11.3 Informacja dla użytkownika (checklist)

> Log ma pokazywać: "ustawione X × Y ml, wykonane Z × W ml"  
> Umożliwia weryfikację czy system działał poprawnie.

---

## 12. INTERFEJS GUI

### 12.1 Stany karty kanału

| Stan | Kolor ramki | Badge | Opis |
|------|-------------|-------|------|
| INACTIVE | Szary | "Inactive" | Żaden event nie zaznaczony |
| INCOMPLETE | Żółty | "Setup" | Brak dni lub dawki |
| INVALID | Czerwony | "Invalid" | Walidacja nie przeszła |
| CONFIGURED | Zielony | "Active" | Gotowy do pracy |
| PENDING | Niebieski | "Pending" | Zmiany od jutra |

### 12.2 Sekcje karty kanału

| Sekcja | Zawartość |
|--------|-----------|
| Header | Numer kanału, badge stanu |
| Time Schedule | 23 checkboxy (01:00-23:00), stany: selected/completed/next |
| Active Days | 7 checkboxów (Pon-Ndz), indicator "today" |
| Dosing Volume | Input daily_dose + obliczone: single, pump time, weekly, today X/Y |
| Calibration | Button "Run Pump 30s" + input "Measured ml" |
| Validation | Komunikat walidacji (info/warn/error/ok) |
| Footer | Cancel + Save |

### 12.3 Layout

| Tryb | Opis |
|------|------|
| Mobile | Swipe góra/dół (nav↔channels), lewo/prawo (między kanałami) |
| Desktop | Sidebar (280-320px) + grid kanałów (2×3 lub 3×2) |

### 12.4 Czas w GUI

| Kontekst | Format |
|----------|--------|
| Wewnętrznie (system) | UTC |
| Wyświetlanie użytkownikowi | CET/CEST (konwersja w JavaScript) |

---

## 13. API ENDPOINTS

| Endpoint | Metoda | Opis |
|----------|--------|------|
| `/api/login` | POST | Logowanie (password) |
| `/api/logout` | POST | Wylogowanie |
| `/api/dosing-status` | GET | Stan wszystkich kanałów |
| `/api/dosing-config` | GET | Konfiguracja kanałów |
| `/api/dosing-config` | POST | Zapis konfiguracji (→ pending) |
| `/api/calibrate` | POST | Uruchomienie kalibracji (channel=N) |

---

## 14. MODUŁY Z PROJEKTU BAZOWEGO (DOLEWKA)

### 14.1 Bez zmian (do współdzielenia)

| Moduł | Plik | Opis |
|-------|------|------|
| WiFi Manager | `wifi_manager.cpp/h` | Zarządzanie połączeniem |
| RTC Controller | `rtc_controller.cpp/h` | Obsługa DS3231 + NTP |
| Auth Manager | `auth_manager.cpp/h` | Hasło admin (hash w FRAM) |
| Session Manager | `session_manager.cpp/h` | Sesje użytkowników |
| Rate Limiter | `rate_limiter.cpp/h` | Ochrona przed brute-force |
| Credentials Manager | `credentials_manager.cpp/h` | Dane dostępowe w FRAM |
| Logging | `logging.cpp/h` | System logów |

### 14.2 Do adaptacji/nadpisania

| Moduł | Zmiany |
|-------|--------|
| FRAM Controller | Nowy layout adresów, struktury `ChannelConfig`, `DailyState` |
| VPS Logger | Nowy payload, zdarzenia dozownika |
| Web Server | Nowe endpointy API |
| Web Handlers | Obsługa konfiguracji kanałów |
| HTML Pages | Kompletnie nowy dashboard |
| Config | Nowe stałe, pinout |

### 14.3 Nowe moduły

| Moduł | Opis |
|-------|------|
| Relay Controller | Sterowanie 6 przekaźnikami + mutex |
| GPIO Validator | Walidacja pracy pomp przez GPIO |
| Dosing Scheduler | Harmonogram i wykonywanie eventów |
| Channel Manager | Zarządzanie konfiguracją kanałów |
| Daily Reset | Reset dobowy + aplikacja pending |
| Critical Error Handler | Obsługa błędów krytycznych |

---

## 15. STRUKTURA FRAM (PROPOZYCJA)

| Adres | Rozmiar | Zawartość |
|-------|---------|-----------|
| 0x0000 | 24 B | Magic + Version |
| 0x0018 | ~480 B | Encrypted Credentials (z bazowego projektu) |
| 0x0200 | 64 B | System Config |
| 0x0240 | 6 × 48 B | Active Config [6 kanałów] |
| 0x03C0 | 6 × 48 B | Pending Config [6 kanałów] |
| 0x0540 | 6 × 16 B | Daily State [6 kanałów] |
| 0x05A0 | 32 B | Error State |
| 0x05C0 | 64 B | VPS Log Buffer |

---

## 16. KWESTIE DO PRZYSZŁEJ IMPLEMENTACJI

| Temat | Status | Uwagi |
|-------|--------|-------|
| Zasilanie awaryjne | Planowane | Backup dla pomp, nadzór |
| Błąd krytyczny - rozszerzenie | Do decyzji | Zatrzymanie tylko kanału vs cały system |
| Retry logowania VPS | Opcjonalne | Można upchnąć między eventami |
| Powiadomienia push | Planowane | Błędy krytyczne na telefon |

---

## 17. PODSUMOWANIE KLUCZOWYCH ZASAD

| Zasada | Opis |
|--------|------|
| **Jeden relay aktywny** | Mutex sprzętowy - nigdy dwie pompy naraz |
| **Walidacja GPIO - jedna próba** | Brak retry, błąd = zatrzymanie systemu |
| **Pending changes** | Zmiany zawsze od następnej doby |
| **UTC wewnętrznie** | Wszystkie czasy w UTC, konwersja tylko w GUI |
| **FRAM persistence** | Wszystkie dane krytyczne w FRAM |
| **Godzina 00:xx zarezerwowana** | Reset + VPS, żadne eventy |
| **Offsety stałe** | Kanał N ma offset N×10 minut |

---

*Dokument wygenerowany jako podsumowanie ustaleń z sesji projektowej.*
*Służy jako źródło prawdy dla kolejnych etapów implementacji.*