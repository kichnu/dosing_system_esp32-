# Specyfikacja Algorytmu Systemu Automatycznego Dolewania Wody

## **1. DEFINICJE ZMIENNYCH SYSTEMOWYCH**

### **1.1 Zmienne Czujników**
- **`T_1`** - Czujnik poziomu wody nr 1
- **`T_2`** - Czujnik poziomu wody nr 2
- **`T_1-HIGH`** - Stan czujnika T_1 przy niskim poziomie wody (aktywny)
- **`T_2-HIGH`** - Stan czujnika T_2 przy niskim poziomie wody (aktywny)
- **`T_1-LOW`** - Stan czujnika T_1 przy wysokim poziomie wody (nieaktywny)
- **`T_2-LOW`** - Stan czujnika T_2 przy wysokim poziomie wody (nieaktywny)
- **`TRIGGER`** - Aktywacja T_1 OR T_2 (początek cyklu)

### **1.2 Zmienne Pompy**
- **`PUMP`** - Pompa dolewająca wodę
- **`PUMP_WORK`** - Status pracy pompy (boolean)
- **`PUMP_START`** - Moment startu pompy (timestamp)
- **`PUMP_STOP`** - Moment zatrzymania pompy (timestamp)
- **`PUMP_WORK_TIME`** - Czas pracy pompy (sekundy)

### **1.3 Zmienne Czasowe Systemu**
- **`TIME_TO_PUMP`** - Czas od TRIGGER do załączenia PUMP (sekundy)
- **`TRYB_1_TIME`** - Czas trwania TRYB_1 = TIME_TO_PUMP (sekundy)
- **`TRYB_2_TIME`** - Czas trwania TRYB_2 (sekundy)
- **`IDLE_TIME`** - Czas oczekiwania na TRIGGER (sekundy)
- **`LOGGING_TIME`** - Czas przeznaczony na logowanie (domyślnie 5s)
- **`CYCLE`** - Pełny cykl pracy systemu

### **1.4 Zmienne Analizy Czasowej**
- **`TIME_GAP_1`** - Różnica czasu aktywacji między T_1 i T_2 (sekundy)
- **`TIME_GAP_2`** - Różnica czasu deaktywacji między T_1 i T_2 (sekundy)
- **`TIME_GAP_1_MAX`** - Maksymalny czas oczekiwania na TIME_GAP_1 (sekundy)
- **`TIME_GAP_2_MAX`** - Maksymalny czas oczekiwania na TIME_GAP_2 (sekundy)
- **`WATER_TRIGGER_TIME`** - Czas od PUMP_START do deaktywacji T_1 OR T_2 (sekundy)
- **`WATER_TRIGGER_MAX_TIME`** - Maksymalny czas oczekiwania na deaktywację czujników (sekundy)

### **1.5 Zmienne Progowe**
- **`THRESHOLD_1`** - Próg czasowy dla TIME_GAP_1 (sekundy)
- **`THRESHOLD_2`** - Próg czasowy dla TIME_GAP_2 (sekundy)
- **`THRESHOLD_WATER`** - Próg czasowy dla WATER_TRIGGER_TIME (sekundy)

### **1.6 Zmienne Objętości**
- **`FILL_WATER`** - Suma dolanej wody w ostatnich 24h (ml)
- **`FILL_WATER_MAX`** - Maksymalna dolewka wody na dobę (ml)
- **`SINGLE_DOSE_VOLUME`** - Objętość jednej dolewki (domyślnie 100ml, konfigurowalna przez WWW)

### **1.7 Funkcje Systemowe**
- **`SENSOR_TIME_MATCH_FUNCTION(time, threshold)`** - Funkcja porównująca czasy
  - Zwraca **0** jeśli `time < threshold`
  - Zwraca **1** jeśli `time >= threshold`

## **2. OGRANICZENIA CZASOWE (static_assert)**

```
TIME_TO_PUMP > 300s AND TIME_TO_PUMP < 600s
TIME_TO_PUMP > (TIME_GAP_1_MAX + 10s)
TIME_TO_PUMP > (THRESHOLD_1 + 30s)
WATER_TRIGGER_MAX_TIME > PUMP_WORK_TIME
```

## **3. ALGORYTM GŁÓWNY**

### **3.1 TRYB_1 - Opadanie Poziomu Wody**

**Warunki rozpoczęcia:** Aktywacja TRIGGER (T_1 OR T_2)

