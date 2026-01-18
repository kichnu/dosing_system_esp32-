# Session: Daily Log Removal & Code Cleanup

**Date:** 2026-01-17
**Status:** In Progress

## Context

Race conditions causing crashes. AsyncWebServer + FRAM operations conflicting. Decision to simplify instead of adding synchronization.

## Discussion Summary

**Problem:** Daily log ring buffer operations during web requests cause system crashes.

**Options considered:**
1. Mutex on I2C bus - proper fix but requires refactoring all I2C access points
2. Request queue - deferred HTTP responses, more complexity
3. Disable web server during dosing - poor UX, workaround not fix
4. **Remove daily log entirely** - eliminates problem source, simplifies code

**Decision:** Remove daily log + prepare for static GUI approach.

## Planned Changes

### Phase 1: Remove Daily Log (current)
- Delete: `daily_log.h`, `daily_log.cpp`, `daily_log_types.h`
- Modify 9 files to remove all references
- Free ~19KB FRAM, ~200B RAM

### Phase 2: Clean debug logs
- Remove excessive Serial.print statements
- Keep only essential runtime logs

### Phase 3: Add total dosed volume
- Per-channel cumulative counter
- GUI reset button
- GUI visual improvements

## Files to Remove
- `src/config/daily_log.h`
- `src/config/daily_log.cpp`
- `src/config/daily_log_types.h`

## Files to Modify
- `src/main.cpp`
- `src/web/web_server.cpp`
- `src/web/html_pages.cpp`
- `src/hardware/dosing_scheduler.cpp`
- `src/hardware/rtc_controller.cpp`
- `src/hardware/safety_manager.cpp`
- `src/cli/cli_commands.cpp`
- `src/cli/cli_tests.cpp`
- `src/config/fram_layout.h`
- `src/config/dosing_types.h`

## Progress
- [x] Analysis complete
- [x] Plan approved
- [x] Delete daily log files (daily_log.h, daily_log.cpp, daily_log_types.h)
- [x] Clean fram_layout.h (removed Daily Log sections, freed ~30KB FRAM)
- [x] Clean dosing_scheduler.cpp (removed commented g_dailyLog code)
- [x] Clean cli_tests.cpp (removed Daily Log dump tests)
- [x] Clean dosing_types.h (removed DailyLogEntry struct)
- [x] Test build - SUCCESS (2026-01-18)
- [ ] Test runtime on device
- [ ] Commit changes

## Next Steps (Phase 2)
- Address race conditions in AsyncWebServer
- Review mutex/synchronization for I2C bus
