# DOSING SYSTEM – SPECYFIKACJA ZINTEGROWANA
Wersja scalona, rozszerzona i ujednolicona pod kątem modeli LLM oraz czytelności dla człowieka. Dokument łączy treść **Specyfikacji Dozera** i **Specification of dosing system.md**, z zachowaniem kluczowej logiki, pełnego GUI, struktury FRAM oraz architektury API. Usunięto tryb CLI oraz plan implementacji.

---
# 1. PRZEZNACZENIE SYSTEMU
System służy do **automatycznego dozowania cieczy** w oparciu o:
- 1–6 kanałów (w zależności od wersji sprzętowej),
- harmonogram tygodniowy,
- dozowanie 1× lub 2× dziennie,
- kalibrację przepływu każdej pompy,
- gwarancję braku konfliktów między kanałami (relay mutex),
- pełną odporność na restarty dzięki FRAM i RTC.

System jest elementem większej platformy IoT i wykorzystuje wspólne moduły: WiFi, autoryzację, VPS logger, menedżer sesji i struktury kryptograficzne.

---
# 2. SPRZĘT
## 2.1 Platforma
**Seeed Studio XIAO ESP32-S3 Zero**

## 2.2 Podstawowe komponenty
- **MCU**: ESP32-S3
- **RTC**: DS3231M (I2C), źródło czasu UTC
- **Pamięć FRAM**: MB85RC256V (32 kB, I2C)
- **Przekaźniki**: 1–6 kanałów, sterowane sygnałem aktywnym HIGH
- **Pojedyncza pompa aktywna** w danym czasie (mutex sprzętowy)

## 2.3 Mapowanie pinów
- RELAY_1–6 → GPIO 1–6 (D0–D5)
- I2C SDA → GPIO 7 (D8)
- I2C SCL → GPIO 8 (D9)
- FRAM @ 0x50
- RTC @ 0x68

---
# 3. LIMITY I STAŁE SYSTEMOWE
- **MAX_WEEKLY_DOSING_VALUE**: 1000 ml
- **MAX_SINGLE_DOSE_VOLUME**: 50 ml
- **MAX_DOSING_DURATION**: 120 s
- **CALIBRATION_DURATION**: 30 s
- **DOSE_TOLERANCE_WINDOW**: 1800 s (±30 minut)
- **CHANNEL_COUNT**: 6 (konfigurowalne w kompilacji)
- **UTC wewnętrznie**, **CET na interfejsie**

---
# 4. STRUKTURY W FRAM
Specyfikacja Dozera wprowadza **sztywną mapę pamięci**, która zabezpiecza dane systemowe.

Adresy:
- 0x0000 – Magic + Version
- 0x0018 – *Encrypted Credentials* (współdzielone z Water System, **niezmienne**)
- 0x0500 – SystemConfig (64 B)
- 0x0540 – ChannelConfig[6] (6 × 48 B = 288 B)
- 0x0660 – DailyVolume[6] (6 × 16 B = 96 B)

## 4.1 ChannelConfig (48 B)
- weekly_schedule (bitmask 0–127)
- daily_schedule (1 lub 2)
- weekly_dosing_value (ml)
- dosing_rate (ml/s)
- enabled (0/1)
- last_dose_timestamp[2]
- doses_completed_today
- current_utc_day
- crc32

## 4.2 DailyVolume (16 B)
- volume_ml
- last_reset_utc_day
- crc32

## 4.3 SystemConfig (64 B)
- channel_count
- system_enabled
- version

---
# 5. ALGORYTMY I LOGIKA DOZOWANIA
## 5.1 Wyliczenia
Formuły:
```
days_active = popcount(weekly_schedule)
single_dose_ml = weekly_dosing_value / (days_active × daily_schedule)
duration_sec = single_dose_ml / dosing_rate
```
Warunki walidacji:
- single_dose_ml ≤ 50 ml
- duration_sec ≤ 120 s

## 5.2 Harmonogram godzin
Stały odstęp między kanałami:
```
interval = 43200 / CHANNEL_COUNT   // 12h / 6 = 7200 s (2h)
```
Jeżeli daily_schedule = 1:
- morning_time = channel_index × interval
- evening_time = UNUSED

Jeżeli daily_schedule = 2:
- morning_time = channel_index × interval
- evening_time = 43200 + (channel_index × interval)

## 5.3 Relay Mutex
Zasady krytyczne:
- w danym momencie **tylko jeden przekaźnik może być aktywny**,
- przed uruchomieniem pompy sprawdzane są wszystkie kanały,
- jeżeli inny kanał jest w stanie DOSING → obecny kanał **czeka**.

## 5.4 Wykonywanie dawek
System co 10 sekund:
- sprawdza aktualny czas UTC od północy,
- sprawdza dzień tygodnia,
- porównuje czas z tolerancją (±30 min),
- wykonuje dawkę, jeżeli nie została jeszcze wykonana.

## 5.5 Day Rollover
O północy UTC system:
- resetuje flagi wykonania dawek,
- resetuje `doses_completed_today`,
- aktualizuje `current_utc_day` w FRAM,
- może resetować struktury DailyVolume.

---
# 6. API – INTERFEJS HTTP
System wykorzystuje wspólny model sesji i hasła administratora.

## 6.1 Endpointy
```
GET  /api/dosing-config      → konfiguracje wszystkich kanałów
POST /api/dosing-config      → zapis konfiguracji (wymaga hasła)
POST /api/calibrate-channel  → kalibracja 30 s (wymaga hasła)
POST /api/manual-dose        → natychmiastowe uruchomienie dawki
GET  /api/dosing-status      → stan czasu rzeczywistego
```

