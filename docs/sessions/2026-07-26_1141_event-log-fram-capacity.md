# 2026-07-26 — Event Log (notatnik zdarzeń) — analiza FRAM, plan funkcji

## Kontekst

Pomysł: nowy ring buffer w FRAM na notatnik zdarzeń opieki nad akwarium,
koncepcyjnie podobny do funkcji w aplikacji "Reef Parameter Tracker".
Osobna funkcja od istniejącego `SharedNotes` (pula 12 "przyklejonych"
notatek per kanał, bez timestampu) i od `ParamLog` (ring pomiarów
liczbowych) — to ma być chronologiczny log zdarzeń z wolnym tekstem.

**Na razie tylko analiza — nic nie zaimplementowane.**

## Specyfikacja GUI (ustalona, nie zaimplementowana)

- Limit tekstu notatki: **160 znaków**.
- Przycisk **"Add"** u góry.
- **Eksport do pliku**.
- Przycisk **"Remove"** pod spodem.
- Input tekstowy: domyślnie widoczna wysokość **jednego wiersza**
  (textarea rozwijalna zapewne, ale startowo 1 wiersz).
- Subtelna informacja/wskaźnik przy przekroczeniu limitu 160 znaków
  podczas wpisywania (nie ustalono jeszcze dokładnej formy — np. licznik
  znaków zmieniający kolor).

## Analiza pojemności FRAM

Bazuje na `src/config/fram_layout.h` (layout v8). Sekcja RESERVED
(0x13CC → 0x7FFF) = **27 700 B wolnego, ciągłego miejsca** (~84,5% całego
chipu 32KB), jedyna duża nietknięta strefa w layoucie.

### Proponowana struktura rekordu (wzorem `ParamRecord`/`NoteEntry` z `dosing_types.h`)

```
timestamp   uint32_t   4 B   — Unix UTC
text        char[160]  160 B — limit = rozmiar tablicy (jak NoteEntry, bez
                                gwarantowanego \0 przy pełnym wypełnieniu)
flags       uint8_t    1 B   — bit0: slot ważny
_pad        uint8_t[3] 3 B   — wyrównanie do 4B (konwencja pliku)
-----------------------------
razem/wpis             168 B
```

Nagłówek ringu (analogicznie do `head`/`count`/`crc32` w `ParamLog`, bez
`tmpl_count` — tu nie ma szablonów):

```
head   uint8_t   1 B
count  uint8_t   1 B
_pad   uint8_t[2] 2 B
crc32  uint32_t  4 B
-----------------------------
razem meta        8 B
```

### Wynik przy ringu 50 wpisów

```
50 × 168 B = 8400 B
+ 8 B (meta)
= 8408 B (8,21 KB) na całą sekcję
```

**Zostaje: 27 700 − 8408 = 19 292 B (~18,84 KB) wolnego w RESERVED.**

Wariant z gwarantowanym `\0` (`text[161]`, rekord zaokrąglony do 172B z
paddingiem): sekcja 8608B, zostaje 19 092 B — różnica pomijalna (~200B).

## Otwarte / do ustalenia przy implementacji

- Dokładna forma "subtelnej informacji" o przekroczeniu 160 znaków w GUI.
- Adres sekcji w `fram_layout.h` (nowy `FRAM_ADDR_EVENT_LOG` za
  `PARAM_LOG`, przed `RESERVED`) + bump `FRAM_LAYOUT_VERSION` (obecnie 8)
  → wymusi factory reset przy starej wersji FRAM.
- Czy `_pad[3]`/`text[161]` — decyzja o gwarancji null-terminacji.
- Backend API (GET/POST/DELETE) i eksport do pliku (format — CSV? podobnie
  jak eksport CSV w sekcji Parameters, patrz `2cf20630`/Etap 6 w historii
  commitów).
