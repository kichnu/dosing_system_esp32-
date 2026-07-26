# 2026-07-25 — Relay active-LOW swap + OTA implementation prep

## Context

Branch: `feature/air-pump-channel8`. Two threads of work in this session:
1. Physical swap of pump drivers from ULN2003AN to relays — firmware needs
   active-LOW logic on `PUMPS_PINS` to match new hardware.
2. Port the OTA-over-WiFi pattern from `thermo_control`
   (`docs/OTA_WIFI_UPLOAD_PATTERN.md`) into this project.

## Done

### Relay active-LOW swap
- `src/hardware/relay_controller.h/.cpp`: `_setPump()` now LOW=ON/HIGH=OFF,
  `begin()` initializes pins HIGH (off); comments updated.
- `src/hardware/safety_manager.cpp`: `_takePumpSnapshot()` checks `== LOW` for
  "was running" detection.
- `src/cli/cli_tests.cpp`: `measureGpioTiming()` HIGH/LOW → ON/OFF labels
  flipped to match new polarity.
- `src/config/config.h`: pinout header comment + ASCII wiring diagram
  (`ULN2003AN` → `RELAY (LOW=ON)`).
- `src/main.cpp`, `README.md`: init log text / status line updated.
- `README.md`: added "Sterowanie pompami — przekaźniki (Active LOW)" note —
  logic is ready in code, but the physical board swap (electrical
  implementation) is not done yet. Candidate component notes already existed
  in `docs/G3VM-61G1_NOTES_1.0.md`.
- Verified: `pio run -e production` builds clean.

### OTA investigation (read-only, no code changes yet)
- Device connected at `/dev/ttyACM1` (VID 303A, Espressif USB-JTAG/serial).
- `esptool.py flash_id` (via `~/.platformio/packages/tool-esptoolpy/esptool.py`
  — the `pio` penv's own `esptool.py` CLI is currently broken, click/esptool
  version mismatch, `TypeError: ParamType.get_metavar()`) confirms **real
  flash = 16MB**. Also surfaced: **PSRAM = 2MB embedded**, not "8MB Octal" as
  claimed in the `config.h` pinout comment (Waveshare board doc) — separate,
  unresolved discrepancy, not touched this session.
- Current `platformio.ini`: `board_build.partitions = huge_app.csv` → single
  `app0` slot (0x300000, no `app1`), partition table only maps 4MB even
  though the chip is 16MB. This is exactly the trap
  `docs/OTA_WIFI_UPLOAD_PATTERN.md` warns about — ArduinoOTA would `abort()`
  in `esp_flash_erase_region` today, same failure mode as `thermo_control`.
- Current firmware size (production): 1,193,474 / 3,145,728 B (37.9% of the
  single app0 slot). RAM 15.2%.
- No SPIFFS/LittleFS/FFat usage in project code (storage is FRAM-based) — a
  data partition in the new table is not functionally needed, just kept for
  parity with the stock partition CSVs.

## Plan (not yet applied)

1. `platformio.ini`:
   - `[platformio]` section + `default_envs = production` (avoid bare
     `pio run` building every env, including `_ota` ones).
   - `board_build.partitions = app3M_fat9M_16MB.csv` (two 3MB OTA slots —
     same size as today's single slot, so headroom is unchanged).
   - `-DOTA_PASSWORD=\"${sysenv.OTA_PASSWORD}\"` in common `[env]` build_flags
     (baked into every build, USB or OTA — needed because the *same* firmware
     image serves both the initial USB flash and later OTA updates).
   - New `[env:production_ota]` / `[env:debug_ota]`, `extends` their base env,
     `upload_protocol = espota`, `upload_flags = --auth=${sysenv.OTA_PASSWORD}
     --host_port=8266`.
2. `src/main.cpp`:
   - `#include <ArduinoOTA.h>`, `ArduinoOTA.setHostname("dozownik")`.
   - In `initNetwork()`, guarded by `initStatus.wifi_ok`: `setPassword()`,
     `onStart()` → `relayController.allOff()` + save/set `systemHalted` (must
     restore previous halt state on `onError`, not blindly clear it — a
     pre-existing critical-error halt must not be lifted by a failed OTA),
     `onError()` restores it, `begin()`.
   - `ArduinoOTA.handle()` early in `loop()`, every iteration, before the
     critical-error early return.
3. Build + **first upload via USB** (device already attached) to deploy the
   new partition table + OTA code together.
4. `README.md`: add WiFi-upload command block (`export OTA_PASSWORD=...` in
   the same shell command as `pio run`, USB first, then `-e production_ota
   -t upload --upload-port dozownik.local`).

## Blocked on

Waiting on the user to supply the `OTA_PASSWORD` value to bake into
build_flags before writing the `platformio.ini`/`main.cpp` changes and doing
the USB reflash (device is connected and ready).

## 2026-07-26 — Plan applied

User supplied `OTA_PASSWORD`. Applied the plan above:
- `platformio.ini`: `[platformio] default_envs = production`,
  `board_build.partitions = app3M_fat9M_16MB.csv`, `-DOTA_PASSWORD=...` in
  common `build_flags`, new `[env:production_ota]`/`[env:debug_ota]`
  (`extends`, `upload_protocol = espota`, `upload_port = dozownik.local`,
  `--auth`/`--host_port=8266`).
- `src/main.cpp`: `#include <ArduinoOTA.h>`; in `initNetwork()` (guarded by
  `initStatus.wifi_ok`) — `setHostname("dozownik")`, `setPassword()`,
  `onStart()` saves prior `systemHalted`, calls `relayController.allOff()`,
  sets `systemHalted = true`; `onError()` restores the saved halt state
  (does not blindly clear it); `ArduinoOTA.begin()`. `ArduinoOTA.handle()`
  added as the very first line of `loop()`.
- Both `production` and `debug` build clean with the new partition table
  (full rebuild triggered, expected). Flash usage unchanged (~39-42% of the
  3MB app slot — same slot size as before).
- `README.md`: new "OTA (upload przez WiFi)" subsection under Build, with
  first-upload-via-USB + `*_ota` env commands, mDNS fallback via
  `--upload-port`, and the "OTA_PASSWORD same shell command" gotcha.

**Still open**: device not connected this session (`/dev/ttyACM1` absent) —
first USB upload with the new partition table + OTA code has not been done
yet. Do that before trying `*_ota` envs (OTA doesn't exist on the device
until firmware with `ArduinoOTA.begin()` is flashed once via USB).

## 2026-07-26 — End-to-end verified

Device connected (`/dev/ttyACM1`). First USB upload (`debug` env, new
partition table + OTA code) succeeded. Boot log confirmed: WiFi OK
(192.168.10.3), `[INIT] OTA... OK (dozownik.local)`, `PROV_BUTTON_PIN` now
reads GPIO4 (per the earlier pin-remap change), SYSTEM READY, no critical
error.

Then did a real OTA-over-WiFi upload (`debug_ota`, `--upload-port
192.168.10.3` — mDNS `dozownik.local` untested, used IP directly): 37.5s,
`Result: OK`. Device rebooted cleanly afterward — SYSTEM READY again, no
stuck `systemHalted`, scheduler/safety/WiFi all fine. `onStart`/`onError`
halt-state save/restore not stress-tested (this was a clean successful OTA,
not a forced failure case).

Pre-existing, unrelated: boot log shows `E (277) octal_psram: PSRAM ID read
error: ... PSRAM chip not found` — same PSRAM discrepancy flagged in the
2026-07-25 entry above (config.h pinout comment says 8MB Octal, chip
reports differently). Not touched, not blocking.

OTA feature is now working end-to-end on this branch.
