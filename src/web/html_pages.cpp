#include "html_pages.h"

// ============================================================================
// LOGIN PAGE
// ============================================================================
const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DOZOWNIK - Login</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-input: #1e293b;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --radius: 12px;
            --radius-sm: 8px;
        }

        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .login-box {
            background: var(--bg-card);
            border: 1px solid var(--border);
            padding: 40px;
            border-radius: var(--radius);
            box-shadow: 0 4px 24px rgba(0,0,0,0.4);
            width: 100%;
            max-width: 400px;
            margin: 20px;
            position: relative;
            z-index: 1;
        }

        .logo {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 12px;
            margin-bottom: 30px;
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border-radius: var(--radius-sm);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .logo-icon svg { width: 24px; height: 24px; fill: var(--bg-primary); }

        h1 { font-size: 1.25rem; font-weight: 700; }
        h1 span { color: var(--text-muted); font-weight: 500; }

        .form-group { margin-bottom: 20px; }

        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--text-secondary);
            font-size: 0.875rem;
        }

        input[type="password"] {
            width: 100%;
            padding: 12px 16px;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            font-size: 1rem;
            color: var(--text-primary);
        }

        input[type="password"]:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .login-btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            color: var(--bg-primary);
            border: none;
            border-radius: var(--radius-sm);
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }

        .login-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(56, 189, 248, 0.3);
        }

        .alert {
            padding: 12px 16px;
            margin: 15px 0;
            border-radius: var(--radius-sm);
            display: none;
            font-size: 0.875rem;
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid rgba(239, 68, 68, 0.3);
            color: var(--accent-red);
        }

        .info {
            margin-top: 24px;
            padding: 16px;
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            font-size: 0.75rem;
            color: var(--text-muted);
        }

        .info strong { color: var(--text-secondary); }
    </style>
</head>
<body>
    <div class="login-box">
        <div class="logo">
            <div class="logo-icon">
                <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
            </div>
            <h1>DOZOWNIK <span>– System</span></h1>
        </div>
        <form id="loginForm">
            <div class="form-group">
                <label for="password">Administrator Password</label>
                <input type="password" id="password" name="password" required />
            </div>
            <button type="submit" class="login-btn">Login</button>
        </form>
        <div id="error" class="alert"></div>
        <div class="info">
            <strong>Dosing System v1.0</strong><br />
            6-channel automatic fertilizer doser<br />
            Session-based authentication
        </div>
    </div>
    <script>
        document.getElementById("loginForm").addEventListener("submit", function(e) {
            e.preventDefault();
            const password = document.getElementById("password").value;
            const errorDiv = document.getElementById("error");

            fetch("api/login", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "password=" + encodeURIComponent(password),
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    window.location.href = "/";
                } else {
                    errorDiv.textContent = data.error || "Login failed";
                    errorDiv.style.display = "block";
                }
            })
            .catch(() => {
                errorDiv.textContent = "Connection error";
                errorDiv.style.display = "block";
            });
        });
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// DASHBOARD PAGE
// ============================================================================
const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, viewport-fit=cover">
    <title>DOZOWNIK - Dashboard</title>
    <style>
/* ============================================================================
   CSS VARIABLES
   ============================================================================ */
:root {
    /* Backgrounds */
    --bg-primary: #0a0f1a;
    --bg-card: #111827;
    --bg-card-hover: #1a2332;
    --bg-input: #1e293b;
    
    /* Borders */
    --border: #2d3a4f;
    --border-light: #3d4a5f;
    
    /* Text */
    --text-primary: #f1f5f9;
    --text-secondary: #94a3b8;
    --text-muted: #64748b;
    
    /* Accents */
    --accent-blue: #38bdf8;
    --accent-cyan: #22d3d5;
    --accent-green: #22c55e;
    --accent-red: #ef4444;
    --accent-orange: #f97316;
    --accent-yellow: #eab308;
    --accent-blue-run: #3b82f6;
    
    /* Channel states */
    --state-inactive: #4b5563;
    --state-incomplete: #eab308;
    --state-invalid: #ef4444;
    --state-configured: #22c55e;
    --state-pending: #38bdf8;
    
    /* Radii */
    --radius-sm: 6px;
    --radius-md: 10px;
    --radius-lg: 14px;
    
    /* Transitions */
    --transition-fast: 0.15s ease;
    --transition-normal: 0.25s ease;
    --transition-slow: 0.4s cubic-bezier(0.25, 0.46, 0.45, 0.94);
    
    /* Card sizing */
    --card-width: 600px;
    --card-padding: 12px;
    --section-gap: 10px;
    --section-padding: 10px;
    
    /* Element sizing */
    --event-slot-height: 38px;
    --day-slot-height: 36px;
    --input-height: 38px;
    --btn-height: 40px;
    --header-height: 44px;
    --footer-height: 56px;
    
    /* Font sizes */
    --font-xs: 0.6rem;
    --font-sm: 0.7rem;
    --font-md: 0.8rem;
    --font-lg: 0.95rem;
    
    /* Mobile */
    --swipe-threshold: 50px;
}

/* ============================================================================
   RESET & BASE
   ============================================================================ */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
    -webkit-tap-highlight-color: transparent;
}

html, body {
    height: 100%;
    touch-action: pan-y;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.4;
}

body::before {
    content: '';
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: 
        radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.06) 0%, transparent 50%),
        radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.04) 0%, transparent 50%);
    pointer-events: none;
    z-index: 0;
}

/* ============================================================================
   NAVIGATION - MOBILE LAYOUT
   ============================================================================ */
.app-container {
    /* position: relative; */
    display: flex;
    width: 100vw;
    min-height: 100dvh;
    /* overflow: hidden; */
    z-index: 1;
}

.vertical-slider {
    display: flex;
    /* flex-direction: column; */
    width: 1000px;
    margin: 0 auto;
    /* position: relative; */
    /*transition: transform var(--transition-slow);*/
    align-items: center; 
    height: 100dvh;
    flex-direction: row;
    transform: none;

}

/* Header Screen */
.header-screen {
    display: flex;
    background-color: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--radius-lg);
    flex-direction: column;
    justify-content: start;
    /* padding: 12px; */
    margin: 10px;
    width: 320px;
    position: absolute;
    left: 0;
    top: 0;
    z-index: 20;
    transition: transform var(--transition-slow);
}

.header-screen.hidden {
    transform: translateX(-100%);
    transform: translateX(calc(-100% - 10px));

}

.header-wraper{
    padding: 12px;
}


.header-bar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 10px var(--card-padding);
    margin-top: 36px;
    background: var(--bg-card);
    border-bottom: 1px solid var(--border);
    height: var(--header-height);
    flex-shrink: 0;
}

.header-toggle {
    position: absolute;
    right: -50px;
    top: 0px;
  
 
    width: 50px;
    height: 100px;
    background: transparent;

    border: none;

    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 25;
    transition: left var(--transition-slow);
}

.header-toggle svg {
    width: 50px;
    height: 100px;
    color: var(--text-secondary);
    transition: transform var(--transition-fast);
    transform: rotate(180deg);
}

.header-toggle.shifted {
    left: 320px;  
}

.header-toggle.shifted svg {
    transform: rotate(0deg);
}

.logo {
    display: flex;
    align-items: center;
    gap: 8px;
}

.logo-icon {
    width: 32px;
    height: 32px;
    background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
    border-radius: var(--radius-sm);
    display: flex;
    align-items: center;
    justify-content: center;
}

.logo-icon svg {
    width: 18px;
    height: 18px;
    fill: var(--bg-primary);
}

.logo-text {
    margin-left: 10px;
    font-size: 1rem;
    font-weight: 700;
    margin-top: 3px;
}

.header-actions {
    display: flex;
    gap: 6px;
}

.btn-icon {
    width: 32px;
    height: 32px;
    border-radius: var(--radius-sm);
    border: 1px solid var(--border);
    background: var(--bg-input);
    color: var(--text-secondary);
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    transition: all var(--transition-fast);
}