1. **Pomiar TIME_GAP_1:**
   - System czeka na aktywację drugiego czujnika przez czas `TIME_GAP_1_MAX`
   - Jeśli drugi czujnik się aktywuje: `TIME_GAP_1 = |czas_aktywacji_T_2 - czas_aktywacji_T_1|`
   - Jeśli drugi czujnik się nie aktywuje: `TIME_GAP_1 = TIME_GAP_1_MAX`

2. **Analiza TIME_GAP_1:**
   - `TRYB_1_result = SENSOR_TIME_MATCH_FUNCTION(TIME_GAP_1, THRESHOLD_1)`

3. **Opóźnienie i start pompy:**
   - System czeka `TIME_TO_PUMP` sekund od TRIGGER
   - Uruchamia PUMP na czas `PUMP_WORK_TIME`

**Warunki zakończenia:** Moment PUMP_START

### **3.2 TRYB_2 - Podnoszenie Poziomu Wody**

**Warunki rozpoczęcia:** Moment PUMP_START

1. **Pierwsza próba dolewania:**
   - Pompa pracuje przez `PUMP_WORK_TIME`
   - System mierzy `WATER_TRIGGER_TIME` (czas do deaktywacji czujników)

2. **Logika powtórzeń (maksymalnie 3 próby):**
   ```
   FOR i = 1 TO 3:
       IF WATER_TRIGGER_TIME > WATER_TRIGGER_MAX_TIME:
           IF i < 3:
               PUMP_START = current_time
               continue next iteration
           ELSE:
               ERROR = "ERR2" // 3 nieudane próby
               GOTO emergency_shutdown
       ELSE:
           BREAK // sukces
   ```

3. **Analiza TIME_GAP_2 (tylko przy sukcesie):**
   ```
   IF TRYB_1_result == 0:
       System czeka na deaktywację drugiego czujnika
       TIME_GAP_2 = |czas_deaktywacji_T_2 - czas_deaktywacji_T_1|
       TRYB_2_result = SENSOR_TIME_MATCH_FUNCTION(TIME_GAP_2, THRESHOLD_2)
   ELSE:
       TIME_GAP_2_result = 0 // nie mierzono
   ```

4. **Analiza WATER_TRIGGER_TIME:**
   ```
   WATER_result = SENSOR_TIME_MATCH_FUNCTION(WATER_TRIGGER_TIME, THRESHOLD_WATER)
   ```

**Warunki zakończenia:** 
- Sukces: Przejście do LOGGING_TIME
- Błąd: Emergency shutdown z resetem

### **3.3 TRYB IDLE**

**Funkcje:**
- Oczekiwanie na TRIGGER
- Ignorowanie dodatkowych TRIGGER podczas aktywnych trybów
- Gotowość do rozpoczęcia nowego CYCLE

## **4. OBSŁUGA BŁĘDÓW KRYTYCZNYCH**

### **4.1 Procedura Emergency Shutdown**
1. **Zapis danych:** Logowanie do FRAM i bazy danych z odpowiednim kodem błędu
2. **Sygnalizacja:** Wystawienie sygnału na pin sterownika według wzorca błędu:
   - **ERR1:** logiczna '1' przez 500ms, logiczna '0' przez 3000ms, powtarzanie
   - **ERR2:** logiczna '1' przez 500ms, logiczna '0' przez 500ms, logiczna '1' przez 500ms, logiczna '0' przez 3000ms, powtarzanie
   - **ERR0:** logiczna '1' przez 500ms, logiczna '0' przez 500ms, logiczna '1' przez 500ms, logiczna '0' przez 500ms, logiczna '1' przez 500ms, logiczna '0' przez 3000ms, powtarzanie
3. **Oczekiwanie:** System czeka na fizyczny reset na dedykowanym pinie
4. **Restart:** Po resecie przejście do IDLE_TIME

### **4.2 Warunki Emergency Shutdown**
- **ERR1:** `FILL_WATER > FILL_WATER_MAX`
- **ERR2:** 3 nieudane próby pompy w TRYB_2
- **ERR0:** Oba błędy jednocześnie

## **5. SYSTEM LOGOWANIA DANYCH**

### **5.1 Format JSON Wynikowego**
```
XX-YY-ZZ-VVVV
```

### **5.2 Składniki Formatu**

**XX (00-99, OV, ER):** 
- Suma wyników `SENSOR_TIME_MATCH_FUNCTION` dla `TIME_GAP_1` z ostatnich 2 tygodni
- **OV:** jeśli suma > 99
- **ER:** błędy systemu

**YY (00-99, OV, ER):**
- Suma wyników `SENSOR_TIME_MATCH_FUNCTION` dla `TIME_GAP_2` z ostatnich 2 tygodni  
- **OV:** jeśli suma > 99
- **ER:** błędy systemu

