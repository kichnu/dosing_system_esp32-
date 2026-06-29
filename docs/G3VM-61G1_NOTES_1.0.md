# G3VM-61G1 — MOS FET Relay (Omron)

SOP 4-pin, SPST-NO, izolacja galwaniczna wejście/wyjście 1500 Vrms.

## Kluczowe parametry

| Parametr | Wartość |
|---|---|
| Io ciągły max | 400 mA |
| Napięcie obciążenia max | 60 V |
| Ron typowy | 1 Ω (przy IF=5mA, Io=400mA) |
| Ron max | 2 Ω |
| LED VF typowe | 1.15 V (przy IF=10mA) |
| Trigger current IFT | 1.6 mA typowy, 3 mA max |
| Czas załączenia | 0.8 ms typowy, 2 ms max |
| Obudowa | SOP-4 (SMD) |

## Sterowanie z ESP32 (3.3V GPIO)

Rezystor szeregowy z LED-em wejściowym: `(3.3 - 1.15) / 0.0075 ≈ 287Ω` → użyć 330Ω.
Prąd roboczy LED: 7.5 mA (tryb typowy), max 25 mA.

## Zastosowanie: silnik DC do 150 mA

Spadek napięcia przy 150 mA: `0.15A × 1Ω = 0.15V` — pomijalny.
Margines prądowy: 150 mA z limitu 400 mA → 37% obciążenia.

## vs ULN2003AN

ULN2003AN ma ~2V spadku na tranzystorze Darlington przy typowym prądzie.
G3VM-61G1: 0.15V przy 150 mA — różnica krytyczna przy zasilaniu 5V.

## Uwaga

Izolacja galwaniczna jest zaletą przy wspólnym GND z siecią AC lub niezależnym zasilaniu silnika.
Jeśli GND wspólny i izolacja zbędna — rozważyć logiczny N-MOSFET (AO3400, IRLZ44N):
Ron ~30–100 mΩ, bramka wysokoimpedancyjna (bez rezystora prądowego), cena kilkanaście groszy.