.btn-icon:hover {
    background: var(--bg-card-hover);
    color: var(--text-primary);
}

.btn-icon svg {
    width: 16px;
    height: 16px;
}

.system-status {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--radius-md);
    padding: 12px;
    margin-bottom: 12px;
    flex-shrink: 0;
}

.status-header {
    display: flex;
    align-items: center;
    gap: 6px;
    margin-bottom: 10px;
    font-size: var(--font-sm);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-secondary);
}

.status-header svg {
    width: 12px;
    height: 12px;
}

.status-grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
}

.status-item {
    background: var(--bg-input);
    border-radius: var(--radius-sm);
    padding: 8px;
    text-align: center;
}

.status-item .label {
    font-size: var(--font-xs);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 2px;
}

.status-item .value {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-md);
    font-weight: 600;
}

.status-item.ok .value { color: var(--accent-green); }
.status-item.warning .value { color: var(--accent-yellow); }
.status-item.error .value { color: var(--accent-red); }

/* Channels Overview */
.channels-overview {
    flex: 1;
    overflow-y: auto;
}

.overview-title {
    font-size: var(--font-sm);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-secondary);
    margin-bottom: 10px;
}

.overview-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 8px;
}

.channel-preview {
    background: var(--bg-card);
    border: 2px solid var(--border);
    border-radius: var(--radius-md);
    padding: 12px;
    cursor: pointer;
    transition: all var(--transition-fast);
    position: relative;
}

.channel-preview.active{
    background: rgba(56, 189, 248, 0.1);
}

.channel-preview:hover {
    border-color: var(--border-light);
    transform: translateY(-2px);
}

.channel-preview:active {
    transform: translateY(0);
}

.channel-preview.inactive { opacity: 0.5; border-color: var(--state-inactive); }
.channel-preview.incomplete { border-color: var(--state-incomplete); }
.channel-preview.invalid { border-color: var(--state-invalid); }
.channel-preview.configured { border-color: var(--state-configured); }
.channel-preview.pending { border-color: var(--state-pending); }

.channel-preview .ch-num {
    font-size: var(--font-xs);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.channel-preview .ch-dose {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-lg);
    font-weight: 700;
    color: var(--text-primary);
    margin-bottom: 2px;
}

.channel-preview.inactive .ch-dose { color: var(--text-muted); }

.channel-preview .ch-info {
    font-size: var(--font-xs);
    color: var(--text-muted);
}

.channel-preview .state-dot {
    position: absolute;
    top: 8px;
    right: 8px;
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--state-inactive);
}

.channel-preview.incomplete .state-dot { background: var(--state-incomplete); }
.channel-preview.invalid .state-dot { background: var(--state-invalid); }
.channel-preview.configured .state-dot { background: var(--state-configured); animation: pulse 2s infinite; }
.channel-preview.pending .state-dot { background: var(--state-pending); animation: pulse 1.5s infinite; }

@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
}

@keyframes bounce {
    0%, 100% { transform: translateY(0); }
    50% { transform: translateY(4px); }
}

.channels-screen {
    width: 100%;
    height: 100%;
    display: flex;
    align-items: center;
    justify-content: center;
}

.dupa{

    position: relative;
    position: static;
    position: absolut;
    left: 100px;
}

.channel-dots {
    display: flex;
    justify-content: center;
    display: flex;
    gap: 6px;
    z-index: 10;
    padding: 10px 0; 
    width: 100%;
}

.channel-dot {
    width: 26px;
    height: 10px;
    background: var(--border);
    cursor: pointer;
    transition: all var(--transition-fast);
    border-radius: 3px;
}

.channel-dot:hover { background: var(--text-muted); }

.channel-dot.active {
    background: var(--accent-cyan);
}

/* ============================================================================
   CHANNEL CARD
   ============================================================================ */
.channel-card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--radius-lg);
    position: absolute;
    top: 0;
    left: 50%;
    transform: translateX(-50%);
    width: var(--card-width);
    max-height: calc(100% - 20px);
    overflow-y: auto;
    opacity: 0;
    margin: 10px;
    pointer-events: none;
    transition: opacity 0.3s ease, transform 0.3s ease;
}
.channel-card {
    overflow-y: auto;
    scrollbar-width: thin;
    scrollbar-color: rgba(255,255,255,0.4) transparent;
}

.channel-card::-webkit-scrollbar {
    width: 16px;
}

.channel-card::-webkit-scrollbar-thumb {
    background-color: rgba(255,255,255,0.25);
    border-radius: 6px;
}
.channel-card.active {
    opacity: 1;
    pointer-events: auto;
}

.card-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 10px var(--card-padding);
    margin-top: 36px;
    background: var(--bg-card);
    border-bottom: 1px solid var(--border);
    height: var(--header-height);
    flex-shrink: 0;
}

.channel-title {
    display: flex;
    align-items: center;
    gap: 8px;
}

.channel-number {
    margin-left: 40px;
    margin-right: 10px;

    font-size: 22px;
    font-weight: 700;
}

.state-badge {
    padding: 3px 8px;
    border-radius: 10px;
    font-size: var(--font-xs);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
}

.state-badge.inactive { background: rgba(75, 85, 99, 0.2); color: var(--state-inactive); }
.state-badge.incomplete { background: rgba(234, 179, 8, 0.15); color: var(--state-incomplete); }
.state-badge.invalid { background: rgba(239, 68, 68, 0.15); color: var(--state-invalid); }
.state-badge.configured { background: rgba(34, 197, 94, 0.15); color: var(--state-configured); }
.state-badge.pending { background: rgba(56, 189, 248, 0.15); color: var(--state-pending); }

// .back-btn {
//     display: flex;
//     align-items: center;
//     gap: 4px;
//     padding: 6px 10px;
//     background: var(--bg-input);
//     border: 1px solid var(--border);
//     border-radius: var(--radius-sm);
//     color: var(--text-secondary);
//     font-size: var(--font-sm);
//     font-weight: 500;
//     cursor: pointer;
//     transition: all var(--transition-fast);
// }

// .back-btn:hover {
//     background: var(--bg-card-hover);
//     color: var(--text-primary);
// }

// .back-btn svg {
//     width: 12px;
//     height: 12px;
// }

.card-content {
    flex: 1;
    padding: var(--card-padding);
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: var(--section-gap);
    overflow-y: auto;
}

.section {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--radius-md);
    overflow: hidden;
    flex-shrink: 0;
}

.section-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px var(--section-padding);
    background: rgba(0,0,0,0.2);
    border-bottom: 1px solid var(--border);
}

.section-title {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: var(--font-sm);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-secondary);
}

.section-title svg {
    width: 12px;
    height: 12px;
    color: var(--accent-cyan);
}

.section-info {
    font-size: var(--font-xs);
    color: var(--text-muted);
}

.section-body {
    padding: var(--section-padding);
}

.events-grid {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
    gap: 4px;
}

.event-slot { position: relative; }

.event-cb { display: none; }

.event-lbl {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: var(--event-slot-height);
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    cursor: pointer;
    transition: all var(--transition-fast);
}

.event-lbl:hover {
    border-color: var(--accent-cyan);
    background: var(--bg-card-hover);
}

.event-cb:checked + .event-lbl {
    background: rgba(34, 211, 213, 0.15);
    border-color: var(--accent-cyan);
}

.event-time {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-xs);
    font-weight: 600;
    color: var(--text-secondary);
}

.event-cb:checked + .event-lbl .event-time { color: var(--accent-cyan); }

.event-dot {
    width: 4px;
    height: 4px;
    border-radius: 50%;
    margin-top: 3px;
    background: transparent;
}

.event-cb:checked + .event-lbl .event-dot { background: var(--accent-cyan); }

.event-slot.done .event-lbl {
    background: rgba(34, 197, 94, 0.15);
    border-color: var(--accent-green);
}
.event-slot.done .event-time { color: var(--accent-green); }
.event-slot.done .event-dot { background: var(--accent-green); }