**ZZ (00-99, OV, ER):**
- Suma wyników `SENSOR_TIME_MATCH_FUNCTION` dla `WATER_TRIGGER_TIME` z ostatnich 2 tygodni
- **OV:** jeśli suma > 99  
- **ER:** błędy systemu

**VVVV (0000-9999, OVER, ERR0/ERR1/ERR2):**
- `FILL_WATER = liczba_PUMP_WORK × SINGLE_DOSE_VOLUME` z ostatnich 24h
- **OVER:** jeśli wartość > 9999
- **ERR0/ERR1/ERR2:** kody błędów krytycznych

### **5.3 Pamięć FRAM**
- **Model:** Adafruit FRAM 256k bitów (32 KB)
- **Wykorzystanie:** Lokalne przechowanie danych cykli z ostatnich 14 dni
- **Zapas:** ~75% wolnej pamięci na rozszerzenia i buforowanie

### **5.4 Procedura Logowania**
1. **Kalkulacja wartości:** Obliczenie sum z ostatnich okresów
2. **Sprawdzenie limitów:** Weryfikacja `FILL_WATER < FILL_WATER_MAX`
3. **Zapis FRAM:** Lokalne przechowanie danych w pamięci FRAM
4. **Zapis baza:** Transmisja na zdalny serwer
5. **Publikacja WWW:** Wyświetlenie aktualnych danych

## **6. SPECYFIKACJA PINÓW I SYGNAŁÓW**

### **6.1 Pin Sygnału Błędu**
- **Funkcja:** Kodowanie typu błędu poprzez sekwencje impulsów
- **Wspólny pin** dla błędów ERR0, ERR1, ERR2
- **Kodowanie błędów:**
  - **ERR1:** 1 impuls (500ms HIGH + 3000ms LOW)
  - **ERR2:** 2 impulsy (500ms HIGH + 500ms LOW + 500ms HIGH + 3000ms LOW)  
  - **ERR0:** 3 impulsy (500ms HIGH + 500ms LOW + 500ms HIGH + 500ms LOW + 500ms HIGH + 3000ms LOW)
- **Cykl:** Ciągłe powtarzanie sekwencji do momentu resetu

### **6.2 Pin Reset**
- **Funkcja:** Fizyczny reset systemu po błędach krytycznych
- **Aktywacja:** Zewnętrzny przycisk/sygnał resetujący
- **Efekt:** Przerwanie sygnalizacji błędu i powrót do IDLE_TIME

### **6.3 Rozróżnienie Przyczyn**
- **Metoda:** Analiza wzorca impulsów na pinie błędu
- **Backup:** Dane zapisane w FRAM przed wystąpieniem błędu
- **Diagnostyka:** Możliwość identyfikacji przyczyny przez liczenie impulsów

## **7. TRYBY PRACY SYSTEMU**

```
START → IDLE_TIME → TRIGGER → TRYB_1 → TRYB_2 → LOGGING_TIME → IDLE_TIME
         ↑                                         ↓
         ←────────── normal cycle ─────────────────┘
         ↑                                         ↓
         ←─── emergency reset ←─── ERROR ←─────────┘
```

## **8. ZMIENNE OPERACYJNE FINALNE**

### **8.1 Sugerowane Nazwy Profesjonalne**
```c++
// Czujniki
bool sensor_T1_state;
bool sensor_T2_state;
uint32_t trigger_timestamp;

// Pompa
bool pump_active;
uint32_t pump_start_time;
uint32_t pump_stop_time;
uint16_t pump_work_duration;  // sekundy

// Analiza czasowa
uint16_t activation_time_gap;   // TIME_GAP_1
uint16_t deactivation_time_gap; // TIME_GAP_2
uint16_t water_response_time;   // WATER_TRIGGER_TIME

// Progi i limity
uint16_t activation_threshold;  // THRESHOLD_1
uint16_t deactivation_threshold; // THRESHOLD_2
uint16_t water_response_threshold; // THRESHOLD_WATER
uint16_t max_daily_volume;      // FILL_WATER_MAX

// Statystyki
uint16_t daily_water_volume;    // FILL_WATER
uint8_t pump_attempts_count;
uint32_t cycle_start_time;

// Dane logowania (2 tygodnie)
uint8_t activation_errors_sum;   // XX
uint8_t deactivation_errors_sum; // YY  
uint8_t response_errors_sum;     // ZZ
```

System zapewnia ciągłą pracę z automatycznym reagowaniem na zmiany poziomu wody, diagnostyką czujników oraz zabezpieczeniami przed przepełnieniem.
