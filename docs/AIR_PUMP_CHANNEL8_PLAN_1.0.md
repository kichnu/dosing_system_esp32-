# Pompa powietrza (fitoplankton) — wykorzystanie kanału 8 (CH7)

## Wymaganie

Kanał obsługujący małą pompę powietrza do mieszania fitoplanktonu:
- załączanie zgodnie z `time schedule` i `active days` (jak inne kanały)
- w sekcji "Configuration" pozostaje tylko: `Save` + input czasu pracy pompy (sekundy)
- reszta funkcjonalności (notatki, lock PIN, etykieta, enable/disable) jak w pozostałych kanałach

## Decyzja: przebudowa kanału 8 (CH7), nie nowy 9. kanał

CH7 (index 7) jest obecnie nieużywany — w `config.h` oznaczony jako `--`, bez fizycznej pompy, z wolnym GPIO 11.

### Koszt wariantu "9. kanał" (odrzucony)

- `CHANNEL_COUNT` 8→9 wymaga nowego GPIO (wolne: 9 lub 10) i przesunięcia wszystkich sekcji FRAM po blokach kanałowych (ACTIVE_CONFIG, PENDING_CONFIG, DAILY_STATE, CONTAINER_VOLUME, DOSED_TRACKER, CHANNEL_LABELS, CHANNEL_PARAMS = +108B)
- bump wersji layoutu FRAM (v8→v9) → factory reset przy pierwszym boocie
- trzeba poprawić 2 hardcodowane pętle `i<8` w `web_server.cpp` (linie ok. 1078, 1122) i rozszerzyć `CHANNEL_SLOT_MAP`

### Koszt wariantu "przebudowa CH7" (wybrany)

- zero zmian w FRAM, GPIO, `CHANNEL_COUNT`, slot-mapie, pętlach — kanał i jego pin już istnieją i są wolne
- cała generyczna logika (scheduler, channel_manager, relay_controller, notatki, lock PIN, labels) działa bez zmian — operuje po `CHANNEL_COUNT` i indeksie kanału, nie wie nic o "typie" kanału

## Realizacja trybu "duration" bez zmiany struktury FRAM

Nie potrzeba nowej struktury danych — to inny sposób wypełnienia istniejącego `ChannelConfig` (32B, FRAM v8 bez zmian).

`getPumpDurationMs()` liczy:
```
duration = (daily_dose_ml / eventsCount) / dosing_rate * 1000
```

Ustawiając `dosing_rate = 1.0` i `daily_dose_ml = żądane_sekundy × liczba_aktywnych_eventów`, wynik to dokładnie żądany czas pracy pompy na event — bez zmian w scheduler/relay_controller/FRAM.

Zmiana ogranicza się do GUI:
- dla CH7 sekcja "Configuration" renderuje jeden input (sekundy) + `Save`
- JS przy zapisie przelicza `daily_dose_ml` i wysyła `dosing_rate=1.0` w tle
- potrzebna flaga "duration mode" dla tego kanału — najprościej 1 bit w `ChannelParams._reserved` albo w `ChannelLabel`, odczytywana przez JS do przełączenia widoku konfiguracji

Reszta (time schedule, active days, enable/disable, notatki, lock) pozostaje wspólnym, niezmienionym kodem dla wszystkich kanałów.

## Do zrobienia (implementacja)

- [ ] Wybór i ustawienie flagi "duration mode" dla CH7 (bit w `ChannelParams` lub `ChannelLabel`)
- [ ] GUI: warunkowe renderowanie sekcji Configuration dla CH7 (input sekund + Save, bez dose/rate)
- [ ] JS: przeliczenie sekundy→`daily_dose_ml` (×eventsCount) i wysyłka `dosing_rate=1.0` przy zapisie
- [ ] Etykieta kanału CH7 (np. "Air pump" / "Phyto mix") w `CHANNEL_LABELS`
- [ ] Weryfikacja na sprzęcie (GPIO 11, realny czas pracy pompy powietrza)
