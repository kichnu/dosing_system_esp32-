# GUI Improvements Plan

**Date:** 2026-01-18
**Status:** IMPLEMENTED

## Overview

Two main areas of GUI changes before starting race condition testing/fixes.

---

## 1. Channel Configuration UI Changes

### 1.1 Remove Cancel Button
- Delete "Cancel" button from card footer
- Footer will contain only Save button (moved to Dosing Volume section)

### 1.2 Move Save Button to Dosing Volume Section
- Remove "Today" calc-item from Dosing Volume section (info already visible in slot coloring)
- Place Save button in its place within calc-grid
- Button styling: same as current `btn-primary` (cyan/blue gradient)

### 1.3 Save Confirmation Modal
- Add confirmation modal before saving (like logout modal)
- Title: "Save changes?"
- Text: "Configuration will be active from tomorrow."
- Buttons: "Cancel" / "Save"
- On confirm: execute save API call
- On cancel: close modal, no action

---

## 2. Container Volume Section Redesign

### 2.1 Remaining Volume Bargraph Redesign
**Current:** Bargraph with percentage text overlay + separate "Remaining" calc-item

**New:**
- Remove percentage text from bargraph center
- Remove separate "Remaining" calc-item
- Add label below/beside bargraph: `remaining XXX ml`
- Bargraph becomes self-contained visual control

### 2.2 New Feature: "Dosed Since Reset" Tracker

**Purpose:** Track cumulative dosed volume since last manual reset

**Data structure (FRAM):**
```c
// Add to ContainerVolume or create new structure
struct DosedTracker {
    float total_dosed_ml;     // Cumulative sum since reset
    uint32_t crc;             // CRC32 validation
} __attribute__((packed));    // 8 bytes per channel
```

**Backend changes:**
- Add field to FRAM layout (find suitable address)
- Increment `total_dosed_ml` after each successful dose
- Add API endpoint: `POST /api/reset-dosed?channel=X`
- Include in `/api/dosing-status` response: `totalDosedMl`

**GUI display:**
- New bargraph in Container Volume section
- 100% = weekly dose value (`calc.weekly_dose_ml`)
- Fill shows: `(total_dosed_ml / weekly) * 100`, capped at 100%
- Label: "Dosed since reset: XXX ml" (shows actual value, not capped)
- Overflow handling: bargraph stops at 100%, numeric value shows true total

**Reset button:**
- Label: "Reset"
- Style: same as Save button (`btn-primary` - cyan/blue gradient)
- No confirmation modal required
- API call: `POST /api/reset-dosed?channel=X`

### 2.3 Refill Button Style Change
- Change from green (`accent-green`) to match Save button style
- Use `btn-primary` (cyan/blue gradient)
- Green color reserved only for "configuration valid" indicator

### 2.4 New Section Layout

```
┌─────────────────────────────────────────────┐
│ CONTAINER VOLUME                            │
├─────────────────────────────────────────────┤
│ Container Size: [____1000____] ml           │
│                                             │
│ ████████████████░░░░░░░░  remaining 750 ml  │
│                                             │
│ ██████████░░░░░░░░░░░░░░  dosed 45.5 ml     │
│ Dosed since reset         [Reset]           │
│                                             │
│ ┌─────────────┐                             │
│ │  Days Left  │                             │
│ │    12.5     │                             │
│ └─────────────┘                             │
│                                             │
│ [      Refill Container      ]              │
└─────────────────────────────────────────────┘
```

---

## 3. Files to Modify

### Frontend (HTML/JS)
- `src/web/html_pages.cpp` - Dashboard HTML and JavaScript

### Backend (C++)
- `src/config/fram_layout.h` - Add FRAM address for DosedTracker
- `src/config/dosing_types.h` - Add DosedTracker struct (or extend ContainerVolume)
- `src/algorithm/channel_manager.h/.cpp` - Add dosed tracking methods
- `src/web/web_server.cpp` - Add reset-dosed API endpoint, extend dosing-status response

---

## 4. Implementation Order

1. **Backend first:**
   - Define FRAM structure for dosed tracker
   - Add ChannelManager methods: `addDosedVolume()`, `resetDosedVolume()`, `getTotalDosed()`
   - Add API endpoint `/api/reset-dosed`
   - Extend `/api/dosing-status` with `totalDosedMl`

2. **Frontend second:**
   - Modify Dosing Volume section (remove Today, add Save button)
   - Remove Cancel button from footer
   - Add save confirmation modal
   - Redesign Container Volume section with new layout
   - Add dosed-since-reset bargraph and Reset button
   - Change Refill button styling

---

## 5. Constants / Limits

- Dosed tracker: no upper limit (float, can exceed weekly)
- Bargraph display: capped at 100%
- Reset: immediate, no confirmation

---

## 6. Notes

- All changes should maintain existing FRAM compatibility where possible
- Green color (`accent-green`) now exclusively for "configuration valid" state
- Modal pattern already exists for logout and refill - reuse for save confirmation

---

## 7. Implementation Summary (Completed)

### Modified Files:
- `src/config/dosing_types.h` - Added `DosedTracker` struct (8 bytes)
- `src/config/fram_layout.h` - Added FRAM addresses for DosedTracker at 0x0760, version bump to 7
- `src/hardware/fram_controller.h/.cpp` - Added FRAM read/write/reset methods for DosedTracker
- `src/algorithm/channel_manager.h/.cpp` - Added dosed tracking methods, integrated with markEventCompleted
- `src/web/web_server.cpp` - Added `/api/reset-dosed` endpoint, extended dosing-status with `totalDosedMl`
- `src/web/html_pages.cpp` - Complete GUI redesign:
  - Removed "Today" from Dosing Volume
  - Moved Save button to Dosing Volume section
  - Removed footer (Cancel button deleted)
  - Added save confirmation modal
  - Redesigned Container Volume with new bargraph layout
  - Added "Dosed since reset" bargraph with Reset button
  - Changed Refill button styling to cyan/blue (no longer green)

### Build Status:
- Compilation successful (debug environment)
- RAM: 14.9% used
- Flash: 37.3% used
