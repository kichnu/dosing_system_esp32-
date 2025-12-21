# VPS Proxy Session Expiry - Security Fix

## Problem

When ESP32 dashboard is displayed through VPS reverse proxy (Nginx), JavaScript polling continues even after VPS session expires. This causes:

1. Every polling request triggers `auth_request` → 401 → redirect to `/login`
2. `/login` page has rate limiting (e.g., 15 requests per 15 minutes)
3. Polling every 2-10 seconds exhausts rate limit within minutes
4. User sees "Too Many Requests" instead of proper "Session Expired" message

## Architecture Context

```
[Browser JS Polling] → [Nginx] → auth_request → [Flask session check]
                                      ↓
                               401 (expired)
                                      ↓
                               redirect /login (×20/min)
                                      ↓
                               RATE LIMIT EXCEEDED
```

## Solution Components

### 1. VPS Side - Nginx auth_request

Protect proxy routes with session validation:

```nginx
location = /api/auth-check {
    internal;
    proxy_pass http://127.0.0.1:5001/api/auth-check;
    proxy_set_header Cookie $http_cookie;
    proxy_pass_request_body off;
    proxy_set_header Content-Length "";
}

location /device/DEVICE_NAME/ {
    auth_request /api/auth-check;
    error_page 401 = @login_redirect;
    proxy_pass http://DEVICE_LAN_IP/;
    # ... other proxy settings
}

location @login_redirect {
    return 302 /login?next=$request_uri;
}
```

### 2. VPS Side - Flask auth endpoint

```python
@app.route('/api/auth-check')
@limiter.exempt  # Critical: exclude from rate limiting
def auth_check():
    if validate_session():
        return '', 200
    return '', 401
```

### 3. ESP32 Side - JavaScript Session Handler

Add at the beginning of `<script>` section:

```javascript
let sessionExpired = false;
let pollingIntervals = [];

function handleSessionExpired() {
    if (sessionExpired) return;
    sessionExpired = true;

    // Stop all polling
    pollingIntervals.forEach(id => clearInterval(id));
    pollingIntervals = [];

    // Show user-friendly overlay
    const overlay = document.createElement('div');
    overlay.innerHTML = `
        <div style="
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.85); z-index: 9999;
            display: flex; justify-content: center; align-items: center;
        ">
            <div style="
                background: white; padding: 40px; border-radius: 12px;
                text-align: center; max-width: 400px; margin: 20px;
            ">
                <h2 style="color: #e74c3c;">Session Expired</h2>
                <p style="color: #666; margin: 15px 0;">Please log in again.</p>
                <a href="/login" style="
                    display: inline-block; padding: 12px 30px;
                    background: #3498db; color: white;
                    text-decoration: none; border-radius: 8px;
                ">Back to Login</a>
            </div>
        </div>
    `;
    document.body.appendChild(overlay);
}

async function secureFetch(url, options = {}) {
    if (sessionExpired) return null;

    try {
        const response = await fetch(url, options);
        
        if (response.status === 401 || 
            (response.redirected && response.url.includes('/login'))) {
            handleSessionExpired();
            return null;
        }
        return response;
    } catch (error) {
        console.error('Fetch error:', error);
        return null;
    }
}
```

### 4. ESP32 Side - Modify Polling Functions

Replace all polling `fetch()` calls with `secureFetch()`:

```javascript
// BEFORE
function loadData() {
    fetch("api/endpoint")
        .then((response) => response.json())
        .then((data) => {
            if (data.success) {
                // process data
            }
        })
        .catch((error) => console.error(error));
}

// AFTER
function loadData() {
    secureFetch("api/endpoint")
        .then((response) => {
            if (!response) return null;
            return response.json();
        })
        .then((data) => {
            if (!data) return;
            if (data.success) {
                // process data
            }
        })
        .catch((error) => console.error(error));
}
```

### 5. ESP32 Side - Register Intervals for Cleanup

```javascript
// BEFORE
setInterval(updateStatus, 2000);
setInterval(loadVolume, 10000);

// AFTER
pollingIntervals.push(setInterval(updateStatus, 2000));
pollingIntervals.push(setInterval(loadVolume, 10000));
```

## Key Points

| Component | Action | Why |
|-----------|--------|-----|
| `/api/auth-check` | `@limiter.exempt` | Called on every proxied request |
| `secureFetch()` | Check for 401/redirect | Detect session expiry |
| `handleSessionExpired()` | Stop all intervals | Prevent rate limit exhaustion |
| Overlay message | User-friendly | Better UX than "Too Many Requests" |

## Testing Checklist

1. ✅ Login through VPS → access ESP32 dashboard
2. ✅ Wait for session timeout (or logout in another tab)
3. ✅ Verify overlay appears (not rate limit error)
4. ✅ Verify no further requests in browser Network tab
5. ✅ Verify "Back to Login" link works
6. ✅ Test direct LAN access still works independently

## Common Mistakes

- Forgetting `@limiter.exempt` on auth-check endpoint
- Having two `.then((data) =>` blocks instead of one combined
- Not registering dynamically created intervals (e.g., pump monitoring)
- Not checking `if (!response) return null` before `.json()`