.event-slot.failed .event-lbl {
    background: rgba(239, 68, 68, 0.15);
    border-color: #ef4444;
}
.event-slot.failed .event-time { color: #ef4444; }
.event-slot.failed .event-dot { background: #ef4444; }

.event-slot.next .event-lbl {
    background: rgba(234, 179, 8, 0.15);
    border-color: var(--accent-yellow);
    animation: pulse-border 2s infinite;
}
.event-slot.next .event-time { color: var(--accent-yellow); }
.event-slot.running .event-lbl {
    background: rgba(59, 130, 246, 0.2);
    border-color: var(--accent-blue-run);
    animation: running-pulse 1s infinite;
}
.event-slot.running .event-time { color: var(--accent-blue-run); }
.event-slot.running .event-dot { background: var(--accent-blue-run); }

@keyframes running-pulse {
    0%, 100% { box-shadow: 0 0 0 0 rgba(59, 130, 246, 0.5); }
    50% { box-shadow: 0 0 8px 2px rgba(59, 130, 246, 0.3); }
}

@keyframes pulse-border {
    0%, 100% { box-shadow: 0 0 0 0 rgba(234, 179, 8, 0.4); }
    50% { box-shadow: 0 0 0 2px rgba(234, 179, 8, 0.1); }
}

.days-grid {
    display: grid;
    grid-template-columns: repeat(7, 1fr);
    gap: 4px;
}

.day-slot { position: relative; }

.day-cb { display: none; }

.day-lbl {
    display: flex;
    align-items: center;
    justify-content: center;
    height: var(--day-slot-height);
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    cursor: pointer;
    transition: all var(--transition-fast);
}

.day-lbl:hover {
    border-color: var(--accent-cyan);
    background: var(--bg-card-hover);
}

.day-cb:checked + .day-lbl {
    background: rgba(34, 211, 213, 0.15);
    border-color: var(--accent-cyan);
}

.day-name {
    font-size: var(--font-sm);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    color: var(--text-secondary);
}

.day-cb:checked + .day-lbl .day-name { color: var(--accent-cyan); }

.day-slot.today .day-lbl { border-width: 2px; }
.day-slot.today .day-name::after {
    content: '';
    display: inline-block;
    width: 4px;
    height: 4px;
    background: var(--accent-yellow);
    border-radius: 50%;
    margin-left: 3px;
    vertical-align: middle;
}

.volume-row {
    display: flex;
    align-items: flex-end;
    gap: 8px;
    margin-bottom: 10px;
}

.volume-group { flex: 1; }

.volume-label {
    display: block;
    font-size: var(--font-xs);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.volume-input {
    width: 100%;
    height: var(--input-height);
    padding: 0 10px;
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-lg);
    font-weight: 600;
    color: var(--text-primary);
    text-align: center;
}

.volume-input:focus {
    outline: none;
    border-color: var(--accent-cyan);
    box-shadow: 0 0 0 2px rgba(34, 211, 213, 0.15);
}

.volume-unit {
    font-size: var(--font-sm);
    font-weight: 600;
    color: var(--text-muted);
    padding-bottom: 10px;
}

.calc-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 6px;
}

.calc-item {
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    padding: 8px;
    text-align: center;
}

.calc-lbl {
    font-size: var(--font-xs);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
    margin-bottom: 2px;
}

.calc-val {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-md);
    font-weight: 700;
    color: var(--text-primary);
}

.calc-item.hl .calc-val { color: var(--accent-cyan); }
.calc-item.ok .calc-val { color: var(--accent-green); }
.calc-item.warn .calc-val { color: var(--accent-yellow); }

.calib-row {
    display: flex;
    gap: 8px;
    align-items: flex-end;
}

.calib-btn {
    flex: 1;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    height: var(--btn-height);
    background: rgba(34, 211, 213, 0.1);
    border: 1px solid var(--accent-cyan);
    border-radius: var(--radius-sm);
    color: var(--accent-cyan);
    font-size: var(--font-sm);
    font-weight: 600;
    cursor: pointer;
    transition: all var(--transition-fast);
}

.calib-btn:hover { background: rgba(34, 211, 213, 0.2); }
.calib-btn:active { transform: scale(0.98); }

.calib-btn.running {
    background: rgba(234, 179, 8, 0.15);
    border-color: var(--accent-yellow);
    color: var(--accent-yellow);
    pointer-events: none;
}

.calib-btn svg {
    width: 14px;
    height: 14px;
}

.calib-input-group { width: 80px; }

.calib-label {
    display: block;
    font-size: var(--font-xs);
    font-weight: 600;
    text-transform: uppercase;
    color: var(--text-muted);
    margin-bottom: 4px;
}

.calib-input {
    width: 100%;
    height: var(--input-height);
    padding: 0 6px;
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-md);
    font-weight: 600;
    color: var(--text-primary);
    text-align: center;
}

.calib-input:focus {
    outline: none;
    border-color: var(--accent-cyan);
}

.valid-msg {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 8px 10px;
    border-radius: var(--radius-sm);
    font-size: var(--font-sm);
    font-weight: 500;
    flex-shrink: 0;
}

.valid-msg svg {
    width: 14px;
    height: 14px;
    flex-shrink: 0;
}

.valid-msg.err {
    background: rgba(239, 68, 68, 0.1);
    border: 1px solid rgba(239, 68, 68, 0.3);
    color: var(--accent-red);
}

.valid-msg.warn {
    background: rgba(234, 179, 8, 0.1);
    border: 1px solid rgba(234, 179, 8, 0.3);
    color: var(--accent-yellow);
}

.valid-msg.info {
    background: rgba(56, 189, 248, 0.1);
    border: 1px solid rgba(56, 189, 248, 0.3);
    color: var(--accent-blue);
}

.valid-msg.ok {
    background: rgba(34, 197, 94, 0.1);
    border: 1px solid rgba(34, 197, 94, 0.3);
    color: var(--accent-green);
}

.card-footer {
    padding: 10px var(--card-padding);
    background: var(--bg-card);
    border-top: 1px solid var(--border);
    display: flex;
    margin-bottom: 6px;
    gap: 8px;
    height: var(--footer-height);
    flex-shrink: 0;
}

.btn {
    flex: 1;
    height: var(--btn-height);
    border-radius: var(--radius-sm);
    font-size: var(--font-sm);
    font-weight: 600;
    cursor: pointer;
    transition: all var(--transition-fast);
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 5px;
}

.btn svg {
    width: 14px;
    height: 14px;
}

.btn-primary {
    background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
    border: none;
    color: var(--bg-primary);
}

.btn-primary:hover {
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(34, 211, 213, 0.3);
}

.btn-primary:disabled {
    opacity: 0.5;
    cursor: not-allowed;
    transform: none;
    box-shadow: none;
}

.btn-secondary {
    background: var(--bg-input);
    border: 1px solid var(--border);
    color: var(--text-secondary);
}

.btn-secondary:hover {
    background: var(--bg-card-hover);
    color: var(--text-primary);
}

.pending-banner {
    display: flex;
    position: absolute;
    top: 0;
    left: 0;
    align-items: center;
    border-top-left-radius: var(--radius-lg);
    border-top-right-radius: var(--radius-lg);
    gap: 6px;
    height: 32px;
    width: 100%;
    padding: 8px var(--card-padding);
    background: rgba(56, 189, 248, 0.1);
    border-bottom: 1px solid rgba(56, 189, 248, 0.3);
    font-size: var(--font-sm);
    color: var(--accent-blue);
    flex-shrink: 0;
}

.pending-banner svg {
    width: 14px;
    height: 14px;
}

.footer-info {
    position: fixed;
    bottom: 20px;
    left: 50%;
    transform: translateX(-50%);
    font-size: var(--font-xs);
    color: var(--text-muted);
    letter-spacing: 0.05em;
    opacity: 0.6;
    z-index: 5;
}

/* MEDIA################################################################################################################ */

