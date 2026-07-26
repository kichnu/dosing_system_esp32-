# VPS / WireGuard — audyt pod kątem kamery akwarium (Raspberry Pi)

Data: 2026-07-05
Kontekst: research pod nowy projekt podglądu akwarium (Raspberry Pi + kamera, pan-tilt), niezależny od DOZOWNIK. Audyt istniejącej infrastruktury VPS/WireGuard przed dodaniem nowego urządzenia.

## 1. WireGuard (wg0)

- Interfejs: `10.99.0.1/24`, port nasłuchu `51820/udp` (publiczny, 0.0.0.0 i ::)
- Skonfigurowane peery (tylko 2):
  - **MikroTik** (`10.99.0.2/32`, `AllowedIPs = 10.99.0.2/32, 192.168.0.0/16`) — to router domowy, keepalive 25s, aktywny handshake (1m31s temu), duży transfer (14.9 GiB odebrane)
  - **Pixel8 Phone** (`10.99.0.3/32`) — ostatni handshake 211 dni temu, brak keepalive — peer nieaktywny/rzadko używany
- **Kluczowy wniosek:** urządzenia domowe (top_off_water/doser/thermo/switch pod `192.168.10.2/.3/.4/.10`) NIE są bezpośrednimi peerami WG. MikroTik jest jedynym bramowym peerem i ma `AllowedIPs 192.168.0.0/16` — to on routuje/NATuje ruch z VPS do całej domowej sieci LAN. VPS (nginx) kieruje żądania wprost na adresy `192.168.10.x`, a pakiety idą przez tunel do MikroTika, który przekazuje je dalej w LAN.

## 2. IP forwarding i reguły FORWARD/NAT

- `net.ipv4.ip_forward = 1` (trwałe, `/etc/sysctl.conf`)
- iptables FORWARD (policy DROP): jawne `ACCEPT` dla `wg0 -> *`, `* -> wg0`, `wg0 -> wg0` — ruch przez tunel jest w pełni przepuszczany
- MASQUERADE tylko dla sieci Dockera (172.17.0.0/16, 172.18.0.0/16) — WG nie jest maskowany na VPS, bo routing do LAN robi MikroTik po swojej stronie
- Potwierdza to model z pkt 1: VPS tylko przekazuje pakiety w tunelu, całe NAT/routing do konkretnych urządzeń dzieje się za MikroTikiem

## 3. Reverse proxy (nginx 1.26.3)

- `app.krzysztoforlinski.pl` (443, SSL/Certbot) — proxy do Flask (127.0.0.1:5001, auth/dashboard/webauthn), ESP32 API (127.0.0.1:5000), oraz per-urządzenie przez `auth_request` do `192.168.10.2/.3/.4/.10`
- `remote.krzysztoforlinski.pl` (443, SSL) — Guacamole (127.0.0.1:8080 → docker 172.18.0.4:8080), rate-limit 10r/s
- Port 80 na obu domenach: tylko redirect na 443

## 4. Firewall (ufw) — zweryfikowane

Reguły INPUT (default deny incoming):
| Port | Reguła | Uwaga |
|---|---|---|
| 2244/tcp | ALLOW Anywhere | SSH na niestandardowym porcie — celowe |
| 80, 443 (+udp) | ALLOW Anywhere | nginx, oczekiwane |
| 51820/udp | ALLOW Anywhere | WireGuard, oczekiwane |
| 5000 (tcp+udp) | ALLOW Anywhere | ESP32 API — otwarty bezpośrednio, nie tylko przez nginx |
| 8080 | ALLOW tylko z 10.10.0.0/24 | Guacamole — ograniczone, ale **10.10.0.0/24 to inna podsieć niż wg0 (10.99.0.0/24)**, źródło niejasne (do wyjaśnienia, nie pilne) |

**Ustalone: port 5001 (Flask admin) NIE jest otwarty w ufw.** Wcześniejsza obawa (Flask nasłuchuje na 0.0.0.0:5001) była nieuzasadniona — default deny w ufw blokuje go z zewnątrz mimo bindu na wszystkich interfejsach. Można rozważyć związanie z 127.0.0.1 jako defense-in-depth, ale nie jest to pilne.

## 5. Zasoby serwera

Ubuntu 25.04, kernel 6.14, 6 CPU, 11 GiB RAM (~9.5 GiB wolne), 96 GB dysk (87 GB wolne). Wystarczające pod dodatkowy stream wideo — realnym ograniczeniem będzie przepustowość łącza domowego (Pi za NAT), nie zasoby VPS.

## 6. Wnioski dla projektu kamery (Raspberry Pi + pan-tilt)

1. **Nie trzeba dodawać nowego peera WG na VPS.** Skoro MikroTik ma już `AllowedIPs 192.168.0.0/16`, Raspberry Pi wystarczy podłączyć do domowego LAN (np. statyczny IP `192.168.10.11`) — będzie automatycznie osiągalny z VPS przez istniejący tunel, tak jak pozostałe urządzenia IoT.
2. Po stronie VPS trzeba tylko dopisać nowy `location`/upstream w nginx (analogicznie do wpisów dla `192.168.10.2/.3/.4/.10`) wskazujący na Pi, ewentualnie z tym samym mechanizmem `auth_request`.
3. Jeśli stream ma być dostępny bez przechodzenia przez nginx/auth (np. wygodniejszy dostęp dla RTSP), można rozważyć wzorzec z portu 5000 (bezpośrednia reguła ufw) — ale wtedy stream trafia na świat bez warstwy auth, do przemyślenia.
4. Konfiguracja pan-tilt/serw i sam wybór oprogramowania streamującego (mediamtx/rtsp-simple-server) — do ustalenia w kolejnym kroku, nie wymaga zmian po stronie VPS/WG poza pkt 2-3.
5. Do wyjaśnienia przy okazji (niezwiązane z kamerą): pochodzenie reguły ufw dla `10.10.0.0/24` na porcie 8080 — nie pasuje do żadnej znanej podsieci (wg0 to 10.99.0.0/24).
