

#include "html_pages.h"

// Long-press threshold for chart scroll buttons (ms).
// Keep in sync with CHART_LONGPRESS_MS in the JS section of DASHBOARD_HTML below.
#define CHART_LONGPRESS_MS 1000

const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ATO Water System - Login</title>
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

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

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
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
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

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }

        .form-group {
            margin-bottom: 20px;
        }

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
        }

        .alert.error {
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

        .info strong {
            color: var(--text-secondary);
        }
    </style>
</head>
<body>
    <div class="login-box">
        <div class="logo">
            <div class="logo-icon">
                <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
            </div>
            <h1>Top Off Water <span>– System</span></h1>
        </div>
        <form id="loginForm">
            <div class="form-group">
                <label for="password">Administrator Password</label>
                <input type="password" id="password" name="password" required />
            </div>
            <button type="submit" class="login-btn">Login</button>
        </form>
        <div id="error" class="alert error"></div>
        <div class="info">
            <strong>Security Features:</strong><br />
            • Session-based authentication<br />
            • Rate limiting & IP filtering<br />
            • VPS event logging
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
            .then((response) => response.json())
            .then((data) => {
                if (data.success) {
                    window.location.href = "./";
                } else {
                    errorDiv.textContent = data.error || "Login failed";
                    errorDiv.style.display = "block";
                }
            })
            .catch((error) => {
                errorDiv.textContent = "Connection error - Check if device is running";
                errorDiv.style.display = "block";
            });
        });
    </script>
</body>
</html>
)rawliteral";

