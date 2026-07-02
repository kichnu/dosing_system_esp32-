# Channel Edit Lock — Implementation Plan v1.0

## Summary

A UI-level protection against accidental edits of critical channel actions
(Save, Refill, Reset Dosed). Not a server-side security mechanism — the web
session remains the actual auth layer. The lock prevents finger-slips on mobile
and unintended changes to a running aquarium schedule.

---

## Decisions & Scope

| Item | Decision |
|---|---|
| Scope | Global — one unlock covers all channels simultaneously |
| Lock state storage | JS RAM only — no FRAM changes |
| Password source | Existing `AUTH_DATA` FRAM via `verifyPassword()` |
| New FRAM sections | None |
| New API endpoint | `POST /api/verify-password` |
| Notes section | NOT locked — not safety-critical |
| Auto-lock | Inactivity timer — `setTimeout`, reset on Save/Refill/Reset Dosed |
| Timer duration | 5 minutes (configurable constant in JS: `LOCK_TIMEOUT_MS`) |
| Re-lock | Manual (topbar button) OR on timer expiry |

---

## Behavior: No Password Configured

When `areCredentialsLoaded() == false`:
- `/api/verify-password` returns `{success: false, no_password: true}`
- Lock button in topbar is hidden (no lock available)
- Save/Refill/Reset Dosed always enabled
- No visual lock UI shown at all

This mirrors existing behavior: web login returns 503 when no credentials.

---

## UI: Lock Button in card-topbar

Location: `card-topbar`, between logo-text and card-time.
Rendered once per card, same across all channels (global state).

### States

**LOCKED (default on page load if password configured):**
```
[ 🔒  EDIT LOCKED ]   ← muted color (--text-muted), border: --border
```
Clicking → expands inline password form below topbar.

**UNLOCKED:**
```
[ 🔓  EDITING ACTIVE · 4:32 ]   ← accent-green, shows countdown
```
Clicking → immediately re-locks, clears timer.

**No password:**
Lock button not rendered.

### Inline Password Form (expands below topbar)

Appears as a slim bar between topbar and channel-nav when lock button clicked.
Contains:
```
[ ••••••••••••••••  ] [ Unlock ]   [ × ]
  input[type=password]
  autocomplete="current-password"
  name="password"
  (inside <form> for password manager detection)
```

On wrong password: form shakes + brief red border, input cleared.
On correct password: form collapses, state → UNLOCKED, timer starts.
`Escape` or `×` → collapses form without change.

---

## Locked Buttons: Visual State

When locked, Save / Refill / Reset Dosed get:
- `disabled` attribute (browser-native: not clickable, reduced opacity)
- `title="Edit locked — click 🔒 to unlock"` (tooltip)
- Small lock icon prepended to button label

The rest of the interface (checkboxes, sliders, notes, nav) is UNCHANGED.
User can still read all values, switch channels, expand notes.

---

## Inactivity Timer

```javascript
const LOCK_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
let lockTimer = null;

function resetLockTimer() {
    clearTimeout(lockTimer);
    lockTimer = setTimeout(() => setLocked(true), LOCK_TIMEOUT_MS);
}
```

`resetLockTimer()` called after each successful:
- Save (confirmSave)
- Refill (confirmRefill)
- Reset Dosed (confirmResetDosed)

On timer expiry → `setLocked(true)` → topbar button updates, buttons disabled.

---

## API: POST /api/verify-password

**Request:**
```json
{ "password": "plain_text_password" }
```

**Response (success):**
```json
{ "success": true }
```

**Response (wrong password):**
```json
{ "success": false }
```

**Response (no credentials loaded):**
```json
{ "success": false, "no_password": true }
```

Implementation: calls existing `verifyPassword(password)` from `auth_manager.cpp`.
No session created, no cookie set. Purely a verification call.
Rate limiting: reuse existing `rateLimiter` (already applied to `/api/login`).

---

## Reset Dosed Modal

Same pattern as Save and Refill. Additions:
- JS: `pendingResetDosedChannel = -1`
- JS: `showResetDosedModal(idx)` / `closeResetDosedModal()` / `confirmResetDosed()`
- HTML: modal with "Reset Dosed?" title, channel info, Cancel + Reset buttons
- `confirmResetDosed()` calls existing `resetDosed()` logic + `resetLockTimer()`

---

## Files to Modify

| File | Changes |
|---|---|
| `src/web/web_server.cpp` | Add `handleApiVerifyPassword()`, register route |
| `src/web/html_pages.cpp` | Lock button HTML in topbar, password form, modal for Reset Dosed, JS state machine, timer |

No changes to: FRAM layout, dosing_types.h, fram_controller, provisioning.

---

## JS State Machine

```
Page load
    │
    ├─ areCredentialsAvailable? ──No──► no lock UI, buttons always enabled
    │
    Yes
    │
    ▼
LOCKED (default)
    │  click 🔒
    ▼
FORM OPEN
    │  correct password     │  wrong / Escape / ×
    ▼                       ▼
UNLOCKED ◄──────────────── LOCKED
    │
    ├── timer expires (5 min inactivity)
    ├── user clicks 🔓
    └── page reload
    │
    ▼
LOCKED
```

---

## Open Questions (for next iteration)

- Timer countdown display: full `MM:SS` or just minutes?
- Should wrong password attempts be counted/throttled client-side?
- Lock icon on disabled buttons: prepend SVG or CSS overlay?