@media (max-width: 768px) and (hover: none) {
    .channel-card {
        left: 0;
        transform: none;
        width: 100%;
        max-height: 100dvh;
        margin: 0;
        border: none;
        border-radius: 0;
    }

    .header-screen{
        width: 100vw;
        border: none;
        border-radius: 0;
        margin: 0;
        height: 100dvh;
        justify-items: start;
        

    }

    .channel-number{
        margin-left: 10px;
    }

    .card-header{
        height: auto;
    }
    .channel-title{
        margin-left: 40px;
    }

    .header-toggle{
        top: 10px;
    }

    .header-toggle.shifted {
    left: 150px;
    top: 6px;
    }
}

/* ============================================================================
   DAILY LOG SECTION
   ============================================================================ */


.daily-log-section {
    margin-top: 12px;
    padding-top: 12px;
    border-top: 1px solid var(--border);
}

.daily-log-container {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--radius-md);
    padding: 10px;
}

/* Header: Date + Day + Badge */
.dl-header {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 8px;
}

.dl-date {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-md);
    font-weight: 700;
    color: var(--text-primary);
}

.dl-day {
    font-size: var(--font-sm);
    color: var(--text-secondary);
}

.dl-badge {
    margin-left: auto;
    padding: 2px 8px;
    border-radius: 10px;
    font-size: var(--font-xs);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
}

.dl-badge.today {
    background: rgba(34, 211, 213, 0.15);
    color: var(--accent-cyan);
    border: 1px solid var(--accent-cyan);
}

.dl-badge.archive {
    background: rgba(100, 116, 139, 0.15);
    color: var(--text-muted);
    border: 1px solid var(--text-muted);
}

/* Navigation */
.dl-nav {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 10px;
    padding: 4px 0;
}

.dl-nav-btn {
    width: 32px;
    height: 28px;
    background: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: var(--radius-sm);
    color: var(--text-secondary);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: all var(--transition-fast);
}

.dl-nav-btn:hover:not(:disabled) {
    background: var(--bg-card-hover);
    color: var(--text-primary);
    border-color: var(--accent-cyan);
}

.dl-nav-btn:disabled {
    opacity: 0.3;
    cursor: not-allowed;
}

.dl-nav-btn svg {
    width: 14px;
    height: 14px;
}

.dl-nav-info {
    font-size: var(--font-xs);
    color: var(--text-muted);
}

/* Channels list */
.dl-channels {
    display: flex;
    flex-direction: column;
    gap: 6px;
    margin-bottom: 10px;
}

.dl-channel {
    background: var(--bg-input);
    border-radius: var(--radius-sm);
    padding: 6px 8px;
}

.dl-channel.inactive {
    opacity: 0.4;
}

.dl-ch-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 4px;
}

.dl-ch-name {
    font-size: var(--font-xs);
    font-weight: 600;
    color: var(--text-secondary);
}

.dl-ch-dose {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-xs);
    color: var(--text-muted);
}

.dl-ch-dose .actual {
    color: var(--accent-green);
    font-weight: 600;
}

/* Bar graph */
.dl-bar {
    height: 8px;
    background: var(--bg-primary);
    border-radius: 4px;
    overflow: hidden;
    display: flex;
}

.dl-bar-segment {
    height: 100%;
    transition: width 0.3s ease;
}

.dl-bar-segment.completed {
    background: var(--accent-green);
}

.dl-bar-segment.pending {
    background: var(--accent-blue);
}

.dl-bar-segment.failed {
    background: var(--accent-red);
}

.dl-bar-segment.inactive {
    background: var(--bg-card);
}

.dl-bar-segment:not(:last-child) {
    border-right: 1px solid var(--bg-primary);
}

/* System stats */
.dl-stats {
    display: flex;
    justify-content: space-between;
    padding-top: 8px;
    border-top: 1px solid var(--border);
}

.dl-stat {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
}

.dl-stat-icon {
    font-size: 10px;
}

.dl-stat-val {
    font-family: 'SF Mono', 'Fira Code', monospace;
    font-size: var(--font-xs);
    color: var(--text-secondary);
}


</style>
</head>
<body>
    <div class="app-container">
        <div class="header-screen">

                <button class="header-toggle shifted" id="headerToggle">
                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <polyline points="15,18 9,12 15,6"/>
                    </svg>
                </button>

                <div class="header-bar">
                 <div class="footer-info">DOZOWNIK • v1.5.0</div>
                    <div class="logo">
                        <div class="logo-icon">
                            <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
                        </div>
                        <div class="logo-text">DOZOWNIK</div>
                    </div>
                    <div class="header-actions">
                        <button class="btn-icon" title="Logout" onclick="logout()">
                            <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <path d="M9 21H5a2 2 0 01-2-2V5a2 2 0 012-2h4"/>
                                <polyline points="16,17 21,12 16,7"/>
                                <line x1="21" y1="12" x2="9" y2="12"/>
                            </svg>
                        </button>
                    </div>
                </div>

                <div class="header-wraper">
                    <div class="system-status">
                        <div class="status-header">
                            <svg fill="currentColor" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>
                            System Status
                        </div>
                        <div class="status-grid">
                            <div class="status-item ok" id="sysStatus">
                                <div class="label">System</div>
                                <div class="value">OK</div>
                            </div>
                            <div class="status-item">
                                <div class="label">Time (UTC)</div>
                                <div class="value" id="sysTime">--:--</div>
                            </div>
                            <div class="status-item" id="wifiStatus">
                                <div class="label">WiFi</div>
                                <div class="value">--</div>
                            </div>
                        </div>
                    </div>
                    
                    <div class="channels-overview">
                        <div class="overview-title">Channels</div>
                        <div class="overview-grid" id="overviewGrid"></div>

                       
                    </div>

                    <!-- DAILY LOG SECTION -->
                    <div class="daily-log-section">
                        <div class="overview-title">Dosing History</div>
                        <div class="daily-log-container" id="dailyLogContainer">
                            <!-- Header: Date + Badge -->
                            <div class="dl-header">
                                <div class="dl-date" id="dlDate">--.--.--</div>
                                <div class="dl-day" id="dlDay">---</div>
                                <div class="dl-badge today" id="dlBadge">TODAY</div>
                            </div>
                            
                            <!-- Navigation -->
                            <div class="dl-nav">
                                <button class="dl-nav-btn" id="dlPrev" onclick="dlPrevDay()">
                                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                        <polyline points="15,18 9,12 15,6"/>
                                    </svg>
                                </button>
                                <div class="dl-nav-info" id="dlNavInfo">Day 1 of 1</div>
                                <button class="dl-nav-btn" id="dlNext" onclick="dlNextDay()">
                                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                        <polyline points="9,18 15,12 9,6"/>
                                    </svg>
                                </button>
                            </div>
                            
                            <!-- Channels -->
                            <div class="dl-channels" id="dlChannels"></div>
                            
                            <!-- System Stats -->
                            <div class="dl-stats" id="dlStats">
                                <div class="dl-stat" title="Uptime">
                                    <span class="dl-stat-icon">⏱</span>
                                    <span class="dl-stat-val" id="dlUptime">--</span>
                                </div>
                                <div class="dl-stat" title="WiFi disconnects">
                                    <span class="dl-stat-icon">📶</span>
                                    <span class="dl-stat-val" id="dlWifi">--</span>
                                </div>
                                <div class="dl-stat" title="FRAM writes">
                                    <span class="dl-stat-icon">💾</span>
                                    <span class="dl-stat-val" id="dlFram">--</span>
                                </div>
                                <div class="dl-stat" title="NTP syncs">
                                    <span class="dl-stat-icon">🔄</span>
                                    <span class="dl-stat-val" id="dlNtp">--</span>
                                </div>
                                <div class="dl-stat" title="Power cycles">
                                    <span class="dl-stat-icon">⚡</span>
                                    <span class="dl-stat-val" id="dlPower">--</span>
                                </div>
                            </div>
                        </div>
                    </div>

                </div>
            </div>
            
            <div class="vertical-slider" id="verticalSlider">
            <!-- CHANNELS SCREEN -->
            <div class="channels-screen" id="channelScreen"></div>



        </div>
    </div>

