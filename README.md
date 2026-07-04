# DOZOWNIK

System dozowania nawozów/pokarmu (akwarium/hydroponika) na ESP32-S3, do 8 kanałów pomp perystaltycznych, z harmonogramem godzinowym i konfiguracją przez WWW. Dane trwałe w FRAM (MB85RC256V), zegar DS3231, bezpieczeństwo: master relay + blokada critical error.

Szczegóły architektury, mapa FRAM, konwencje kodu → [CLAUDE.md](CLAUDE.md) (źródło prawdy — nie duplikowane tutaj).

## Build

```bash
pio run -e production        # bez CLI, minimalne logi — do wgrania na docelowe urządzenie
pio run -e debug              # pełne CLI i logi — do rozwoju/testów
pio run -e production -t upload
pio device monitor -b 115200
```

## Status

Trwa przebudowa v4.0 (ULN2003AN, 8 kanałów, przygotowanie pod monitoring pomp Edge Impulse) — patrz `docs/sessions/` i historia commitów.

## Dokumentacja (docs/)

| Plik | Zawartość |
|---|---|
| `FRAM_MAP.txt` | Aktualna mapa pamięci FRAM (adresy, sekcje, wersja layoutu) |
| `SECURITY_PATTERNS_1.0.md` | Wzorce bezpieczeństwa IoT+VPS (WireGuard), wspólne dla wszystkich urządzeń ESP32 |
| `buzzer_control.md` | Sterowanie buzzerem przez FreeRTOS task (zamiast pollingu millis()) |
| `CHANNEL_LOCK_PLAN_1.0.md` | Plan blokady edycji kanału w GUI — ochrona przed przypadkową zmianą |
| `AIR_PUMP_CHANNEL8_PLAN_1.0.md` | Kanał 8 (CH7) jako pompa powietrza — tryb czasowy zamiast dawkowania ml |
| `G3VM-61G1_NOTES_1.0.md` | Notatki o przekaźniku MOSFET G3VM-61G1 |
| `VPS_PROXY_SESSION_EXPIRY_1.0.md` | Obsługa wygaśnięcia sesji za VPS reverse proxy (Nginx) — strona ESP32 zaimplementowana, nginx/Flask jako referencja |
| `Captive_portal_full_specification_1.0.md` | Specyfikacja captive portal — przeniesiona z projektu DOLEWKA (ESP32-C3), do weryfikacji aktualności dla DOZOWNIKA |
| `MEMORY_LEAK_FIXES_1.0.md` | Analiza wycieku pamięci — przeniesiona z projektu DOLEWKA (ESP32-C3), do weryfikacji aktualności |
| `ESP32-S3-Pico.png`, `Results_88800_pl (1).pdf` | Materiały pomocnicze (pinout płytki, wyniki testów wody) |
| `sessions/` | Log sesji roboczych (chronologicznie) |

## Do uporządkowania (otwarte)

- `Captive_portal_full_specification_1.0.md` i `MEMORY_LEAK_FIXES_1.0.md` — przeniesione z DOLEWKI, do weryfikacji czy nadal aktualne dla DOZOWNIKA.