const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ATO Water System</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-card-hover: #1a2332;
            --bg-input: #1e293b;
            --bg-control: #33435e;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --accent-yellow: #eab308;
            --shadow: 0 4px 24px rgba(0,0,0,0.4);
            --radius: 12px;
            --radius-sm: 8px;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            line-height: 1.5;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 16px;
            position: relative;
            z-index: 1;
        }

        /* Header */
        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 0 24px;
        }

        .logo {
            display: flex;
            align-items: center;
            gap: 12px;
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

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
            letter-spacing: -0.02em;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }
        

        .btn-back {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
            padding: 8px 16px;
            border-radius: var(--radius-sm);
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn-back:hover {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        /* Cards */
        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 20px;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
        }

        .card-header {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 16px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border);
        }

        .card-header h2 {
            font-size: 0.9rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .stat-column h3{
            font-size: 0.9rem;
            font-weight: 600;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .card-header-icon {
            width: 28px;
            height: 28px;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .card-header-icon svg {
            width: 16px;
            height: 16px;
        }

        /* ===== FIRST CARD: System Status ===== */
        .status-main {
            display: flex; align-items: center; justify-content: space-between;
            background: var(--bg-input); border: 1px solid var(--border);
            border-radius: var(--radius-sm); padding: 12px 16px;
            gap: 12px; position: relative;
        }
        .status-main.status-ok {
            border-color: rgba(34,197,94,0.35); background: rgba(34,197,94,0.05);
        }
        .status-main.status-ok::after {
            content: ''; position: absolute; top: 8px; right: 8px;
            width: 6px; height: 6px; background: var(--accent-green);
            border-radius: 50%; animation: pulse 2s infinite;
        }
        .status-main.status-error {
            border-color: rgba(239,68,68,0.4); background: rgba(239,68,68,0.06);
        }
        .status-main.status-warn {
            border-color: rgba(234,179,8,0.4); background: rgba(234,179,8,0.06);
        }
        .status-main.status-disabled {
            border-color: rgba(148,163,184,0.35); background: rgba(148,163,184,0.05);
        }
        .status-main-body { flex: 1; min-width: 0; }
        .status-main-desc { font-size: 0.9rem; font-weight: 600; color: var(--text-primary); }
        .status-main-sub {
            display: flex; flex-wrap: wrap; gap: 8px;
            margin-top: 5px; font-size: 0.72rem;
        }
        .sub-on     { color: var(--accent-green); }
        .sub-off    { color: var(--text-secondary); }
        .sub-warn   { color: var(--accent-yellow); }
        .sub-danger { color: var(--accent-red); }
        .sub-sep    { color: var(--border); }
        .status-main-wifi {
            display: flex; flex-direction: column; align-items: center;
            gap: 2px; flex-shrink: 0; padding-right: 8px;
        }
        .wifi-label { font-size: 0.6rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; }
        .wifi-dot   { font-size: 1.1rem; line-height: 1; }
        .status-main-wifi.wifi-on  .wifi-dot { color: var(--accent-green); }
        .status-main-wifi.wifi-off .wifi-dot { color: var(--text-muted); }
        .status-info {
            display: inline-flex; align-items: center; flex-wrap: wrap; gap: 6px;
            color: var(--text-secondary); font-family: 'Courier New', monospace;
        }
        .status-info .si-sep { color: var(--text-muted); font-family: sans-serif; }
        .status-info.rtc-warn { border-color: rgba(251,191,36,0.35); }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }


        /* ===== SECOND CARD: Pump Control ===== */
        .pump-controls {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
        }
        @media (max-width: 480px) {
            .pump-controls { grid-template-columns: repeat(1, 1fr); }

        }
        /* Manual dosing: ON adds a portion, OFF finishes the running one */
        .manual-pump {
            display: grid;
            grid-template-columns: 1fr auto;
            gap: 6px;
        }

        .manual-pump #manualOffBtn { padding: 10px 14px; }

        .btn-service {
            background: rgba(249,115,22,0.10); border: 1px solid rgba(249,115,22,0.35);
            color: #f97316;
        }
        .btn-service:hover:not(:disabled) { background: rgba(249,115,22,0.20); }

        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            // height: 48px;
            gap: 8px;
            padding: 10px 20px;
            border-radius: var(--radius-sm);
            font-family: inherit;
            font-size: 0.9rem;
            font-weight: 600;
            border: none;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none !important;
        }

        .btn svg {
            width: 18px;
            height: 18px;
        }

        .btn-primary {
            background: rgba(34, 197, 94, 0.15);
            color: var(--accent-green);
        }

        .btn-primary:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 2px 8px rgba(34, 197, 94, 0.3);
        }

        /* Stan OFF dla przycisków bistabilnych */
        .btn-off {
            background: var(--bg-input);
            color: var(--text-secondary);
        }

        .btn-off:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-secondary {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
        }

        .btn-control {
            background: var(--bg-control);
            border: 1px solid var(--border);
            color: var(--text-secondary);
        }

        .btn-secondary:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-small {
            padding: 10px 16px;
            font-size: 0.8rem;
        }

        .btn-small:nth-of-type(2){
            margin-left: 10px;
        }

        /* ===== ALG SETTINGS PANEL: Pump Calibration wizard ===== */
        .cal-box {
            margin-top: 8px;
            padding: 10px 12px;
            height: 160px;
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            font-size: 0.72rem;
            color: var(--text-secondary);
        }
        .cal-box .panel-row { flex-wrap: wrap; }
        .cal-big { font-size: 1.1rem; font-family: 'Courier New', monospace; color: var(--text-primary); }
        .cal-times { font-family: 'Courier New', monospace; color: var(--text-muted); margin-top: 6px; word-break: break-word; }

        /* ===== THIRD CARD: Statistics ===== */
        .stats-columns {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }

        /* Single Dose: pierwsza kolumna na desktopie (ostatnia w HTML → order:-1) */
        .stat-col-dose {
            order: -1;
        }

        @media (max-width: 600px) {
            .stats-columns {
                grid-template-columns: 1fr;
            }
            /* Na mobile Single Dose wraca na koniec */
            .stat-col-dose {
                order: 3;
            }
        }

        .stat-column {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .stat-column .btn {
            width: 100%;
        }

        .stat-content {
            display: flex;
            flex-direction: column;
            gap: 6px;
        }

        .stat-errors {
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

         .stat-available{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

        .stat-daily{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }
        
        .stat-line {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.8rem;
        }

        .stat-line .stat-label {
            color: var(--text-muted);
        }

        .stat-line .stat-value {
            font-family: 'Courier New', monospace;
            font-weight: 600;
            color: var(--accent-green);
        }

        .stat-line .stat-value.neutral {
            color: var(--text-primary);
        }

        /* Volume indicator */
        .volume-indicator {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .volume-bar {
            height: 8px;
            background: var(--bg-primary);
            border-radius: 4px;
            overflow: hidden;
        }

        .volume-bar-fill {
            height: 100%;
            width: 0%;
            background: linear-gradient(90deg, var(--accent-cyan), var(--accent-blue));
            border-radius: 4px;
            transition: width 0.3s;
        }

        .volume-text {
            font-family: 'Courier New', monospace;
            font-size: 0.85rem;
            color: var(--text-primary);
            text-align: center;
        }

        /* Inset panel inside a card (reserve, rolling 24h) */
        .panel-box {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 10px 12px;
            margin-bottom: 10px;
        }

        .panel-head {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            margin-bottom: 6px;
        }

        .panel-title {
            font-size: 0.72rem;
            font-weight: 600;
            color: var(--text-muted);
            letter-spacing: 0.05em;
            text-transform: uppercase;
        }

        .panel-value          { font-size: 0.8rem; color: var(--text-secondary); }
        .panel-value.is-muted { color: var(--text-muted); }
        .panel-value.is-warn  { color: var(--accent-yellow); }
        .panel-value.is-alarm { color: var(--accent-red); }

        .panel-row {
            display: flex;
            gap: 6px;
            margin-top: 8px;
            align-items: center;
        }

        .panel-row input { flex: 1; min-width: 0; }

        /* Capacity and remaining water: one line on desktop, wraps on narrow screens */
        .res-meta {
            display: flex;
            flex-wrap: wrap;
            justify-content: space-between;
            gap: 2px 16px;
            font-size: 0.95rem;
            color: var(--text-secondary);
            margin-top: 8px;
        }

        .res-meta .is-muted { color: var(--text-muted); }
        .res-meta .is-warn  { color: var(--accent-yellow); }
        .res-meta .is-alarm { color: var(--accent-red); }

        .res-note {
            font-size: 0.7rem;
            color: var(--text-muted);
            margin-top: 4px;
        }

        .res-note:empty { display: none; }

        .res-estimate { color: #f97316; }

        /* Source-water reserve: level bar from the level sensor (0 = bottom,
           100% = marker); the reserve range is yellow, the alarm range red */
        .res-bar-wrap {
            position: relative;
            height: 8px;
            margin-top: 6px;
            background: var(--bg-primary);
            border-radius: 7px;
            overflow: hidden;
        }

        .res-bar-fill {
            height: 100%;
            width: 0%;
            background: linear-gradient(90deg, var(--accent-blue), var(--accent-cyan));
            transition: width 0.3s, background 0.3s;
        }

        /* Reserve range: faint background over its whole width, solid where filled */
        .res-bar-low {
            position: absolute;
            top: 0;
            bottom: 0;
            left: 0;
            background: rgba(234, 179, 8, 0.18);
            pointer-events: none;
        }

        .res-bar-low-fill {
            position: absolute;
            top: 0;
            bottom: 0;
            left: 0;
            width: 0%;
            background: var(--accent-yellow);
            transition: width 0.3s, background 0.3s;
            pointer-events: none;
        }

        /* Estimated mode (no level reading): each range up to the minimum estimate as
           an outline in its own color with a tinted inside (opaque, so the range
           drawn over another hides its outline) */
        .res-bar-wrap.is-estimated .res-bar-fill,
        .res-bar-wrap.is-estimated .res-bar-low-fill,
        .res-bar-wrap.is-estimated .res-bar-alarm-fill {
            border: 2px solid;
            border-radius: 7px;
            box-sizing: border-box;
        }

        .res-bar-wrap.is-estimated .res-bar-fill {
            border-color: var(--accent-blue);
            background: linear-gradient(rgba(56, 189, 248, 0.3), rgba(56, 189, 248, 0.3)), var(--bg-primary);
        }

        .res-bar-wrap.is-estimated .res-bar-low-fill {
            border-color: var(--accent-yellow);
            background: linear-gradient(rgba(234, 179, 8, 0.3), rgba(234, 179, 8, 0.3)), var(--bg-primary);
        }

        .res-bar-wrap.is-estimated .res-bar-alarm-fill {
            border-color: var(--accent-red);
            background: linear-gradient(rgba(239, 68, 68, 0.3), rgba(239, 68, 68, 0.3)), var(--bg-primary);
        }

        /* An empty range (width 0) would still show its 2 px border */
        .res-bar-wrap.is-estimated .is-empty { display: none; }

        .res-bar-alarm {
            position: absolute;
            top: 0;
            bottom: 0;
            left: 0;
            background: rgba(239, 68, 68, 0.22);
            pointer-events: none;
        }

        .res-bar-alarm-fill {
            position: absolute;
            top: 0;
            bottom: 0;
            left: 0;
            width: 0%;
            background: var(--accent-red);
            transition: width 0.3s;
            pointer-events: none;
        }

        /* ===== Algorithm parameters: one line on desktop, stacked on mobile ===== */
        .alg-inputs {
            display: flex;
            flex-wrap: wrap;
            gap: 8px 20px;
        }

        .alg-inputs label {
            flex: 1 1 200px;
            display: grid;
            grid-template-columns: 1fr 80px;
            align-items: center;
            gap: 8px;
            font-size: 0.72rem;
            color: var(--text-muted);
        }

        .alg-inputs input { width: 100%; }

        /* ===== Reservoir geometry: oblique wireframe drawing + inputs ===== */
        .res-geo {
            display: flex;
            flex-wrap: wrap;
            gap: 12px 20px;
            align-items: center;
        }

        .res-geo svg {
            flex: 0 1 280px;
            width: 100%;
            max-width: 280px;
            height: auto;
        }

        .res-geo-inputs {
            flex: 1 1 200px;
            display: grid;
            gap: 8px;
        }

        .res-geo-inputs label {
            display: grid;
            grid-template-columns: 22px 1fr 90px;
            align-items: center;
            gap: 8px;
            font-size: 0.72rem;
            color: var(--text-muted);
        }

        .res-geo-inputs input { width: 100%; }

        .dim-badge {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 22px;
            height: 22px;
            border-radius: 50%;
            font-weight: 700;
            font-size: 0.75rem;
            color: var(--accent-cyan);
            border: 1px solid var(--accent-cyan);
            transition: color 0.2s, border-color 0.2s, background 0.2s;
        }

        .res-geo-inputs label.hl .dim-badge {
            color: var(--bg-primary);
            background: var(--accent-yellow);
            border-color: var(--accent-yellow);
        }

        .tank-edge   { fill: none; stroke: var(--text-secondary); stroke-width: 1.5; stroke-linejoin: round; }
        .tank-hidden { fill: none; stroke: var(--text-muted); stroke-width: 1; stroke-dasharray: 4 3; }
        .tank-water  { fill: var(--accent-blue); fill-opacity: 0.15; stroke: none; }
        .tank-level  { fill: none; stroke: var(--accent-blue); stroke-width: 1; }
        .tank-sensor { fill: var(--bg-input); stroke: var(--text-secondary); stroke-width: 1.5; }
        .dim line, .dim path { stroke: var(--accent-cyan); stroke-width: 1.2; fill: none; }
        .dim .dim-ext { stroke-width: 0.8; stroke-dasharray: 2 2; opacity: 0.7; }
        .dim text { fill: var(--accent-cyan); font-size: 13px; font-weight: 700; font-family: inherit; }
        .dim.hl line, .dim.hl path { stroke: var(--accent-yellow); stroke-width: 2; }
        .dim.hl .dim-ext { stroke-width: 1; }
        .dim.hl text { fill: var(--accent-yellow); }
        #dimArrow path { fill: var(--accent-cyan); stroke: none; }

        /* ===== FOURTH CARD: Pump Settings ===== */
        .settings-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 16px;
            align-items: start;
        }

        @media (max-width: 600px) {
            .settings-row {
                grid-template-columns: 1fr;
            }
        }

        .setting-item {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .setting-item.input-group {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 12px;
        }

        .setting-item label {
            font-size: 0.75rem;
            font-weight: 500;
            text-transform: uppercase;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-muted);
        }

        input[type="text"],
        input[type="number"] {
            background: var(--bg-primary);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 10px 14px;
            font-family: 'Courier New', monospace;
            font-size: 1rem;
            color: var(--text-primary);
            width: 100%;
            text-align: center;
        }

        input:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .current-value {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-align: center;
            margin-top: 4px;
        }

        /* Footer */
        .footer-info {
            text-align: center;
            padding: 16px;
            color: var(--text-muted);
            font-size: 0.75rem;
        }

        /* Cycle History Charts */
        .chart-wrap { margin-top: 12px; }
        .chart-sublabel:first-child { margin-top: 0; }
        .chart-rate-scroll {
            width: 100%;
            border-radius: 6px;
            overflow: hidden;
        }
        #chartRate {
            width: 100%;
            height: 190px;
            display: block;
            background: #1e293b;
        }
        .chart-legend {
            display: flex;
            flex-wrap: wrap;
            gap: 12px;
            margin-top: 10px;
            font-size: 0.72rem;
            color: var(--text-muted);
        }

        .card-subheader {
            font-size: 0.9rem; font-weight: 600; color: var(--text-primary);
            padding: 12px 0 4px; border-top: 1px solid var(--border); margin-top: 12px;
        }

    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header>
            <div class="logo">
                <div class="logo-icon">
                    <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
                </div>
                <h1>ATO Water System</h1>
            </div>
            <button class="btn-back" onclick="logout()">Back</button>
        </header>

        <!-- FIRST CARD: System Status -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(56, 189, 248, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-blue);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>
                </div>
                <h2>System Status</h2>
            </div>

            <div class="status-main status-ok" id="statusMain">
                <div class="status-main-body">
                    <div class="status-main-desc" id="processDescription">IDLE - Waiting for sensors</div>
                    <div class="status-main-sub">
                        <span id="sensor1Badge" class="sub-off">Sensor: OFF</span>
                        <span class="sub-sep">•</span>
                        <span id="pumpBadge" class="sub-off">Pump: OFF</span>
                        <span class="sub-sep">•</span>
                        <span class="status-info" id="rtcItem">
                            <span id="rtcTime">—</span><span id="rtcHint"></span>
                            <span class="si-sep">•</span>
                            <span id="freeHeap">—</span>
                            <span class="si-sep">•</span>
                            <span id="uptime">—</span>
                        </span>
                    </div>
                </div>
                <div class="status-main-wifi wifi-off" id="wifiItem">
                    <span class="wifi-label">WiFi</span>
                    <span class="wifi-dot" id="wifiStatus">●</span>
                </div>
            </div>
            </div>

        <!-- SECOND CARD: Pump Control -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(34, 197, 94, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-green);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 14.5v-9l6 4.5-6 4.5z"/></svg>
                </div>
                <h2>System Control</h2>
            </div>

            <div class="pump-controls">
                <button id="systemToggleBtn" class="btn btn-service" onclick="toggleSystem()">Service Mode</button>
                <div class="manual-pump">
                    <button id="manualPumpBtn" class="btn btn-off" onclick="manualPumpOn()" title="Each press adds one portion">Pump ON</button>
                    <button id="manualOffBtn" class="btn btn-off" onclick="manualPumpOff()" disabled title="Finish the running portion, drop the rest">OFF</button>
                </div>
                <button id="systemResetBtn" class="btn btn-control" onclick="systemReset()">Cycle Reset</button>
            </div>
        </div>



        <!-- ATO PROCESS MONITOR CARD -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(129, 140, 248, 0.15);">
                    <svg fill="currentColor" style="color: #818cf8;" viewBox="0 0 24 24"><path d="M13 2.05v2.02c3.95.49 7 3.85 7 7.93 0 3.21-1.81 6-4.72 7.28L13 17v5c5.05-.56 9-4.78 9-9.93 0-5.45-4.22-9.92-9-10.02zm-2 0C5.95 2.61 2 6.83 2 12c0 4.64 3.17 8.6 7.2 9.86v-2.09c-2.89-1.14-4.96-3.87-4.96-7.05 0-3.25 2-6 4.76-7.72V2.05z"/></svg>
                </div>
                <h2>ATO Process Monitor</h2>
            </div>
            <div class="panel-box">
                <div class="panel-head">
                    <span class="panel-title">Water Reserve</span>
                </div>
                <div class="res-bar-wrap" id="resBarWrap">
                    <div class="res-bar-fill" id="resBarFill"></div>
                    <div class="res-bar-low" id="resBarLow"></div>
                    <div class="res-bar-low-fill" id="resBarLowFill"></div>
                    <div class="res-bar-alarm" id="resBarAlarm"></div>
                    <div class="res-bar-alarm-fill" id="resBarAlarmFill"></div>
                </div>
                <div class="res-meta">
                    <span id="resCapacity">&#8212;</span>
                    <span id="resLeft"></span>
                </div>
                <div class="res-note" id="resNote"></div>
                <div id="resRefillRow" style="display:none;margin-top:8px;">
                    <button class="btn btn-secondary btn-small" onclick="reserveRefilled()" id="resRefillBtn">Reservoir refilled</button>
                </div>
            </div>
            <div class="panel-box">
                <div class="panel-head">
                    <span class="panel-title">Added in 24h</span>
                    <span class="panel-value" id="volumeText">0.0 l / 2.0 l</span>
                </div>
                <div class="volume-bar"><div class="volume-bar-fill" id="volumeBarFill"></div></div>
                <div class="panel-row">
                    <input type="number" id="dailyLimitInput" min="100" max="10000" step="100" placeholder="limit ml">
                    <button class="btn btn-secondary btn-small" onclick="setDailyLimit()">Set</button>
                    <button class="btn btn-off btn-small" onclick="resetDailyVolume()">Reset</button>
                </div>
            </div>
            <div class="chart-wrap">
                <div style="font-size:0.7rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;margin-bottom:5px;padding-left:2px;">Evaporation Rate (ml/h)</div>
                <div style="display:flex;gap:8px;margin-bottom:8px;">
                    <button class="btn btn-secondary" onclick="loadCycleHistory()" id="loadCyclesBtn" style="flex:1;">Load History</button>
                </div>
                <!-- Hidden until Load History (empty canvas after a page refresh) -->
                <div id="chartRateScroll" class="chart-rate-scroll" style="display:none;">
                    <canvas id="chartRate"></canvas>
                </div>
                <div class="chart-legend" style="align-items:center;">
                    <span><span style="color:#94a3b8;font-weight:bold;">&#8212;</span> EMA</span>
                    <span style="color:#f97316;font-weight:bold;">&#9650;</span><span style="color:var(--text-muted);"> clipped</span>
                    <span id="legendEmaRate" style="color:var(--text-secondary);border-left:1px solid var(--border);padding-left:10px;display:none;"></span>
                    <span id="scoreZoneBadge" style="display:none;padding-left:10px;font-weight:600;border-left:1px solid var(--border);"></span>
                    <button onclick="toggleAlgSettings()" id="algSettingsBtn"
                        style="margin-left:auto;font-size:0.70rem;padding:3px 8px;border-radius:4px;border:1px solid var(--border);background:var(--bg-input);color:var(--text-muted);cursor:pointer;">
                        &#9881; Settings</button>
                </div>
                <div id="algSettingsPanel" style="display:none;margin-top:10px;background:var(--bg-input);border:1px solid var(--border);border-radius:var(--radius-sm);padding:12px 14px;">
                    <div style="font-size:0.68rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;margin-bottom:10px;">Algorithm Parameters</div>
                    <div class="alg-inputs">
                        <label>Zone Green (1–200)
                            <input type="number" id="algZoneGreen" min="1" max="200" step="0.5"></label>
                        <label>Zone Yellow (&le;500)
                            <input type="number" id="algZoneYellow" min="1" max="500" step="0.5"></label>
                        <label>Initial EMA ml/h (1–500)
                            <input type="number" id="algInitEma" min="1" max="500" step="1"></label>
                    </div>
                    <div id="algSettingsError" style="display:none;color:#f87171;font-size:0.72rem;margin-top:8px;"></div>
                    <div style="display:flex;gap:8px;margin-top:10px;">
                        <button class="btn btn-primary btn-small" onclick="saveAlgSettings()" id="algSaveBtn">Save</button>
                        <button class="btn btn-secondary btn-small" onclick="toggleAlgSettings()">Cancel</button>
                        <button class="btn btn-secondary btn-small" onclick="deleteCycleHistory()" id="deleteCyclesBtn"
                            style="margin-left:auto;color:#f87171;border-color:rgba(248,113,113,0.3);">Delete Data</button>
                    </div>

                    <div style="font-size:0.68rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;margin:16px 0 10px;padding-top:12px;border-top:1px solid var(--border);">Reservoir &amp; Level Sensor</div>
                    <div class="res-geo">
                        <!-- Oblique (cabinet) view: front face = length x height, receding edge = width -->
                        <svg viewBox="0 0 280 240" role="img" aria-label="Reservoir dimensions">
                            <defs>
                                <marker id="dimArrow" viewBox="0 0 10 10" refX="9" refY="5"
                                    markerWidth="7" markerHeight="7" orient="auto-start-reverse">
                                    <path d="M0,1 L10,5 L0,9 z"/>
                                </marker>
                            </defs>
                            <!-- water up to the marker level -->
                            <polygon class="tank-water" points="30,206 180,206 180,116 30,116"/>
                            <polygon class="tank-water" points="180,206 240,166 240,76 180,116"/>
                            <polyline class="tank-level" points="30,116 180,116 240,76"/>
                            <polyline class="tank-hidden" points="30,116 90,76 240,76"/>
                            <!-- hidden edges -->
                            <polyline class="tank-hidden" points="30,206 90,166 240,166"/>
                            <line class="tank-hidden" x1="90" y1="166" x2="90" y2="36"/>
                            <!-- visible edges -->
                            <rect class="tank-edge" x="30" y="76" width="150" height="130"/>
                            <polyline class="tank-edge" points="30,76 90,36 240,36 180,76"/>
                            <polyline class="tank-edge" points="240,36 240,166 180,206"/>
                            <!-- sensor above the top centre, beam down to the bottom centre -->
                            <rect class="tank-sensor" x="122" y="8" width="26" height="10" rx="2"/>

                            <!-- A: length (front bottom edge) -->
                            <g class="dim" id="dimA">
                                <line class="dim-ext" x1="30" y1="208" x2="30" y2="228"/>
                                <line class="dim-ext" x1="180" y1="208" x2="180" y2="228"/>
                                <line x1="31" y1="222" x2="179" y2="222" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
                                <text x="100" y="217">A</text>
                            </g>
                            <!-- B: width (receding bottom edge) -->
                            <g class="dim" id="dimB">
                                <line class="dim-ext" x1="182" y1="208" x2="196" y2="214"/>
                                <line class="dim-ext" x1="242" y1="168" x2="256" y2="174"/>
                                <line x1="195" y1="212" x2="254" y2="173" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
                                <text x="230" y="204">B</text>
                            </g>
                            <!-- C: sensor face to bottom (along the beam) -->
                            <g class="dim" id="dimC">
                                <line x1="135" y1="19" x2="135" y2="185" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
                                <text x="141" y="150">C</text>
                            </g>
                            <!-- D: marker level from the bottom -->
                            <g class="dim" id="dimD">
                                <line class="dim-ext" x1="28" y1="116" x2="8" y2="116"/>
                                <line class="dim-ext" x1="28" y1="206" x2="8" y2="206"/>
                                <line x1="14" y1="117" x2="14" y2="205" marker-start="url(#dimArrow)" marker-end="url(#dimArrow)"/>
                                <text x="17" y="166">D</text>
                            </g>
                        </svg>
                        <div class="res-geo-inputs">
                            <label data-dim="A"><span class="dim-badge">A</span>Length mm (50–2000)
                                <input type="number" id="resLength" min="50" max="2000" step="1"></label>
                            <label data-dim="B"><span class="dim-badge">B</span>Width mm (50–2000)
                                <input type="number" id="resWidth" min="50" max="2000" step="1"></label>
                            <label data-dim="C"><span class="dim-badge">C</span>Sensor &rarr; bottom mm (70–1200)
                                <input type="number" id="resSensorBottom" min="70" max="1200" step="1"></label>
                            <label data-dim="D"><span class="dim-badge">D</span>Marker level mm (from bottom)
                                <input type="number" id="resMarker" min="20" max="1150" step="1"></label>
                        </div>
                    </div>
                    <div class="alg-inputs" style="margin-top:10px;">
                        <label>Reserve level % of capacity (warning)
                            <input type="number" id="resReservePct" min="6" max="90" step="1"></label>
                        <label>Alarm level % of capacity (5–50)
                            <input type="number" id="resAlarmPct" min="5" max="50" step="1"></label>
                    </div>
                    <div id="resDerived" style="font-size:0.72rem;color:var(--text-secondary);margin-top:8px;"></div>
                    <div id="resWarning" style="display:none;color:#fbbf24;font-size:0.72rem;margin-top:6px;"></div>
                    <div id="resError" style="display:none;color:#f87171;font-size:0.72rem;margin-top:6px;"></div>
                    <div style="display:flex;gap:8px;margin-top:10px;">
                        <button class="btn btn-primary btn-small" onclick="saveReserveConfig()" id="resSaveBtn">Save reservoir</button>
                    </div>

                    <div style="font-size:0.68rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;margin:16px 0 10px;padding-top:12px;border-top:1px solid var(--border);">Pump Calibration &amp; Single Dose</div>
                    <div id="calSummary" style="font-size:0.72rem;color:var(--text-secondary);"></div>
                    <div style="display:flex;gap:8px;margin-top:10px;">
                        <button class="btn btn-secondary btn-small" onclick="calOpen()" id="calOpenBtn">Calibrate pump</button>
                    </div>
                    <div class="cal-box" id="calBox" style="display:none;">
                        <div id="calText"></div>
                        <div class="cal-times" id="calTimes"></div>
                        <div class="panel-row" id="calActions"></div>
                    </div>
                </div>
            </div>
        </div>

                <!-- THIRD CARD: Statistics -->


        <!-- Footer -->
        <div class="footer-info">
            ATO Water System • v4.0
        </div>
    </div>

    <script>

        // ============================================
        // SESSION EXPIRY HANDLING (for VPS proxy)
        // ============================================

        let sessionExpired = false;
        let pollingIntervals = [];

        function handleSessionExpired() {
            if (sessionExpired) return;
            sessionExpired = true;

            // Stop all polling intervals
            pollingIntervals.forEach(id => clearInterval(id));
            pollingIntervals = [];
            // Show overlay
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
                        <h2 style="color: #e74c3c; margin-bottom: 15px;">Session Expired</h2>
                        <p style="color: #666; margin-bottom: 25px;">Your session has expired. Please log in again.</p>
                        <a href="login"
                           onclick="if(window.location.pathname.indexOf('/device/')!==-1){window.location.href='/dashboard';return false;}"
                           style="
                            display: inline-block; padding: 12px 30px;
                            background: #3498db; color: white;
                            text-decoration: none; border-radius: 8px;
                            font-weight: bold;
                        ">Back to Login</a>
                    </div>
                </div>
            `;
            document.body.appendChild(overlay);
            console.log("Session expired - polling stopped");
        }

        async function secureFetch(url, options = {}) {
            if (sessionExpired) return null;

            try {
                const response = await fetch(url, options);
                
                // Check for auth failure (401 or redirect to login)
                if (response.status === 401 || 
                    response.redirected && response.url.includes('/login')) {
                    handleSessionExpired();
                    return null;
                }
                return response;
            } catch (error) {
                console.error('Fetch error:', error);
                return null;
            }
        }


        // ============================================
        // STATE TRACKING
        // ============================================
        let systemEnabled = true;
        let maxDailyVolume = 2000;

        // ============================================
        // SYSTEM TOGGLE (Auto Mode / Service Mode)
        // ============================================
        function toggleSystem() {
            const btn = document.getElementById("systemToggleBtn");
            btn.disabled = true;

            secureFetch("api/system-toggle", { method: "POST" })
                .then((response) => {
                    if (!response) { btn.disabled = false; return; }
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled);
                    }
                    btn.disabled = false;
                })
                .catch((error) => {
                    console.error("Toggle system error:", error);
                    btn.disabled = false;
                });
        }

        // remainingS — Service Mode auto return to Auto (0 / undefined = none shown)
        function updateSystemToggleButton(enabled, remainingS) {
            const btn = document.getElementById("systemToggleBtn");
            if (!btn) return;

            if (enabled) {
                btn.textContent = "Auto Mode";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = remainingS > 0
                    ? "Service Mode (" + Math.ceil(remainingS / 60) + " min)"
                    : "Service Mode";
                btn.className = "btn btn-service";
            }
        }

        function loadSystemState() {
            secureFetch("api/system-toggle")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled);
                    }
                })
                .catch((error) => console.error("Failed to load system state:", error));
        }

        // ============================================
        // CYCLE RESET
        // ============================================
        function systemReset() {
            var btn = document.getElementById("systemResetBtn");
            btn.disabled = true;
            btn.textContent = "Resetting...";
            fetch("api/system-reset", { method: "POST" })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (!data.success) console.error("System reset failed:", data.message);
                })
                .catch(function(e) { console.error("System reset error:", e); })
                .finally(function() {
                    btn.disabled = false;
                    btn.textContent = "Cycle Reset";
                });
        }

        // ============================================
        // MANUAL DOSING IN PORTIONS (ON = +1 portion, OFF = finish the running one)
        // ============================================
        function manualPumpPost(url) {
            return fetch(url, { method: "POST" })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (!data.success) alert(data.error || "Pump command failed");
                    updateStatus();
                })
                .catch(function(e) { console.error("Manual pump error:", e); });
        }

        function manualPumpOn()  { manualPumpPost("api/pump/direct-on"); }
        function manualPumpOff() { manualPumpPost("api/pump/direct-off"); }

        function updatePumpButton(d) {
            var btn = document.getElementById("manualPumpBtn");
            var off = document.getElementById("manualOffBtn");
            if (!btn || !off) return;
            if (d.direct_pump_mode) {
                var total = d.manual_done + d.manual_left;
                btn.textContent = d.manual_pause
                    ? "Pause " + d.manual_done + "/" + total
                    : "Portion " + (d.manual_done + 1) + "/" + total;
                btn.className = "btn btn-primary";
                off.disabled = false;
            } else {
                btn.textContent = "Pump ON";
                btn.className = "btn btn-off";
                off.disabled = true;
            }
            // Manual dosing only in Service Mode (on-site service action)
            btn.disabled = !d.system_disabled;
            btn.title = d.system_disabled ? "Each press adds one portion" : "Manual dosing only in Service Mode";
        }

        // ============================================
        // PUMP CALIBRATION WIZARD (portion = Single Dose ml)
        // ============================================
        let calSetupOpen = false;   // step 1 (portion) shown, firmware still idle
        let calKey = '';
        let calRunTimer = null, calRunStartLocal = 0;

        function calPost(body) {
            return secureFetch('api/pump/calibration', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: body
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(d) {
                if (d && !d.success) alert(d.error || 'Calibration error');
                updateStatus();
                return d;
            })
            .catch(function(e) { console.error('calibration error:', e); });
        }

        function calOpen() {
            calSetupOpen = true;
            calKey = '';
            if (lastStatus) updateCalibration(lastStatus);
        }

        function calStart() {
            const dose = parseInt(document.getElementById('calDoseInput').value, 10);
            if (isNaN(dose) || dose < 1 || dose > 2000) { alert('Single Dose: 1\u20132000 ml'); return; }
            calPost('action=start&dose_ml=' + dose).then(function(d) { if (d && d.success) calSetupOpen = false; });
        }

        function calAction(a) {
            if (a === 'stop' && calRunTimer) { clearInterval(calRunTimer); calRunTimer = null; }
            if (a === 'cancel' && !confirm('Cancel the calibration? Portion and pump time stay unchanged.')) return;
            calPost('action=' + a);
        }

        function calBtn(label, onclick, cls, id) {
            return '<button class="btn ' + (cls || 'btn-secondary') + ' btn-small"' + (id ? ' id="' + id + '"' : '') +
                   ' onclick="' + onclick + '">' + label + '</button>';
        }

        function fmtS(ms) { return (ms / 1000).toFixed(1) + ' s'; }

        function updateCalibration(d) {
            const sum = document.getElementById('calSummary');
            const dose = d.res_dose_ml;
            sum.textContent = 'Single Dose ' + dose + ' ml \u2022 portion time ' + fmtS(d.cal_portion_ms);
            const st = document.createElement('div');
            st.style.marginTop = '3px';
            st.style.color = d.cal_calibrated ? 'var(--accent-green)' : 'var(--accent-yellow)';
            st.textContent = d.cal_calibrated ? 'Pump calibrated' : 'Calibrate the pump';
            sum.appendChild(st);

            const box = document.getElementById('calBox');
            const openBtn = document.getElementById('calOpenBtn');
            const phase = d.cal_phase || 0;
            openBtn.disabled = phase !== 0 || calSetupOpen;
            if (phase !== 0 && calSetupOpen) calSetupOpen = false;

            // The wizard lives in the settings panel — keep it open while active
            if ((phase !== 0 || calSetupOpen) && !algSettingsOpen) toggleAlgSettings();

            if (phase !== 3 && calRunTimer) { clearInterval(calRunTimer); calRunTimer = null; }

            const txt = document.getElementById('calText');
            const times = document.getElementById('calTimes');
            const act = document.getElementById('calActions');
            let key, html = '';
            times.textContent = (d.cal_times && d.cal_times.length)
                ? 'Runs: ' + d.cal_times.map(function(ms) { return (ms / 1000).toFixed(2); }).join(' \u00b7 ') + ' s' : '';

            if (phase === 0 && !calSetupOpen) {
                box.style.display = d.cal_note ? '' : 'none';
                txt.textContent = d.cal_note ? 'Last calibration: ' + d.cal_note + '.' : '';
                times.textContent = '';
                act.innerHTML = ''; calKey = '';
                return;
            }
            box.style.display = '';

            if (phase === 0) {
                txt.innerHTML = 'Step 1 \u2014 portion. Prepare a measuring vessel with a mark at the portion. ' +
                    'Each run is started with Start run and stopped exactly at the mark. ' +
                    'After each run empty the vessel \u2014 <b>not back into the reservoir</b>. ' +
                    'The reservoir needs 4 portions + 1.';
                key = 'setup';
                html = '<label style="font-size:0.72rem;color:var(--text-muted);">Single Dose ml (1\u20132000) ' +
                       '<input type="number" id="calDoseInput" min="1" max="2000" step="1" value="' + dose + '" style="width:110px;"></label>' +
                       calBtn('Start calibration', 'calStart()', 'btn-primary') +
                       calBtn('Close', 'calSetupOpen=false;updateStatus()');
            } else if (phase === 2) {
                const n = d.cal_runs + 1;
                txt.innerHTML = '<span class="cal-big">Run ' + n + ' / ' + d.cal_runs_total + '</span> \u2014 portion ' + d.cal_dose_ml + ' ml. ' +
                    (d.cal_pause_left > 0 ? 'Hose draining back, empty the vessel; next run in ' + d.cal_pause_left + ' s.'
                                          : 'Empty vessel in place? Start, then Stop exactly at the mark.');
                key = 'ready' + (d.cal_pause_left > 0 ? 'p' : '');
                html = calBtn('Start run', "calAction('run')", 'btn-primary', 'calRunBtn') + calBtn('Cancel', "calAction('cancel')");
            } else if (phase === 3) {
                if (!calRunTimer) {
                    calRunStartLocal = Date.now() - (d.cal_run_ms || 0);
                    calRunTimer = setInterval(function() {
                        const el = document.getElementById('calRunClock');
                        if (el) el.textContent = fmtS(Date.now() - calRunStartLocal);
                    }, 100);
                }
                txt.innerHTML = '<span class="cal-big">Run ' + (d.cal_runs + 1) + ' / ' + d.cal_runs_total +
                    ' \u2014 <span id="calRunClock">' + fmtS(d.cal_run_ms || 0) + '</span></span><br>Pumping \u2014 press Stop when the water reaches the mark.';
                key = 'run';
                html = calBtn('STOP', "calAction('stop')", 'btn-primary', 'calStopBtn') + calBtn('Cancel', "calAction('cancel')");
            } else if (phase === 4) {
                let r = 'Result: portion ' + d.cal_dose_ml + ' ml = <b>' + fmtS(d.cal_mean_ms) + '</b> (mean), ' +
                        'min ' + fmtS(d.cal_min_ms) + ', max ' + fmtS(d.cal_max_ms) + ', sd ' + (d.cal_sd_ms / 1000).toFixed(2) + ' s.';
                if (d.cal_note) r += '<br><span style="color:var(--accent-yellow);">' + d.cal_note.charAt(0).toUpperCase() + d.cal_note.slice(1) + '.</span>';
                txt.innerHTML = r;
                key = 'result';
                html = calBtn('Save', "calAction('confirm')", 'btn-primary') + calBtn('Reject', "calAction('cancel')");
            }
            if (key !== calKey) { calKey = key; act.innerHTML = html; }
            const rb = document.getElementById('calRunBtn');
            if (rb) rb.disabled = d.cal_pause_left > 0;
        }

        // ============================================
        // STATUS BADGE UPDATES
        // ============================================
        function updateSensorBadge(badgeId, isActive) {
            const el = document.getElementById(badgeId);
            if (!el) return;
            el.textContent = 'Sensor: ' + (isActive ? 'ON' : 'OFF');
            el.className = isActive ? 'sub-on' : 'sub-off';
        }

        function updatePumpBadge(badgeId, isActive, attempt) {
            const el = document.getElementById(badgeId);
            if (!el) return;
            if (!isActive) {
                el.textContent = 'Pump: OFF'; el.className = 'sub-off';
            } else {
                el.textContent = 'Pump: ON';
                el.className = attempt >= 3 ? 'sub-danger' : attempt === 2 ? 'sub-warn' : 'sub-on';
            }
        }

        function updateSystemBadge(badgeId, hasError, isDisabled, hasWarning) {
            const main = document.getElementById('statusMain');
            if (!main) return;
            main.className = 'status-main';
            if (hasError)         main.classList.add('status-error');
            else if (isDisabled)  main.classList.add('status-disabled');
            else if (hasWarning)  main.classList.add('status-warn');
            else                  main.classList.add('status-ok');
        }

        function formatTime(seconds) {
            if (!seconds || seconds <= 0) return "—";
            if (seconds < 60) return seconds + " sec";
            const minutes = Math.floor(seconds / 60);
            const secs = seconds % 60;
            if (secs === 0) return minutes + " min";
            return minutes + "m " + secs + "s";
        }

        function formatUptime(milliseconds) {
            const seconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            return hours + "h " + minutes + "m";
        }

        // ============================================
        // RESERVE BARGRAPH
        // ============================================
        // Level bar from the level sensor — estimate (±2 cm), information only
        // Litres rounded to 0.1 l (panel boxes only)
        function fmtL(ml) { return (ml / 1000).toFixed(1) + ' l'; }

        // Duration for the reserve notes: min / h / days
        function fmtAge(s) {
            if (s < 3600)  return Math.max(1, Math.round(s / 60)) + ' min';
            if (s < 172800) return (s / 3600).toFixed(1) + ' h';
            return Math.round(s / 86400) + ' days';
        }

        function reserveRefilled() {
            if (!confirm('Reservoir refilled to the marker?\nThe level sensor is not reading \u2014 the estimate restarts from a full reservoir and the reservoir alarm clears. Confirm only after filling it to the marker.')) return;
            secureFetch('api/reserve/refill', {method: 'POST'})
                .then(function(r) { return r ? r.json() : null; })
                .then(function(d) { if (d && !d.success) alert(d.error || 'Refill failed'); updateStatus(); })
                .catch(function(e) { console.error('reserve refill error:', e); });
        }

        // Text with every word "estimate" highlighted (DOM nodes — no HTML from the data)
        function setTextMarkEstimate(el, text) {
            el.textContent = '';
            text.split('estimate').forEach(function(part, i) {
                if (i > 0) {
                    const span = document.createElement('span');
                    span.className = 'res-estimate';
                    span.textContent = 'estimate';
                    el.appendChild(span);
                }
                if (part) el.appendChild(document.createTextNode(part));
            });
        }

        function updateReserveBar(d) {
            if (d.res_capacity_ml === undefined) return;
            const wrap = document.getElementById('resBarWrap');
            const capEl = document.getElementById('resCapacity');
            const leftEl = document.getElementById('resLeft');
            const note = document.getElementById('resNote');

            // The level bar needs the reservoir geometry saved in Settings
            if (!d.res_configured) {
                wrap.style.display = 'none';
                capEl.textContent = 'Not configured';
                capEl.className = 'is-muted';
                leftEl.textContent = '';
                note.textContent = 'Enter the reservoir dimensions in Settings \u2192 Reservoir & Level Sensor';
                document.getElementById('resRefillRow').style.display = 'none';
                return;
            }
            wrap.style.display = '';
            capEl.className = '';

            const zones = ['zone-ok', 'zone-reserve', 'zone-alarm', 'zone-unknown'];
            wrap.className = 'res-bar-wrap ' + (zones[d.res_zone] || 'zone-unknown') +
                             (d.res_estimated ? ' is-estimated' : '');
            const cap = d.res_capacity_ml || 1;
            const lowPct = Math.min(100, d.res_reserve_ml * 100 / cap);
            const alarmPct = Math.min(100, d.res_alarm_ml * 100 / cap);
            const fillPct = (d.res_reading || d.res_estimated) ? Math.min(100, d.res_pct) : 0;
            const setW = function(id, pct) {
                const el = document.getElementById(id);
                el.style.width = pct + '%';
                el.classList.toggle('is-empty', pct <= 0);
            };
            setW('resBarLow', lowPct);
            setW('resBarLowFill', Math.min(fillPct, lowPct));
            setW('resBarAlarm', alarmPct);
            setW('resBarAlarmFill', Math.min(fillPct, alarmPct));
            setW('resBarFill', fillPct);

            capEl.textContent = 'Capacity: ' + fmtL(d.res_capacity_ml) + ' / ' + d.res_capacity_portions + ' doses';

            const notes = [];
            const leftClass = d.res_zone === 2 ? 'is-alarm' : d.res_zone === 1 ? 'is-warn' : '';
            if (d.res_reading) {
                leftEl.textContent = 'Left: \u2248 ' + fmtL(d.res_ml) + ' / ' + d.res_portions + ' doses';
                leftEl.className = leftClass;
            } else if (d.res_estimated) {
                setTextMarkEstimate(leftEl, 'Left: \u2265 ' + fmtL(d.res_ml) + ' / ' + d.res_portions + ' doses (estimate)');
                leftEl.className = leftClass;
                notes.push('no level reading ' + fmtAge(d.res_no_reading_s) + ' \u2014 minimum estimate');
            } else {
                leftEl.textContent = 'Left: no reading';
                leftEl.className = 'is-muted';
                notes.push('level sensor ' + (d.level_sensor || '?'));
            }
            if (d.reserve_empty) notes.unshift('RESERVOIR ALARM: ' + (d.reserve_alarm_text || '?') +
                (d.res_reading ? ' \u2014 refill, then Cycle Reset' : ' \u2014 refill, then Reservoir refilled'));
            else if (d.res_zone === 2) notes.push('at the alarm level');
            if (d.res_hours_left > 0) notes.push('\u2248 ' + fmtAge(d.res_hours_left * 3600) + ' to the alarm level');
            if (d.level_settling) notes.push('settling after pump');
            // Refill without a level reading — restarts the estimate from a full reservoir
            // (not in the first minute after boot — readings come back within ~30 s)
            document.getElementById('resRefillRow').style.display =
                (!d.res_reading && d.res_no_reading_s >= 60) ? '' : 'none';
            setTextMarkEstimate(note, notes.join(' \u00b7 '));
        }

        let lastStatus = null;

        // ============================================
        // MAIN STATUS UPDATE
        // ============================================
        function updateStatus() {
            secureFetch("api/status")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    lastStatus = data;

                    // Reserve state for the system badge
                    const hasReserveEmpty  = !!data.reserve_empty;
                    const hasLowResWarning = !!data.reserve_low || !!data.reserve_no_reading;
                    updateReserveBar(data);

                    // Badges
                    updateSensorBadge("sensor1Badge", data.sensor_active);
                    updatePumpBadge("pumpBadge", data.pump_active, data.pump_attempt || 0);
                    updateSystemBadge("systemBadge", data.system_error || hasReserveEmpty, data.system_disabled, hasLowResWarning);

                    // Process status
                    document.getElementById("processDescription").textContent = data.state_description || "IDLE - Waiting for sensors";

                    // System toggle sync
                    if (typeof data.system_disabled !== 'undefined') {
                        systemEnabled = !data.system_disabled;
                        updateSystemToggleButton(!data.system_disabled, data.service_remaining_s);
                    }

                    // Sync manual pump button with actual pump state
                    updatePumpButton(data);

                    // WiFi status
                    const wifiItem = document.getElementById("wifiItem");
                    wifiItem.className = 'status-main-wifi ' + (data.wifi_connected ? 'wifi-on' : 'wifi-off');

                    // RTC
                    const rtcItem = document.getElementById("rtcItem");
                    const rtcTime = document.getElementById("rtcTime");
                    const rtcHint = document.getElementById("rtcHint");
                    // Time only — rtc_time is "YYYY-MM-DD HH:MM:SS"
                    rtcTime.textContent = data.rtc_time ? data.rtc_time.split(" ").pop() : "Error";
                    if (data.rtc_battery_issue || data.rtc_needs_sync) {
                        rtcItem.className = "status-info rtc-warn";
                        rtcHint.textContent = " ⚠ Battery";
                    } else {
                        rtcItem.className = "status-info";
                        rtcHint.textContent = "";
                    }

                    // Memory & Uptime
                    document.getElementById("freeHeap").textContent = (data.free_heap / 1024).toFixed(1) + " KB";
                    document.getElementById("uptime").textContent = formatUptime(data.uptime);

                    // manualPumpBtn: enabled only in Service Mode (updatePumpButton)

                    updateCalibration(data);

                })
                .catch((error) => {
                    console.error("Status update failed:", error);
                });
        }


        // ============================================
        // DAILY VOLUME + AVAILABLE WATER + DOSE DISPLAY
        // Wszystkie dane z jednego endpointu: api/daily-volume
        // ============================================
        function loadDailyVolume() {
            secureFetch("api/daily-volume")
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data || !data.success) return;

                    const rolling  = data.rolling_24h_ml;
                    const limit    = data.daily_limit_ml;
                    const dose     = data.dose_ml;

                    // Added in 24h vs. daily limit bar
                    const pctUsed = limit > 0 ? Math.min((rolling / limit) * 100, 100) : 0;
                    document.getElementById("volumeBarFill").style.width = pctUsed + "%";
                    document.getElementById("volumeText").textContent = fmtL(rolling) + " / " + fmtL(limit);

                    // Populate inputs with current saved values (skip if user is editing)
                    const limitInp = document.getElementById("dailyLimitInput");
                    if (document.activeElement !== limitInp) limitInp.value = limit;
                })
                .catch((e) => console.error("loadDailyVolume error:", e));
        }

        // ============================================
        // SET DAILY LIMIT
        // ============================================
        function setDailyLimit() {
            const input = document.getElementById("dailyLimitInput");
            const value = parseInt(input.value);
            if (isNaN(value) || value < 100 || value > 10000) return;
            if (!confirm("Set daily refill limit to " + value + " ml?")) return;

            secureFetch("api/set-fill-water-max", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data) return;
                    if (data.success) { loadDailyVolume(); }
                    else alert(data.error || "Set daily limit failed");
                })
                .catch((e) => console.error("Set daily limit error:", e));
        }


        // ============================================
        // RESET DAILY VOLUME
        // ============================================
        function resetDailyVolume() {
            if (!confirm("Reset rolling 24h counter to 0?")) return;
            secureFetch("api/reset-daily-volume", { method: "POST" })
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (data && data.success) loadDailyVolume();
                    else console.error("Reset daily volume failed:", data && data.error);
                })
                .catch((e) => console.error("Reset daily volume error:", e));
        }

        // ============================================
        // LOGOUT
        // ============================================
        function logout() {
            fetch("api/logout", { method: "POST" }).then(() => {
                if (window.location.pathname.indexOf("/device/") !== -1) {
                    window.location.href = "/dashboard";
                } else {
                    window.location.href = "login";
                }
            });
        }

        // ============================================
        // CYCLE CHARTS
        // ============================================
        var CC = {
            bg:'#1e293b', grid:'rgba(148,163,184,0.10)', txt:'#94a3b8',
            ema:'#22d3ee', ok:'#4ade80', warn:'#fbbf24', err:'#f87171',
            boot:'#64748b',
            bandG:'rgba(74,222,128,0.13)',   // green  — normal range (±yellow_sigma)
            bandY:'rgba(251,191,36,0.13)',   // yellow — warning ring (between yellow and red)
            bandR:'rgba(239,68,68,0.12)'     // red    — alarm zone (outside ±red_sigma)
        };


        function fmtV(v) { return Math.round(v); }

        var CHART_Y_MAX_FACTOR = 3;   // auto Y-max = ema_rate * CHART_Y_MAX_FACTOR

        function drawRateChart(pts, ema) {
            var RING_SIZE = 60;
            var cv  = document.getElementById('chartRate');
            var H   = 190;
            var dpr = window.devicePixelRatio || 1;
            var W   = cv.parentElement.offsetWidth || 300;
            cv.style.width  = W + 'px';
            cv.style.height = H + 'px';
            cv.width  = Math.round(W * dpr);
            cv.height = Math.round(H * dpr);
            var ctx = cv.getContext('2d');
            ctx.scale(dpr, dpr);

            var p  = {t:10, r:6, b:10, l:6};
            var cw = W - p.l - p.r;
            var ch = H - p.t - p.b;

            // Background
            ctx.fillStyle = CC.bg;
            ctx.fillRect(0, 0, W, H);

            // Y-max: ema_rate * factor, fallback 300
            var hasEma = ema.ema_rate > 0.01;
            var yMax = hasEma ? ema.ema_rate * CHART_Y_MAX_FACTOR : 300;

            function yOf(v) { return p.t + (1 - Math.min(v, yMax) / yMax) * ch; }

            // Grid lines — 4 horizontal
            ctx.strokeStyle = CC.grid;
            ctx.lineWidth = 0.5;
            for (var gi = 1; gi <= 4; gi++) {
                var gy = p.t + ch * (1 - gi / 4);
                ctx.beginPath(); ctx.moveTo(p.l, gy); ctx.lineTo(p.l + cw, gy); ctx.stroke();
            }

            if (!pts.length) {
                ctx.fillStyle = CC.txt; ctx.font = '13px sans-serif'; ctx.textAlign = 'center';
                ctx.fillText('No data yet', W / 2, H / 2);
                return;
            }

            // Bar geometry: RING_SIZE slots, oldest left → newest right
            var slotW   = cw / RING_SIZE;
            var gapPx   = Math.max(1, slotW * 0.18);
            var barW    = Math.max(1, slotW - gapPx);
            var offset  = RING_SIZE - pts.length; // empty slots on the left

            // Bar colours — two alternating steel-blue tones
            var barClrA = 'rgba(79,140,190,0.88)';
            var barClrB = 'rgba(56,108,152,0.88)';
            var dotClr  = '#cbd5e1';
            var flareClr = '#f97316';  // orange for clipped bars

            for (var i = 0; i < pts.length; i++) {
                var slot  = offset + i;
                var rate  = pts[i].rate_ml_h || 0;
                if (rate <= 0) continue;

                var clipped = rate >= yMax;
                var barH    = clipped ? ch : Math.max(2, (rate / yMax) * ch);
                var bx      = p.l + slot * slotW + gapPx / 2;
                var by      = p.t + ch - barH;

                // Bar body
                ctx.fillStyle = (slot % 2 === 0) ? barClrA : barClrB;
                ctx.fillRect(bx, by, barW, barH);

                if (clipped) {
                    // Flare: bright orange cap + white centre dot
                    ctx.fillStyle = flareClr;
                    ctx.fillRect(bx, p.t, barW, 4);
                    ctx.beginPath();
                    ctx.arc(bx + barW / 2, p.t + 2, 2.5, 0, Math.PI * 2);
                    ctx.fillStyle = '#fff';
                    ctx.fill();
                } else {
                    // Normal tip dot
                    ctx.beginPath();
                    ctx.arc(bx + barW / 2, by, 2, 0, Math.PI * 2);
                    ctx.fillStyle = dotClr;
                    ctx.fill();
                }
            }

            // EMA dashed line
            if (hasEma) {
                var ye = yOf(ema.ema_rate);
                ctx.strokeStyle = '#94a3b8'; ctx.lineWidth = 1.5;
                ctx.setLineDash([6, 4]);
                ctx.beginPath(); ctx.moveTo(p.l, ye); ctx.lineTo(p.l + cw, ye); ctx.stroke();
                ctx.setLineDash([]);
            }

            // Legend: EMA value
            var elER = document.getElementById('legendEmaRate');
            if (elER) {
                elER.textContent = hasEma ? ('EMA ' + fmtV(ema.ema_rate) + ' ml/h') : '';
                elER.style.display = hasEma ? '' : 'none';
            }

            // Score badge
            var score = ema.alarm_score || 0;
            var badge = document.getElementById('scoreZoneBadge');
            if (badge) {
                if (hasEma) {
                    var zoneName, zoneClr;
                    if      (score >= ema.zone_yellow) { zoneName='RED';    zoneClr='#f87171'; }
                    else if (score >= ema.zone_green)  { zoneName='YELLOW'; zoneClr='#fbbf24'; }
                    else                               { zoneName='GREEN';  zoneClr='#4ade80'; }
                    badge.textContent = 'Score: ' + score.toFixed(1) + ' \u25B2 ' + zoneName;
                    badge.style.color = zoneClr;
                    badge.style.display = '';
                } else {
                    badge.style.display = 'none';
                }
            }
        }


        function deleteCycleHistory() {
            if (!confirm("Delete all cycle history and EMA data?\nThis cannot be undone.")) return;
            var btn = document.getElementById("deleteCyclesBtn");
            btn.disabled = true; btn.textContent = "Deleting...";
            secureFetch("api/clear-cycle-history", { method: "POST" })
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (data && data.success) {
                        loadCycleHistory();
                    } else {
                        alert("Delete failed: " + (data && data.error ? data.error : "unknown error"));
                    }
                })
                .catch(function(e) { console.error("Delete cycles error:", e); })
                .finally(function() { btn.disabled=false; btn.textContent="Delete Data"; });
        }

        function loadCycleHistory() {
            var btn = document.getElementById("loadCyclesBtn");
            btn.disabled = true; btn.textContent = "Loading...";

            secureFetch("api/cycle-history")
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (!data || !data.success) return;
                    var pts = (data.cycles || []).slice().reverse(); // oldest first
                    var ema = {
                        ema_rate:    data.ema_rate    || 0,
                        alarm_score: data.alarm_score || 0,
                        bootstrap:   data.bootstrap   || 0,
                        zone_green:  data.zone_green  || 20,
                        zone_yellow: data.zone_yellow || 50,
                        alarm_p1:    data.alarm_p1    || 0.10,
                        ema_clamp:   data.ema_clamp   || 0.40
                    };
                    // Show before drawing — the chart width comes from the container
                    document.getElementById('chartRateScroll').style.display = '';
                    drawRateChart(pts, ema);
                })
                .catch(function(e) { console.error("Load cycles error:", e); })
                .finally(function() { btn.disabled=false; btn.textContent="Load History"; });
        }

        // ============================================
        // ALG SETTINGS FORM
        // ============================================

        var algSettingsOpen = false;

        function toggleAlgSettings() {
            var panel = document.getElementById('algSettingsPanel');
            if (!panel) return;
            algSettingsOpen = !algSettingsOpen;
            panel.style.display = algSettingsOpen ? '' : 'none';
            if (algSettingsOpen) {
                secureFetch('api/alg-config')
                    .then(function(r) { return r ? r.json() : null; })
                    .then(function(d) {
                        if (!d || !d.success) return;
                        document.getElementById('algZoneGreen').value = d.zone_green;
                        document.getElementById('algZoneYellow').value= d.zone_yellow;
                        document.getElementById('algInitEma').value   = d.initial_ema;
                    })
                    .catch(function(e) { console.error('alg-config GET error:', e); });
                loadReserveConfig();
            }
        }

        // ============================================
        // RESERVOIR & LEVEL SENSOR SETTINGS
        // ============================================
        function showReserveConfig(d) {
            document.getElementById('resLength').value       = d.length_mm;
            document.getElementById('resWidth').value        = d.width_mm;
            document.getElementById('resSensorBottom').value = d.sensor_to_bottom_mm;
            document.getElementById('resMarker').value       = d.marker_level_mm;
            document.getElementById('resReservePct').value   = d.reserve_pct;
            document.getElementById('resAlarmPct').value     = d.alarm_pct;
            document.getElementById('resDerived').textContent =
                (d.saved ? '' : 'Defaults (not saved yet) \u2014 ') +
                d.ml_per_mm.toFixed(1) + ' ml/mm \u2022 capacity ' + (d.capacity_ml / 1000).toFixed(2) + ' l' +
                ' = ' + (d.full_ml / d.dose_ml) + ' portions of ' + d.dose_ml + ' ml' +
                ' \u2022 reserve ' + (d.reserve_ml / 1000).toFixed(2) + ' l (' + (d.reserve_ml / d.dose_ml) + ' portions)' +
                ' \u2022 alarm ' + (d.alarm_ml / 1000).toFixed(2) + ' l (' + (d.alarm_ml / d.dose_ml) +
                ' portions, ' + Math.round(d.alarm_mm) + ' mm)';
            var w = document.getElementById('resWarning');
            w.textContent = d.warning || '';
            w.style.display = d.warning ? '' : 'none';
        }

        // Highlight the drawing dimension that belongs to the focused / hovered input
        document.querySelectorAll('.res-geo-inputs label').forEach(function(lbl) {
            var dim = document.getElementById('dim' + lbl.dataset.dim);
            function on()  { lbl.classList.add('hl');    dim.classList.add('hl'); }
            function off() {
                if (lbl.contains(document.activeElement)) return;
                lbl.classList.remove('hl'); dim.classList.remove('hl');
            }
            lbl.addEventListener('mouseenter', on);
            lbl.addEventListener('mouseleave', off);
            lbl.querySelector('input').addEventListener('focus', on);
            lbl.querySelector('input').addEventListener('blur', function() {
                lbl.classList.remove('hl'); dim.classList.remove('hl');
            });
        });

        function loadReserveConfig() {
            secureFetch('api/reserve-config')
                .then(function(r) { return r ? r.json() : null; })
                .then(function(d) { if (d && d.success) showReserveConfig(d); })
                .catch(function(e) { console.error('reserve-config GET error:', e); });
        }

        function saveReserveConfig() {
            var errEl = document.getElementById('resError');
            errEl.style.display = 'none';
            var v = function(id) { return parseInt(document.getElementById(id).value, 10); };
            var len = v('resLength'), wid = v('resWidth'), bottom = v('resSensorBottom');
            var marker = v('resMarker');
            var resPct = v('resReservePct'), alarmPct = v('resAlarmPct');

            var err = '';
            if (isNaN(len) || len < 50 || len > 2000 || isNaN(wid) || wid < 50 || wid > 2000) err = 'Length/width: 50\u20132000 mm';
            else if (isNaN(bottom) || bottom < 70 || bottom > 1200)   err = 'Sensor \u2192 bottom: 70\u20131200 mm';
            else if (isNaN(marker) || marker < 20 || marker > bottom - 50) err = 'Marker level: 20 mm \u2026 (sensor \u2192 bottom \u2212 50 mm)';
            else if (isNaN(alarmPct) || alarmPct < 5 || alarmPct > 50) err = 'Alarm level: 5\u201350 %';
            else if (isNaN(resPct) || resPct <= alarmPct || resPct > 90) err = 'Reserve level: above the alarm level, max 90 %';
            if (err) { errEl.textContent = err; errEl.style.display = ''; return; }

            var btn = document.getElementById('resSaveBtn');
            btn.disabled = true; btn.textContent = 'Saving...';
            secureFetch('api/reserve-config', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'length_mm=' + len + '&width_mm=' + wid + '&sensor_to_bottom_mm=' + bottom +
                      '&marker_level_mm=' + marker + '&reserve_pct=' + resPct + '&alarm_pct=' + alarmPct
            })
            .then(function(r) { return r ? r.json() : Promise.reject('no response'); })
            .then(function(d) {
                if (!d || !d.success) return Promise.reject((d && d.error) || 'Save failed');
                showReserveConfig(d);
                btn.textContent = 'Saved';
                setTimeout(function() { btn.textContent = 'Save reservoir'; }, 3000);
            })
            .catch(function(e) {
                errEl.textContent = typeof e === 'string' ? e : 'Network error';
                errEl.style.display = '';
                btn.textContent = 'Save reservoir';
            })
            .finally(function() { btn.disabled = false; });
        }

        function saveAlgSettings() {
            var errEl = document.getElementById('algSettingsError');
            errEl.style.display = 'none';

            var zGreen  = parseFloat(document.getElementById('algZoneGreen').value);
            var zYellow = parseFloat(document.getElementById('algZoneYellow').value);
            var initEma = parseFloat(document.getElementById('algInitEma').value);

            var err = '';
            if (isNaN(zGreen) || zGreen < 1 || zGreen > 200)      err = 'Zone Green: 1–200';
            else if (isNaN(zYellow) || zYellow <= zGreen || zYellow > 500) err = 'Zone Yellow: must be > Zone Green and ≤ 500';
            else if (isNaN(initEma) || initEma < 1 || initEma > 500)   err = 'Initial EMA: 1–500 ml/h';

            if (err) {
                errEl.textContent = err;
                errEl.style.display = '';
                return;
            }

            var btn = document.getElementById('algSaveBtn');
            btn.disabled = true; btn.textContent = 'Saving...';

            var algBody = 'zone_green='   + zGreen   +
                          '&zone_yellow=' + zYellow  +
                          '&initial_ema=' + initEma;

            secureFetch('api/alg-config', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: algBody
            })
            .then(function(r) { return r ? r.json() : Promise.reject('no response'); })
            .then(function(d) {
                if (!d || !d.success) return Promise.reject((d && d.error) || 'Alg save failed');
                btn.textContent = 'Saved';
                setTimeout(function() { btn.textContent = 'Save'; }, 3000);
            })
            .catch(function(e) {
                var msg = typeof e === 'string' ? e : 'Network error';
                errEl.textContent = msg;
                errEl.style.display = '';
            })
            .finally(function() { btn.disabled = false; });
        }

        // ============================================
        // INITIALIZATION
        // ============================================

        // Register all intervals for cleanup on session expiry
        pollingIntervals.push(setInterval(updateStatus, 2000));
        pollingIntervals.push(setInterval(loadSystemState, 30000));
        pollingIntervals.push(setInterval(loadDailyVolume, 10000));

        // Initial loads
        updateStatus();
        loadSystemState();
        loadDailyVolume();
    </script>
</body>
</html>
)rawliteral";