<script>
// ============================================================================
// CONFIGURATION
// ============================================================================
const CFG = {
    CHANNEL_COUNT: 4,
    EVENTS_PER_DAY: 23,
    FIRST_EVENT_HOUR: 1,
    CHANNEL_OFFSET_MIN: 15,
    EVENT_WINDOW_SEC: 300,
    MAX_PUMP_SEC: 180,
    MIN_DOSE_ML: 1.0,
    CALIB_SEC: 30,
    SWIPE_THRESHOLD: 50
};

const DAYS = ['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];

// ============================================================================
// STATE
// ============================================================================
let channels = [];
let currentCh = 0;
let activeChannel = -1;
let activeEventHour = -1;
let showingChannels = false;
let touchStart = {x:0, y:0};
let editingChannel = -1;

// ============================================================================
// INIT
// ============================================================================
function init() {
    // Initialize channel data
    for (let i = 0; i < CFG.CHANNEL_COUNT; i++) {
        channels.push({
            id: i,
            events: 0,          // bitmask (bits 1-23)
            days: 0,            // bitmask (bits 0-6)
            dailyDose: 0,
            dosingRate: 0.33,   // ml/s from calibration
            eventsCompleted: 0, // bitmask of completed today
            state: 'inactive'
        });
    }
    
    // Demo data for channel 0
    channels[0] = {
        id: 0,
        events: (1<<6)|(1<<10)|(1<<14)|(1<<18),  // 06,10,14,18
        days: 0b0011111,  // Mon-Fri
        dailyDose: 15.0,
        dosingRate: 0.33,
        eventsCompleted: (1<<6)|(1<<10),
        state: 'configured'
    };
    
    channels[1] = {
        id: 1,
        events: (1<<8)|(1<<16),
        days: 0b1111111,
        dailyDose: 20.0,
        dosingRate: 0.33,
        eventsCompleted: 0,
        state: 'pending'
    };
    setupHeaderToggle();

    renderOverview();
    renderChannels();

    goToChannel(0);
   // renderDots();
    setupTouch();
    updateClock();
    setInterval(updateClock, 1000);
    
    // Load data from API
    loadStatus();
    setInterval(loadStatus, 5000);

    initDailyLog();
}

// ============================================================================
// RENDER
// ============================================================================
function renderOverview() {
    const grid = document.getElementById('overviewGrid');
    grid.innerHTML = channels.map((ch, i) => {
        const evCnt = popcount(ch.events);
        const single = evCnt > 0 ? (ch.dailyDose / evCnt).toFixed(1) : '0.0';
        const activeClass = (i === currentCh) ? 'active' : '';
        return `
            <div class="channel-preview ${ch.state} ${activeClass}" onclick="goToChannel(${i})">
                <div class="state-dot"></div>
                <div class="ch-num">CH ${i+1}</div>
                <div class="ch-dose">${ch.dailyDose > 0 ? ch.dailyDose.toFixed(1)+' ml' : 'Off'}</div>
                <div class="ch-info">${evCnt > 0 ? evCnt+'× '+single+'ml' : 'Tap to setup'}</div>
            </div>
        `;
    }).join('');
}

function renderChannels() {
    const slider = document.getElementById('channelScreen');
    slider.innerHTML = channels.map((ch, idx) => renderChannelCard(ch, idx)).join('');
    
    // Attach event listeners
    channels.forEach((ch, idx) => {
        attachCardEvents(idx);
    });

        const activeCard = document.querySelector(`.channel-card[data-ch="${currentCh}"]`);
    if (activeCard) {
        activeCard.classList.add('active');
    }
}

function getNextEventHour(ch, chIdx) {
    const now = new Date();
    const utcHour = now.getUTCHours();
    const utcMinute = now.getUTCMinutes();
    const channelOffset = chIdx * CFG.CHANNEL_OFFSET_MIN;
    
    for (let h = CFG.FIRST_EVENT_HOUR; h <= 23; h++) {
        // Czy event jest zaplanowany?
        if (!(ch.events & (1 << h))) continue;
        // Czy już wykonany?
        if (ch.eventsCompleted & (1 << h)) continue;
        
        // Czy event jeszcze nie minął?
        if (h > utcHour) {
            return h;
        }
        if (h === utcHour && channelOffset > utcMinute) {
            return h;
        }
    }
    return -1;
}

function renderChannelCard(ch, idx) {
    const now = new Date();
    const evCnt = popcount(ch.events);
    const dayCnt = popcount(ch.days);
    const single = evCnt > 0 ? ch.dailyDose / evCnt : 0;
    const pumpTime = ch.dosingRate > 0 ? single / ch.dosingRate : 0;
    const weekly = ch.dailyDose * dayCnt;
    const completedCnt = popcount(ch.eventsCompleted);
    
    const nextEvent = getNextEventHour(ch, idx);
    
    // Today's day index (0=Mon)
    const todayIdx = (now.getDay() + 6) % 7;
    
    // Validation
    let validClass = 'ok';
    let validMsg = 'Configuration valid';
    let validIcon = '<path d="M22 11.08V12a10 10 0 11-5.93-9.14"/><polyline points="22,4 12,14.01 9,11.01"/>';
    
    if (evCnt === 0) {
        validClass = 'info';
        validMsg = 'Select time slots to activate';
        validIcon = '<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>';
    } else if (dayCnt === 0) {
        validClass = 'warn';
        validMsg = 'Select active days';
        validIcon = '<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>';
    } else if (ch.dailyDose <= 0) {
        validClass = 'warn';
        validMsg = 'Enter daily dose';
        validIcon = '<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>';
    } else if (single < CFG.MIN_DOSE_ML) {
        validClass = 'err';
        validMsg = `Single dose ${single.toFixed(1)}ml < min ${CFG.MIN_DOSE_ML}ml`;
        validIcon = '<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>';
    } else if (pumpTime > CFG.MAX_PUMP_SEC) {
        validClass = 'err';
        validMsg = `Pump time ${pumpTime.toFixed(0)}s > max ${CFG.MAX_PUMP_SEC}s`;
        validIcon = '<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>';
    }

    // Events HTML
    let eventsHtml = '';
    for (let h = CFG.FIRST_EVENT_HOUR; h <= 23; h++) {
        const checked = (ch.events & (1 << h)) ? 'checked' : '';
        const done = (ch.eventsCompleted & (1 << h)) ? 'done' : '';
        const failed = (ch.eventsFailed & (1 << h)) ? 'failed' : '';
        const running = (idx === activeChannel && h === activeEventHour) ? 'running' : '';
        const next = (h === nextEvent && !running) ? 'next' : '';
        const timeStr = String(h).padStart(2,'0') + ':' + String(idx * CFG.CHANNEL_OFFSET_MIN).padStart(2,'0');
        eventsHtml += `
            <div class="event-slot ${done} ${failed} ${running} ${next}">
                <input type="checkbox" id="ev_${idx}_${h}" class="event-cb" data-ch="${idx}" data-hour="${h}" ${checked}>
                <label for="ev_${idx}_${h}" class="event-lbl">
                    <span class="event-time">${timeStr}</span>
                    <span class="event-dot"></span>
                </label>
            </div>
        `;
    }
    
    // Days HTML
    let daysHtml = '';
    DAYS.forEach((name, d) => {
        const checked = (ch.days & (1 << d)) ? 'checked' : '';
        const today = (d === todayIdx) ? 'today' : '';
        daysHtml += `
            <div class="day-slot ${today}">
                <input type="checkbox" id="day_${idx}_${d}" class="day-cb" data-ch="${idx}" data-day="${d}" ${checked}>
                <label for="day_${idx}_${d}" class="day-lbl">
                    <span class="day-name">${name}</span>
                </label>
            </div>
        `;
    });

    return `
        <div class="channel-card" data-ch="${idx}">
            ${ch.state === 'pending' ? `
            <div class="pending-banner">
                <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                    <circle cx="12" cy="12" r="10"/><polyline points="12,6 12,12 16,14"/>
                </svg>
                Changes pending – active from tomorrow
            </div>
            ` : ''}
            
            <div class="card-header">
                <div class="channel-title">
                    <span class="channel-number">Channel ${idx+1}</span>
                    <span class="state-badge ${ch.state}">${getStateLabel(ch.state)}</span>
                </div>
            </div>
            
            <div class="card-content">
                <!-- TIME SCHEDULE -->
                <div class="section">
                    <div class="section-header">
                        <div class="section-title">
                            <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <circle cx="12" cy="12" r="10"/><polyline points="12,6 12,12 16,14"/>
                            </svg>
                            Time Schedule (UTC)
                        </div>
                        <div class="section-info" id="evInfo_${idx}">${evCnt} of 23</div>
                    </div>
                    <div class="section-body">
                        <div class="events-grid">${eventsHtml}</div>
                    </div>
                </div>
                
                <!-- ACTIVE DAYS -->
                <div class="section">
                    <div class="section-header">
                        <div class="section-title">
                            <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <rect x="3" y="4" width="18" height="18" rx="2" ry="2"/>
                                <line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/>
                                <line x1="3" y1="10" x2="21" y2="10"/>
                            </svg>
                            Active Days
                        </div>
                        <div class="section-info" id="dayInfo_${idx}">${dayCnt} of 7</div>
                    </div>
                    <div class="section-body">
                        <div class="days-grid">${daysHtml}</div>
                    </div>
                </div>
                
                <!-- VOLUME -->
                <div class="section">
                    <div class="section-header">
                        <div class="section-title">
                            <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <path d="M12 2L2 7l10 5 10-5-10-5z"/>
                                <path d="M2 17l10 5 10-5"/><path d="M2 12l10 5 10-5"/>
                            </svg>
                            Dosing Volume
                        </div>
                    </div>
                    <div class="section-body">
                        <div class="volume-row">
                            <div class="volume-group">
                                <label class="volume-label">Daily Dose</label>
                                <input type="number" class="volume-input" id="dose_${idx}" 
                                       value="${ch.dailyDose}" step="0.1" min="0" data-ch="${idx}">
                            </div>
                            <span class="volume-unit">ml/day</span>
                        </div>
                        <div class="calc-grid">
                            <div class="calc-item hl">
                                <div class="calc-lbl">Single Dose</div>
                                <div class="calc-val" id="single_${idx}">${single.toFixed(1)} ml</div>
                            </div>
                            <div class="calc-item">
                                <div class="calc-lbl">Pump Time</div>
                                <div class="calc-val" id="pumpTime_${idx}">${pumpTime.toFixed(1)} s</div>
                            </div>
                            <div class="calc-item">
                                <div class="calc-lbl">Weekly</div>
                                <div class="calc-val" id="weekly_${idx}">${weekly.toFixed(1)} ml</div>
                            </div>
                            <div class="calc-item ok">
                                <div class="calc-lbl">Today</div>
                                <div class="calc-val" id="today_${idx}">${completedCnt} / ${evCnt}</div>
                            </div>
                        </div>
                    </div>
                </div>
                
                <!-- CALIBRATION -->
                <div class="section">
                    <div class="section-header">
                        <div class="section-title">
                            <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                <path d="M14.7 6.3a1 1 0 000 1.4l1.6 1.6a1 1 0 001.4 0l3.77-3.77a6 6 0 01-7.94 7.94l-6.91 6.91a2.12 2.12 0 01-3-3l6.91-6.91a6 6 0 017.94-7.94l-3.76 3.76z"/>
                            </svg>
                            Calibration
                        </div>
                    </div>
                    <div class="section-body">
                        <div class="calib-row">
                            <button class="calib-btn" id="calibBtn_${idx}" onclick="runCalib(${idx})">
                                <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                                    <polygon points="5,3 19,12 5,21 5,3"/>
                                </svg>
                                Run Pump (30s)
                            </button>
                            <div class="calib-input-group">
                                <label class="calib-label">Measured</label>
                                <input type="number" class="calib-input" id="calibMl_${idx}" 
                                       placeholder="ml" step="0.1" data-ch="${idx}">
                            </div>
                        </div>
                    </div>
                </div>
                
                <!-- VALIDATION -->
                <div class="valid-msg ${validClass}" id="validMsg_${idx}">
                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">${validIcon}</svg>
                    <span id="validTxt_${idx}">${validMsg}</span>
                </div>
            </div>
            
            <div class="card-footer">
                <button class="btn btn-secondary" onclick="cancelChanges(${idx})">
                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>
                    </svg>
                    Cancel
                </button>
                <button class="btn btn-primary" id="saveBtn_${idx}" onclick="saveChanges(${idx})">
                    <svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                        <path d="M19 21H5a2 2 0 01-2-2V5a2 2 0 012-2h11l5 5v11a2 2 0 01-2 2z"/>
                        <polyline points="17,21 17,13 7,13 7,21"/><polyline points="7,3 7,8 15,8"/>
                    </svg>
                    Save
                </button>
            </div>
        </div>
    `;
}

function attachCardEvents(idx) {
    // Mark as editing on any interaction
    const markEditing = () => { editingChannel = idx; };
    
    // Event checkboxes
    document.querySelectorAll(`.event-cb[data-ch="${idx}"]`).forEach(cb => {
        cb.addEventListener('change', () => { markEditing(); updateChannel(idx); });
    });
    
    // Day checkboxes
    document.querySelectorAll(`.day-cb[data-ch="${idx}"]`).forEach(cb => {
        cb.addEventListener('change', () => { markEditing(); updateChannel(idx); });
    });
    
    // Dose input
    const doseInput = document.getElementById(`dose_${idx}`);
    if (doseInput) {
        doseInput.addEventListener('focus', markEditing);
        doseInput.addEventListener('input', () => updateChannel(idx));
    }
    
    // Calibration input
    const calibInput = document.getElementById(`calibMl_${idx}`);
    if (calibInput) {
        calibInput.addEventListener('focus', markEditing);
        calibInput.addEventListener('change', () => calcCalibration(idx));
    }
}

// ============================================================================
// CHANNEL UPDATE
// ============================================================================
function updateChannel(idx) {
    const ch = channels[idx];
    
    // Read events bitmask
    let events = 0;
    document.querySelectorAll(`.event-cb[data-ch="${idx}"]:checked`).forEach(cb => {
        events |= (1 << parseInt(cb.dataset.hour));
    });
    ch.events = events;
    
    // Read days bitmask
    let days = 0;
    document.querySelectorAll(`.day-cb[data-ch="${idx}"]:checked`).forEach(cb => {
        days |= (1 << parseInt(cb.dataset.day));
    });
    ch.days = days;
    
    // Read daily dose
    const doseInput = document.getElementById(`dose_${idx}`);
    ch.dailyDose = parseFloat(doseInput.value) || 0;
    
    // Calculate
    const evCnt = popcount(ch.events);
    const dayCnt = popcount(ch.days);
    const single = evCnt > 0 ? ch.dailyDose / evCnt : 0;
    const pumpTime = ch.dosingRate > 0 ? single / ch.dosingRate : 0;
    const weekly = ch.dailyDose * dayCnt;
    const completedCnt = popcount(ch.eventsCompleted);
    const failedCnt = popcount(ch.eventsFailed || 0);
    
    // Update display
    document.getElementById(`evInfo_${idx}`).textContent = `${evCnt} of 23`;
    document.getElementById(`dayInfo_${idx}`).textContent = `${dayCnt} of 7`;
    document.getElementById(`single_${idx}`).textContent = `${single.toFixed(1)} ml`;
    document.getElementById(`pumpTime_${idx}`).textContent = `${pumpTime.toFixed(1)} s`;
    document.getElementById(`weekly_${idx}`).textContent = `${weekly.toFixed(1)} ml`;
    document.getElementById(`today_${idx}`).textContent = `${completedCnt} / ${evCnt}`;

    document.getElementById(`today_${idx}`).textContent = failedCnt > 0 
    ? `${completedCnt}✓ ${failedCnt}✗ / ${evCnt}` 
    : `${completedCnt} / ${evCnt}`;
    
    // Validation
    const validMsg = document.getElementById(`validMsg_${idx}`);
    const validTxt = document.getElementById(`validTxt_${idx}`);
    const saveBtn = document.getElementById(`saveBtn_${idx}`);
    
    validMsg.className = 'valid-msg';
    let icon = '';
    
    if (evCnt === 0) {
        validMsg.classList.add('info');
        validTxt.textContent = 'Select time slots to activate';
        icon = '<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>';
        saveBtn.disabled = false;
        ch.state = 'inactive';
    } else if (dayCnt === 0) {
        validMsg.classList.add('warn');
        validTxt.textContent = 'Select active days';
        icon = '<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>';
        saveBtn.disabled = true;
        ch.state = 'incomplete';
    } else if (ch.dailyDose <= 0) {
        validMsg.classList.add('warn');
        validTxt.textContent = 'Enter daily dose';
        icon = '<path d="M10.29 3.86L1.82 18a2 2 0 001.71 3h16.94a2 2 0 001.71-3L13.71 3.86a2 2 0 00-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>';
        saveBtn.disabled = true;
        ch.state = 'incomplete';
    } else if (single < CFG.MIN_DOSE_ML) {
        validMsg.classList.add('err');
        validTxt.textContent = `Single dose ${single.toFixed(1)}ml < min ${CFG.MIN_DOSE_ML}ml`;
        icon = '<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>';
        saveBtn.disabled = true;
        ch.state = 'invalid';
    } else if (pumpTime > CFG.MAX_PUMP_SEC) {
        validMsg.classList.add('err');
        validTxt.textContent = `Pump time ${pumpTime.toFixed(0)}s > max ${CFG.MAX_PUMP_SEC}s`;
        icon = '<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>';
        saveBtn.disabled = true;
        ch.state = 'invalid';
    } else {
        validMsg.classList.add('ok');
        validTxt.textContent = 'Configuration valid';
        icon = '<path d="M22 11.08V12a10 10 0 11-5.93-9.14"/><polyline points="22,4 12,14.01 9,11.01"/>';
        saveBtn.disabled = false;
        ch.state = 'configured';
    }
    
    validMsg.querySelector('svg').innerHTML = icon;
    
    // Update state badge
    const badge = document.querySelector(`.channel-card[data-ch="${idx}"] .state-badge`);
    if (badge) {
        badge.className = `state-badge ${ch.state}`;
        badge.textContent = getStateLabel(ch.state);
    }

    renderOverview();
}

// ============================================================================
// CALIBRATION
// ============================================================================
function runCalib(idx) {
    const btn = document.getElementById(`calibBtn_${idx}`);
    btn.classList.add('running');
    btn.innerHTML = `<svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
        <circle cx="12" cy="12" r="10"/><polyline points="12,6 12,12 16,14"/>
    </svg> Running...`;
    
    // API call would go here
    fetch(`api/calibrate?channel=${idx}`, { method: 'POST' })
        .then(r => r.json())
        .catch(() => ({}));
    
    // Simulate 30s countdown
    let remaining = CFG.CALIB_SEC;
    const timer = setInterval(() => {
        remaining--;
        btn.innerHTML = `<svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
            <circle cx="12" cy="12" r="10"/><polyline points="12,6 12,12 16,14"/>
        </svg> ${remaining}s`;
        
        if (remaining <= 0) {
            clearInterval(timer);
            btn.classList.remove('running');
            btn.innerHTML = `<svg fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
                <polygon points="5,3 19,12 5,21 5,3"/>
            </svg> Run Pump (30s)`;
        }
    }, 1000);
}

function calcCalibration(idx) {
    const ml = parseFloat(document.getElementById(`calibMl_${idx}`).value) || 0;
    if (ml > 0) {
        channels[idx].dosingRate = ml / CFG.CALIB_SEC;
        updateChannel(idx);
    }
}

// ============================================================================
// SAVE / CANCEL
// ============================================================================
function saveChanges(idx) {
    const ch = channels[idx];
    
    const payload = {
        channel: idx,
        events: ch.events,
        days: ch.days,
        dailyDose: ch.dailyDose,
        dosingRate: ch.dosingRate
    };
    
    fetch('api/dosing-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(r => r.json())
    .then(data => {
        editingChannel = -1;
        if (data.success) {
            ch.state = 'pending';
            renderChannels();
            renderOverview();
            alert('Configuration saved. Changes active from tomorrow.');
        } else {
            alert('Save failed: ' + (data.error || 'Unknown error'));
        }
    })
    .catch(() => alert('Connection error'));
}

function cancelChanges(idx) {
    editingChannel = -1;
    // Reload from server
    loadStatus();
}

// ============================================================================
// NAVIGATION
// ============================================================================
function showChannels() {
    showingChannels = true;
    document.getElementById('verticalSlider').classList.add('show-channels');
}

function showNav() {
    showingChannels = false;
    document.getElementById('verticalSlider').classList.remove('show-channels');
}

function setupHeaderToggle() {
    const toggle = document.getElementById('headerToggle');
    const header = document.querySelector('.header-screen');
    
    toggle.addEventListener('click', () => {
        header.classList.toggle('hidden');
        toggle.classList.toggle('shifted');
    });
}

function goToChannel(idx) {
  
    document.querySelectorAll('.channel-card').forEach(card => {
        card.classList.remove('active');
    });
    
    currentCh = idx;
    renderOverview();

    const newCard = document.querySelector(`.channel-card[data-ch="${idx}"]`);
    if (newCard) {
        newCard.classList.add('active');
    }
}

function nextCh() { if (currentCh < CFG.CHANNEL_COUNT - 1) goToChannel(currentCh + 1); }
function prevCh() { if (currentCh > 0) goToChannel(currentCh - 1); }

// ============================================================================
// TOUCH
// ============================================================================
function setupTouch() {
    const container = document.querySelector('.app-container');
    
    container.addEventListener('touchstart', e => {
        touchStart.x = e.changedTouches[0].screenX;
        touchStart.y = e.changedTouches[0].screenY;
    }, { passive: true });

    container.addEventListener('touchend', e => {
        const dx = e.changedTouches[0].screenX - touchStart.x;
        const dy = e.changedTouches[0].screenY - touchStart.y;
        const threshold = CFG.SWIPE_THRESHOLD;
        
        if (Math.abs(dx) > Math.abs(dy)) {
            if (showingChannels) {
                if (dx < -threshold) nextCh();
                else if (dx > threshold) prevCh();
            }
        } else {
            if (dy < -threshold && !showingChannels) showChannels();
            else if (dy > threshold && showingChannels) showNav();
        }
    }, { passive: true });
}

// ============================================================================
// API
// ============================================================================
function loadStatus() {
    fetch('api/dosing-status')
        .then(r => r.json())
        .then(data => {
            if (data.channels) {
                data.channels.forEach((chData, i) => {
                    // Skip channel being edited
                    if (i === editingChannel) return;
                    
                    if (channels[i]) {
                        channels[i].events = chData.events || 0;
                        channels[i].days = chData.days || 0;
                        channels[i].dailyDose = chData.dailyDose || 0;
                        channels[i].dosingRate = chData.dosingRate || 0.33;
                        channels[i].eventsCompleted = chData.eventsCompleted || 0;
                        channels[i].eventsFailed = chData.eventsFailed || 0;
                        channels[i].state = chData.state || 'inactive';
                    }
                });
                
                // Only re-render if not editing
                if (editingChannel === -1) {
                    renderChannels();
                }
                renderOverview();
            }
            
            // System status
            if (data.systemOk !== undefined) {
                const sysEl = document.getElementById('sysStatus');
                sysEl.className = 'status-item ' + (data.systemOk ? 'ok' : 'error');
                sysEl.querySelector('.value').textContent = data.systemOk ? 'OK' : 'ERROR';
            }
            
            if (data.wifiConnected !== undefined) {
                const wifiEl = document.getElementById('wifiStatus');
                wifiEl.className = 'status-item ' + (data.wifiConnected ? 'ok' : '');
                wifiEl.querySelector('.value').textContent = data.wifiConnected ? 'OK' : 'OFF';
            }
            
            // Active dosing state
            if (data.activeChannel !== undefined) {
                const wasActive = activeChannel >= 0;
                activeChannel = data.activeChannel;
                activeEventHour = data.activeEventHour !== undefined ? data.activeEventHour : -1;
                
                // Re-render if dosing state changed
                if (wasActive !== (activeChannel >= 0)) {
                    renderChannels();
                }
            }
        })
        .catch(() => {});
}

// ============================================================================
// UTILITIES
// ============================================================================
function popcount(n) {
    let count = 0;
    while (n) { count += n & 1; n >>>= 1; }
    return count;
}

function getStateLabel(state) {
    return { inactive:'Inactive', incomplete:'Setup', invalid:'Invalid', configured:'Active', pending:'Pending' }[state] || state;
}

function updateClock() {
    const now = new Date();
    const utcHours = String(now.getUTCHours()).padStart(2, '0');
    const utcMinutes = String(now.getUTCMinutes()).padStart(2, '0');
    document.getElementById('sysTime').textContent = utcHours + ':' + utcMinutes;
}

function logout() {
    if (confirm('Logout?')) {
        fetch('api/logout', { method: 'POST' }).then(() => location.href = '/login');
    }
}

// ============================================================================
// DAILY LOG
// ============================================================================
const DL_CFG = {
    MAX_CHANNELS: 6,
    DISPLAY_CHANNELS: 4,  // Aktualnie aktywne kanały
    REFRESH_CURRENT: 30000,  // 30s dla bieżącego dnia
    REFRESH_LIST: 60000      // 60s dla listy
};

const DAY_NAMES = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

let dlEntries = [];
let dlCurrentIndex = 0;
let dlLastRefresh = 0;

function initDailyLog() {
    loadDailyLogList();
    setInterval(() => {
        const now = Date.now();
        if (dlCurrentIndex === 0 && now - dlLastRefresh > DL_CFG.REFRESH_CURRENT) {
            loadDailyLogEntry(0);
        }
    }, DL_CFG.REFRESH_CURRENT);
    
    setInterval(loadDailyLogList, DL_CFG.REFRESH_LIST);
}

function loadDailyLogList() {
    fetch('api/daily-logs')
        .then(r => r.json())
        .then(data => {
            if (data.entries && data.entries.length > 0) {
                dlEntries = data.entries;
                renderDailyLogNav();
                loadDailyLogEntry(dlCurrentIndex);
            }
        })
        .catch(err => console.log('DL list error:', err));
}

function loadDailyLogEntry(index) {
    if (index < 0 || index >= dlEntries.length) return;
    
    fetch(`api/daily-log?index=${index}`)
        .then(r => r.json())
        .then(data => {
            dlLastRefresh = Date.now();
            renderDailyLogEntry(data, index);
        })
        .catch(err => console.log('DL entry error:', err));
}

function renderDailyLogNav() {
    const total = dlEntries.length;
    document.getElementById('dlNavInfo').textContent = `Day ${dlCurrentIndex + 1} of ${total}`;
    document.getElementById('dlPrev').disabled = (dlCurrentIndex >= total - 1);
    document.getElementById('dlNext').disabled = (dlCurrentIndex <= 0);
}

function renderDailyLogEntry(entry, index) {
    // Date header
    const date = utcDayToDate(entry.utcDay);
    document.getElementById('dlDate').textContent = formatDate(date);
    document.getElementById('dlDay').textContent = DAY_NAMES[entry.dayOfWeek];
    
    // Badge
    const badge = document.getElementById('dlBadge');
    if (entry.isCurrent !== undefined ? entry.isCurrent : (index === 0 && !entry.isFinalized)) {
        badge.textContent = 'TODAY';
        badge.className = 'dl-badge today';
    } else {
        badge.textContent = 'ARCHIVE';
        badge.className = 'dl-badge archive';
    }
    
    // Channels
    renderDailyLogChannels(entry.channels || []);
    
    // System stats
    const sys = entry.system || {};
    document.getElementById('dlUptime').textContent = formatUptime(sys.uptimeSeconds || 0);
    document.getElementById('dlWifi').textContent = sys.wifiDisconnects || 0;
    document.getElementById('dlFram').textContent = sys.framWrites || 0;
    document.getElementById('dlNtp').textContent = sys.ntpSyncs || 0;
    document.getElementById('dlPower').textContent = entry.powerCycles || sys.powerCycles || 0;
}

function renderDailyLogChannels(channels) {
    const container = document.getElementById('dlChannels');
    let html = '';
    
    for (let i = 0; i < DL_CFG.DISPLAY_CHANNELS; i++) {
        const ch = channels[i] || {};
        const planned = ch.eventsPlanned || 0;
        const completed = ch.eventsCompleted || 0;
        const failed = ch.eventsFailed || 0;
        const pending = Math.max(0, planned - completed - failed);
        const dosePlanned = ch.dosePlannedMl || 0;
        const doseActual = ch.doseActualMl || 0;
        
        const isInactive = (ch.status === 'inactive' || planned === 0) && doseActual === 0;
        const inactiveClass = isInactive ? 'inactive' : '';
        
        // Calculate bar percentages
        const total = planned > 0 ? planned : 1;
        const pctCompleted = (completed / total) * 100;
        const pctFailed = (failed / total) * 100;
        const pctPending = (pending / total) * 100;
        
        html += `
            <div class="dl-channel ${inactiveClass}">
                <div class="dl-ch-header">
                    <span class="dl-ch-name">CH ${i + 1}</span>
                    <span class="dl-ch-dose">
                        <span class="actual">${doseActual.toFixed(1)}</span> / ${dosePlanned.toFixed(1)} ml
                    </span>
                </div>
                <div class="dl-bar">
                    ${isInactive ? 
                        `<div class="dl-bar-segment inactive" style="width: 100%"></div>` :
                        `<div class="dl-bar-segment completed" style="width: ${pctCompleted}%"></div>
                         <div class="dl-bar-segment failed" style="width: ${pctFailed}%"></div>
                         <div class="dl-bar-segment pending" style="width: ${pctPending}%"></div>`
                    }
                </div>
            </div>
        `;
    }
    
    container.innerHTML = html;
}

function dlNextDay() {
    if (dlCurrentIndex > 0) {
        dlCurrentIndex--;
        renderDailyLogNav();
        loadDailyLogEntry(dlCurrentIndex);
    }
}

function dlPrevDay() {
    if (dlCurrentIndex < dlEntries.length - 1) {
        dlCurrentIndex++;
        renderDailyLogNav();
        loadDailyLogEntry(dlCurrentIndex);
    }
}

// Helpers
function utcDayToDate(utcDay) {
    return new Date(utcDay * 86400 * 1000);
}

function formatDate(date) {
    const d = date.getUTCDate().toString().padStart(2, '0');
    const m = (date.getUTCMonth() + 1).toString().padStart(2, '0');
    const y = date.getUTCFullYear().toString().slice(-2);
    return `${d}.${m}.${y}`;
}

function formatUptime(seconds) {
    if (seconds < 60) return `${seconds}s`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    return `${h}h${m}m`;
}

document.addEventListener('DOMContentLoaded', init);
</script>
</body>
</html>
)rawliteral";

// ============================================================================
// GETTER FUNCTIONS
// ============================================================================
String getLoginHTML() {
    return String(LOGIN_HTML);
}

String getDashboardHTML() {
    return String(DASHBOARD_HTML);
}