## 6.2 Przykładowy JSON (skrócony)
```
{
  "id": 1,
  "enabled": true,
  "weekly_schedule": 127,
  "daily_schedule": 2,
  "weekly_dosing_value": 217,
  "dosing_rate": 0.33,
  "single_dose_volume": 15.5,
  "dosing_duration": 47,
  "dosing_times_utc": [0, 43200],
  "status_morning": "completed",
  "status_evening": "pending"
}
```

---
# 7. INTEGRACJA Z VPS
## 7.1 Zdarzenia
- DOSE_EXECUTED
- DOSE_MISSED
- DOSE_MANUAL
- CALIBRATION
- CONFIG_CHANGED

## 7.2 Payload
```
{
  "device_id": "DOSING_UNIT_001",
  "device_type": "dosing_system",
  "unix_time": 1729512345,
  "channel_id": 1,
  "event_type": "DOSE_EXECUTED",
  "volume_ml": 15.5,
  "dosing_duration": 47,
  "weekly_schedule": 127,
  "daily_schedule": 2,
  "system_status": "OK"
}
```

---
# 8. GRAFICZNY INTERFEJS UŻYTKOWNIKA (GUI)
Sekcja pochodzi z oryginalnej specyfikacji, ujednolicona i dopracowana.

## 8.1 Koncepcja interfejsu
GUI prezentuje **dwa równoległe tory czasu**:
- **Poranny (00:00–12:00 CET)**
- **Wieczorny (12:00–24:00 CET)**

Każdy tor zawiera **6 płytek (tiles)** odpowiadających kanałom.
- Jeżeli kanał ma `daily_schedule = 1` → płytka wieczorna jest widoczna, ale wyszarzona.
- Edycja odbywa się **w miejscu**, poprzez kliknięcie płytki.

## 8.2 Stany płytek (CSS)
- pending – zaplanowane, czekają na wykonanie
- completed – zakończone
- active – pompa aktualnie pracuje
- disabled – brak funkcji / niewłączone
- edit-mode – tryb formularza konfiguracji

## 8.3 Widok płytki – tryb odczytu
```
┌─────────────────────────┐
│ Channel 1      [✓ Done] │
│ Weekly: 217 ml          │
│ Single: 15.5 ml         │
│ Duration: 47 s          │
│ Schedule: Every day     │
│ ─────────────────────   │
│        01:00 CET        │
└─────────────────────────┘
```

## 8.4 Tryb edycji
```
┌─────────────────────────┐
│ Channel 1          [⚙]  │
│ [Enabled ▼]             │
│ [Every day ▼]           │
│ [2× per day ▼]          │
│ [217 ml/week]           │
│ Single: 15.5 ml         │
│ Duration: 47 s          │
│ [Save] [Cancel]         │
│ [Calibrate 30s]         │
└─────────────────────────┘
```

## 8.5 Logika GUI
- Wszystkie kalkulacje aktualizują się **natychmiast po zmianie parametrów**.
- Widok jest odświeżany co 5 sekund poprzez `/api/dosing-status`.
- Kolory i stany odzwierciedlają realny status algorytmu.

## 8.6 Fragment HTML/CSS/JS (uproszczony)
Fragment z oryginału (skondensowany dla dokumentacji):
```
<div class="timeline-track" id="morningTrack"></div>
<div class="timeline-track" id="eveningTrack"></div>

<script>
setInterval(() => {
  fetch('/api/dosing-status')
    .then(r => r.json())
    .then(updateTiles);
}, 5000);
</script>
```

---
# 9. ZASADY KRYTYCZNE
1. **Only one relay active at a time** (mutex sprzętowy)
2. Wszelkie dane muszą przetrwać restart (FRAM)
3. Wewnętrzne czasy zawsze w UTC
4. Dzienna zmiana czasu o północy UTC
5. Okno tolerancji ±30 minut
6. Staggered schedule – żadna pompa nie koliduje czasowo z inną

---
# 10. WSKAZÓWKI INTEGRACYJNE
## 10.1 Moduły współdzielone z Water System
- logging
- rtc_controller
- crypto
- wifi_manager
- security
- credentials_manager

## 10.2 Moduły do adaptacji
- fram_controller (nowy layout)
- vps_logger (payload)
- web handlers (dosing)

## 10.3 Nowe moduły
- hardware_pins
- relay_controller
- dosing_scheduler
- channel_controller
- dosing_web_handlers

---
# 11. PRESETY HARMONOGRAMU
Bitmask definiuje dni tygodnia, gdzie bit0 = poniedziałek, bit6 = niedziela.
- 127 → każdy dzień
- 31 → dni robocze
- 96 → weekend
- 1 → tylko niedziela
- 42 → wt/ czw / ndz

---
# 12. PRZYKŁAD WYLICZEŃ
217 ml/tydz, 2×/dzień, 7 aktywnych dni, rate = 0.33 ml/s:
- single_dose = 217 / (7 × 2) = 15.5 ml
- duration = 15.5 / 0.33 = 47 s
- czasy kanału 0: 00:00 + 12:00 UTC (01:00 + 13:00 CET)

---
# 13. PODSUMOWANIE
Dokument stanowi spójny i kompletny opis systemu dozowania, łącząc:
- logiczne i matematyczne działanie algorytmu,
- sprzęt, pamięć FRAM i restrykcje czasowe,
- pełną architekturę API,
- kompletny GUI z oryginalnego projektu,
- zasady integracji z infrastrukturą IoT.

Tryb CLI i plan implementacji zostały usunięte zgodnie z wytycznymi.

