

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
    <title>ATO & Kalkwasser Dosing - Login</title>
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
    <title>ATO & Kalkwasser Dosing</title>
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
        .sub-off    { color: var(--text-muted); }
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
            display: flex; align-items: center; flex-wrap: wrap; gap: 6px;
            background: var(--bg-input); border: 1px solid var(--border);
            border-radius: var(--radius-sm); padding: 8px 16px;
            font-size: 0.75rem; color: var(--text-secondary);
            font-family: 'Courier New', monospace; margin-top: 10px;
        }
        .status-info .si-sep { color: var(--text-muted); font-family: sans-serif; }
        .status-info.rtc-warn { border-color: rgba(251,191,36,0.35); }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }


        /* ===== SECOND CARD: Pump Control ===== */
        .pump-controls {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 12px;
        }
        @media (max-width: 480px) {
            .pump-controls { grid-template-columns: repeat(2, 1fr); }
        }
        .btn-kalk-off {
            background: rgba(234,179,8,0.08); border: 1px solid rgba(234,179,8,0.3);
            color: var(--accent-yellow);
        }
        .btn-kalk-off:hover:not(:disabled) { background: rgba(234,179,8,0.18); }
        .btn-service {
            background: rgba(249,115,22,0.10); border: 1px solid rgba(249,115,22,0.35);
            color: #f97316;
        }
        .btn-service:hover:not(:disabled) { background: rgba(249,115,22,0.20); }
        .btn-kalk-on {
            background: rgba(34,197,94,0.15); border: 1px solid rgba(34,197,94,0.3);
            color: var(--accent-green);
        }
        .btn-kalk-on:hover:not(:disabled) { background: rgba(34,197,94,0.25); }

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

        /* ===== THIRD CARD: Statistics ===== */
        .stats-columns {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
            align-items: start;
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

        /* Kalkwasser schedule tiles */
        .kalk-schedule-wrap {
            margin-bottom: 20px; 
        }
        .kalk-section {
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 12px;
            margin-top: 10px;
        }
        .kalk-section-label {
            font-size: 0.7rem; font-weight: 600; color: var(--text-primary);
            text-transform: uppercase; letter-spacing: 0.08em; margin-bottom: 8px;
        }
        .kalk-mix-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 6px;
        }
        .kalk-dose-grid {
            display: grid;
            grid-template-columns: repeat(8, 1fr);
            gap: 6px;
        }
        @media (max-width: 640px) {
            .kalk-dose-grid { grid-template-columns: repeat(4, 1fr); }
        }
        .kalk-tile {
            background: var(--bg-primary); border: 1px solid var(--border);
            border-radius: 6px; padding: 8px 4px; font-size: 0.78rem;
            font-family: 'Courier New', monospace; font-weight: 600;
            color: var(--text-secondary); cursor: default;
            text-align: center; transition: background 0.2s, color 0.2s;
        }
        .kalk-tile.kalk-ev-done    { background: rgba(74,222,128,0.1); border-color: rgba(74,222,128,0.3); color: #4ade80; }
        .kalk-tile.kalk-ev-active  { background: rgba(251,191,36,0.15); border-color: rgba(251,191,36,0.45); color: #fbbf24; font-weight: 700; }
        .kalk-tile.kalk-ev-pending { color: var(--text-secondary); }
        .kalk-tile.kalk-ev-missed  { color: rgba(248,113,113,0.45); border-color: rgba(248,113,113,0.15); }
        .kalk-alarm-banner {
            background: rgba(239,68,68,0.12); border: 1px solid rgba(239,68,68,0.3);
            color: #f87171; border-radius: 6px; padding: 8px 12px;
            font-size: 0.8rem; font-weight: 600; margin-top: 10px; display: none;
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
                <h1>ATO & Kalkwasser Dosing</h1>
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
                        <span id="sensor1Badge" class="sub-off">Sensors: OFF</span>
                        <span class="sub-sep">•</span>
                        <span id="pumpBadge" class="sub-off">Pump: OFF</span>
                        <span class="sub-sep">•</span>
                        <span id="kalkPumpBadge" class="sub-off">Kalk Pump: OFF</span>
                        <span class="sub-sep">•</span>
                        <span id="mixingPumpBadge" class="sub-off">Mix Pump: OFF</span>
                        <span class="sub-sep" id="reservoirSep" style="display:none">•</span>
                        <span id="reservoirAlarmBadge" style="display:none"></span>
                    </div>
                </div>
                <div class="status-main-wifi wifi-off" id="wifiItem">
                    <span class="wifi-label">WiFi</span>
                    <span class="wifi-dot" id="wifiStatus">●</span>
                </div>
            </div>
            <div class="status-info" id="rtcItem">
                <span id="rtcTime">—</span><span id="rtcHint"></span>
                <span class="si-sep">•</span>
                <span id="freeHeap">—</span>
                <span class="si-sep">•</span>
                <span id="uptime">—</span>
            </div>
            <!-- Kalkwasser schedule (tile grid) -->
            <div class="kalk-schedule-wrap" id="kalkScheduleWrap">
                <div class="kalk-alarm-banner" id="kalkAlarmBanner">
                    &#9888; No top-off events in last 24h — kalkwasser dose may be too high!
                </div>
                <div class="kalk-section">
                    <div class="kalk-section-label">Kalkwasser Mixing schedule</div>
                    <div class="kalk-mix-grid">
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix0">00:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix1">06:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix2">12:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix3">18:15</div>
                    </div>
                </div>
                <div class="kalk-section">
                    <div class="kalk-section-label">Kalkwasser Dosing schedule</div>
                    <div class="kalk-dose-grid">
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose0">02:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose1">03:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose2">04:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose3">05:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose4">08:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose5">09:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose6">10:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose7">11:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose8">14:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose9">15:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose10">16:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose11">17:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose12">20:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose13">21:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose14">22:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose15">23:00</div>
                    </div>
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
                <button id="kalkwasserBtn" class="btn btn-kalk-off" onclick="toggleKalkwasser()">Kalkwasser OFF</button>
                <button id="alarmBtn" class="btn btn-kalk-on" onclick="toggleAlarm()">Alert Sound ON</button>
                <button id="manualPumpBtn" class="btn btn-off" onclick="toggleManualPump()">Pump OFF</button>
                <button id="mixingPumpBtn" class="btn btn-off" onclick="toggleMixingPump()">Mixing Pump OFF</button>
                <button id="peristalticPumpBtn" class="btn btn-off" onclick="togglePeristalticPump()">Peristaltic OFF</button>
                <button id="systemResetBtn" class="btn btn-control" onclick="systemReset()">Cycle Reset</button>
                <button id="volBtn" class="btn btn-control" onclick="handleVolBtnClick(event)" title="lewa 30% = ciszej, prawa 30% = głośniej" style="display:flex;align-items:center;padding-left:0;padding-right:0;gap:0;"><span style="flex:0 0 30%;text-align:center;">&#8722;</span><span id="volCenterLabel" style="flex:1;text-align:center;">Vol 3/5</span><span style="flex:0 0 30%;text-align:center;">+</span></button>
            </div>
        </div>

        <!-- THIRD CARD: Statistics -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(234, 179, 8, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-yellow);" viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zM9 17H7v-7h2v7zm4 0h-2V7h2v10zm4 0h-2v-4h2v4z"/></svg>
                </div>
                <h2>System Setting</h2>
            </div>

 

            <div class="stats-columns">

                <!-- Column 1: Kalkwasser Dosing Settings -->
                <div class="stat-column">
                    <h3>Kalkwasser Dosing - Current Value</h3>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="kalkDailyDose" min="1" max="5000" step="1" placeholder="ml/day">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="saveKalkConfig()">Save</button>
                    </div>
                </div>

                <!-- Column 2: Single Dose -->
                <div class="stat-column stat-col-dose">
                    <h3>Single Dose - Current Value</h3>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="doseInput" min="1" max="2000" step="1" placeholder="ml">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="setDose()">Set</button>
                    </div>
                </div>
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
            <div style="display:flex;gap:8px;margin-bottom:10px;">
                <button class="btn btn-secondary" onclick="loadCycleHistory()" id="loadCyclesBtn" style="flex:1;">Load History</button>
                <button class="btn btn-secondary" onclick="deleteCycleHistory()" id="deleteCyclesBtn" style="flex:1;color:#f87171;border-color:rgba(248,113,113,0.3);">Delete Data</button>
            </div>
            <div style="background:var(--bg-input);border-radius:var(--radius-sm);padding:10px 12px;margin-bottom:10px;">
                <div style="display:flex;justify-content:space-between;align-items:baseline;margin-bottom:5px;">
                    <span style="font-size:0.72rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;">Rolling 24h Refill</span>
                    <span id="volumeText" style="font-size:0.8rem;color:var(--text-secondary);">0 ml / 2000 ml</span>
                </div>
                <div class="volume-bar"><div class="volume-bar-fill" id="volumeBarFill"></div></div>
                <div style="display:flex;gap:6px;margin-top:8px;align-items:center;">
                    <input type="number" id="dailyLimitInput" min="100" max="10000" step="100" placeholder="limit ml" style="flex:1;min-width:0;">
                    <button class="btn btn-secondary btn-small" onclick="setDailyLimit()">Set</button>
                    <button class="btn btn-off btn-small" onclick="resetDailyVolume()">Reset</button>
                </div>
            </div>
            <div class="chart-wrap">
                <div style="font-size:0.7rem;font-weight:600;color:var(--text-muted);letter-spacing:0.05em;text-transform:uppercase;margin-bottom:5px;padding-left:2px;">Evaporation Rate (ml/h)</div>
                <div id="chartRateScroll" class="chart-rate-scroll">
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
                    <div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px 14px;">
                        <label style="font-size:0.72rem;color:var(--text-muted);">EMA Alpha (0.05–0.50)
                            <input type="number" id="algAlpha" min="0.05" max="0.50" step="0.01"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">EMA Clamp (0.10–0.80)
                            <input type="number" id="algClamp" min="0.10" max="0.80" step="0.01"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">P1 spike weight (0.01–2.0)
                            <input type="number" id="algP1" min="0.01" max="2.0" step="0.01"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">P2 decay speed (0.05–0.99)
                            <input type="number" id="algP2" min="0.05" max="0.99" step="0.01"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">Zone Green (1–200)
                            <input type="number" id="algZoneGreen" min="1" max="200" step="0.5"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">Zone Yellow (&gt;green, max 500)
                            <input type="number" id="algZoneYellow" min="1" max="500" step="0.5"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">Initial EMA ml/h (1–500)
                            <input type="number" id="algInitEma" min="1" max="500" step="1"
                                style="width:100%;margin-top:3px;"></label>
                        <label style="font-size:0.72rem;color:var(--text-muted);">Chart Y-Max ml/h (blank=auto)
                            <input type="number" id="algChartYMax" min="10" max="5000" step="10"
                                placeholder="auto" style="width:100%;margin-top:3px;"></label>
                    </div>
                    <div id="algSettingsError" style="display:none;color:#f87171;font-size:0.72rem;margin-top:8px;"></div>
                    <div style="display:flex;gap:8px;margin-top:10px;">
                        <button class="btn btn-primary btn-small" onclick="saveAlgSettings()" id="algSaveBtn">Save</button>
                        <button class="btn btn-secondary btn-small" onclick="toggleAlgSettings()">Cancel</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- FOURTH CARD: Pump Setting -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(249, 115, 22, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-orange);" viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
                </div>
                <h2>Pump Calibration</h2>
            </div>

            <div class="card-subheader">ATO Pump Calibration</div>
            <div class="settings-row">
                <div class="setting-item">
                    <button id="extendedBtn" class="btn btn-off" onclick="toggleAtoCalibration()">Calibration OFF</button>
                </div>
                <div class="setting-item input-group">
                    <label for="calibrationMl" style="text-align: center;">Mililiters</label>
                    <input type="number" id="calibrationMl" min="1" max="3000" step="1" placeholder="—">
                    <label for="calibrationSeconds" style="text-align: center;">Seconds</label>
                    <input type="number" id="calibrationSeconds" min="0" max="3600" step="1" value="0" readonly>
                    <small id="calibrationCurrent" style="color:#aaa; margin-top:4px;">current: —</small>
                </div>
                <div class="setting-item">
                    <button class="btn btn-primary" onclick="updateVolumePerSecond()">Update Setting</button>
                </div>
            </div>

            <div class="card-subheader">Kalkwasser Pump Calibration</div>
            <div class="settings-row">
                <div class="setting-item">
                    <button id="kalkCalibBtn" class="btn btn-off" onclick="toggleKalkCalibration()">Calibration OFF</button>
                </div>
                <div class="setting-item input-group">
                    <label for="kalkMeasuredMl" style="text-align: center;">Mililiters per 30 Seconds)</label>
                    <input type="number" id="kalkMeasuredMl" min="0.1" max="500" step="0.1" placeholder="15.3">
                </div>
                <div class="setting-item">
                    <button class="btn btn-primary" onclick="saveKalkFlowRate()">Save Result</button>
                </div>
            </div>

        </div>

        <!-- Footer -->
        <div class="footer-info">
            ATO & Kalkwasser Dosing• v3.0
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
            if (pumpCountdownTimer) { clearInterval(pumpCountdownTimer); pumpCountdownTimer = null; }
            if (mixingCountdownTimer) { clearInterval(mixingCountdownTimer); mixingCountdownTimer = null; }
            if (peristalticCountdownTimer) { clearInterval(peristalticCountdownTimer); peristalticCountdownTimer = null; }

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
        let alarmLevel = 3;  // poziomy 0-5, domyślnie 3 = głośność 20

        function volLevelToVol(l) { return (l + 1) * 5; }
        function volToLevel(v)    { return Math.round(v / 5) - 1; }

        function updateVolBtn() {
            var lbl = document.getElementById('volCenterLabel');
            if (lbl) lbl.textContent = 'Vol ' + alarmLevel + '/5';
        }

        function handleVolBtnClick(e) {
            var el = e.currentTarget;
            var x  = e.clientX - el.getBoundingClientRect().left;
            var w  = el.offsetWidth;
            var nl = alarmLevel;
            if      (x < w * 0.30) nl = Math.max(0, alarmLevel - 1);
            else if (x > w * 0.70) nl = Math.min(5, alarmLevel + 1);
            else return;
            if (nl === alarmLevel) return;
            alarmLevel = nl;
            updateVolBtn();
            secureFetch('api/audio-volume', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'volume=' + volLevelToVol(nl)
            }).then(function(r){ return r ? r.json() : null; })
              .then(function(d){ if (d && !d.success) { alarmLevel = nl; updateVolBtn(); } })
              .catch(function(){});
        }

        // Manual pump countdown state
        var pumpCountdownTimer = null;
        var pumpOnTimeMs = 0;
        var MANUAL_PUMP_DURATION_S = 60;

        // Mixing pump countdown state
        var mixingCountdownTimer = null;
        var mixingOnTimeMs = 0;
        var MIXING_PUMP_DURATION_S = 300;

        // Peristaltic pump countdown state
        var peristalticCountdownTimer = null;
        var peristalticOnTimeMs = 0;
        var PERISTALTIC_PUMP_DURATION_S = 60;

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

        function updateSystemToggleButton(enabled) {
            const btn = document.getElementById("systemToggleBtn");
            if (!btn) return;

            if (enabled) {
                btn.textContent = "Auto Mode";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = "Service Mode";
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
        // ALARM AUDIO TOGGLE (bistable ON/OFF)
        // ============================================
        function toggleAlarm() {
            const btn = document.getElementById("alarmBtn");
            btn.disabled = true;

            secureFetch("api/alarm-toggle", { method: "POST" })
                .then((response) => {
                    if (!response) { btn.disabled = false; return; }
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) updateAlarmButton(data.muted);
                    btn.disabled = false;
                })
                .catch((error) => {
                    console.error("Toggle alarm error:", error);
                    btn.disabled = false;
                });
        }

        function updateAlarmButton(muted) {
            const btn = document.getElementById("alarmBtn");
            if (!btn) return;
            if (muted) {
                btn.textContent = "Alert Sound OFF";
                btn.className = "btn btn-kalk-off";
            } else {
                btn.textContent = "Alert Sound ON";
                btn.className = "btn btn-kalk-on";
            }
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
        // MANUAL PUMP (bistable + 60s countdown)
        // ============================================
        function toggleManualPump() {
            var btn = document.getElementById("manualPumpBtn");
            var isOn = btn.classList.contains("btn-primary");
            btn.disabled = true;
            if (isOn) {
                fetch("api/pump/direct-off", { method: "POST" })
                    .then(function(r) { return r.json(); })
                    .then(function(data) { if (data.success) updatePumpButton(false); btn.disabled = false; })
                    .catch(function(e) { console.error("Pump stop error:", e); btn.disabled = false; });
            } else {
                fetch("api/pump/direct-on", { method: "POST" })
                    .then(function(r) { return r.json(); })
                    .then(function(data) {
                        if (data.success) { pumpOnTimeMs = Date.now(); updatePumpButton(true); startPumpCountdown(); }
                        else console.error("Failed to start pump");
                        btn.disabled = false;
                    })
                    .catch(function(e) { console.error("Pump start error:", e); btn.disabled = false; });
            }
        }

        function startPumpCountdown() {
            if (pumpCountdownTimer) clearInterval(pumpCountdownTimer);
            pumpCountdownTimer = setInterval(function() {
                var elapsed = Math.floor((Date.now() - pumpOnTimeMs) / 1000);
                var remaining = MANUAL_PUMP_DURATION_S - elapsed;
                var btn = document.getElementById("manualPumpBtn");
                if (!btn || !btn.classList.contains("btn-primary")) {
                    clearInterval(pumpCountdownTimer); pumpCountdownTimer = null; return;
                }
                if (remaining <= 0) {
                    clearInterval(pumpCountdownTimer); pumpCountdownTimer = null; return;
                }
                btn.textContent = "Pump ON " + remaining + "s";
            }, 1000);
        }

        function updatePumpButton(isOn) {
            var btn = document.getElementById("manualPumpBtn");
            if (!btn) return;
            if (isOn) {
                if (!pumpOnTimeMs) pumpOnTimeMs = Date.now();
                if (!pumpCountdownTimer) startPumpCountdown();
                var remaining = Math.max(0, MANUAL_PUMP_DURATION_S - Math.floor((Date.now() - pumpOnTimeMs) / 1000));
                btn.textContent = "Pump ON " + remaining + "s";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = "Pump OFF";
                btn.className = "btn btn-off";
                if (pumpCountdownTimer) { clearInterval(pumpCountdownTimer); pumpCountdownTimer = null; }
                pumpOnTimeMs = 0;
            }
        }

        // ============================================
        // MIXING PUMP DIRECT (bistable + 300s countdown)
        // ============================================
        function toggleMixingPump() {
            const btn = document.getElementById("mixingPumpBtn");
            const isOn = btn.classList.contains("btn-primary");
            btn.disabled = true;
            secureFetch("api/mixing-pump", {
                method: "POST",
                headers: {"Content-Type": "application/x-www-form-urlencoded"},
                body: "action=" + (isOn ? "off" : "on")
            })
            .then(function(r) { if (!r) return null; return r.json(); })
            .then(function(data) {
                if (data && data.success) {
                    if (data.active) mixingOnTimeMs = Date.now();
                    updateMixingPumpButton(data.active);
                } else if (data) console.error("Mixing pump error:", data.error);
                btn.disabled = false;
            })
            .catch(function(e) { console.error("Mixing pump error:", e); btn.disabled = false; });
        }

        function startMixingCountdown() {
            if (mixingCountdownTimer) clearInterval(mixingCountdownTimer);
            mixingCountdownTimer = setInterval(function() {
                var elapsed = Math.floor((Date.now() - mixingOnTimeMs) / 1000);
                var remaining = MIXING_PUMP_DURATION_S - elapsed;
                var btn = document.getElementById("mixingPumpBtn");
                if (!btn || !btn.classList.contains("btn-primary")) {
                    clearInterval(mixingCountdownTimer); mixingCountdownTimer = null; return;
                }
                if (remaining <= 0) { clearInterval(mixingCountdownTimer); mixingCountdownTimer = null; return; }
                btn.textContent = "Mixing ON " + remaining + "s";
            }, 1000);
        }

        function updateMixingPumpButton(isOn, kalkState) {
            var btn = document.getElementById("mixingPumpBtn");
            if (!btn) return;
            if (isOn && kalkState === "DIRECT_MIX") {
                if (!mixingOnTimeMs) mixingOnTimeMs = Date.now();
                if (!mixingCountdownTimer) startMixingCountdown();
                var remaining = Math.max(0, MIXING_PUMP_DURATION_S - Math.floor((Date.now() - mixingOnTimeMs) / 1000));
                btn.textContent = "Mixing ON " + remaining + "s";
                btn.className = "btn btn-primary";
                btn.disabled = false;
            } else if (isOn) {
                // Scheduled cycle — do not interfere
                btn.textContent = "Mixing ON (auto)";
                btn.className = "btn btn-primary";
                btn.disabled = true;
                if (mixingCountdownTimer) { clearInterval(mixingCountdownTimer); mixingCountdownTimer = null; }
                mixingOnTimeMs = 0;
            } else {
                btn.textContent = "Mixing Pump OFF";
                btn.className = "btn btn-off";
                btn.disabled = false;
                if (mixingCountdownTimer) { clearInterval(mixingCountdownTimer); mixingCountdownTimer = null; }
                mixingOnTimeMs = 0;
            }
        }

        // ============================================
        // PERISTALTIC PUMP DIRECT (bistable + 60s countdown)
        // ============================================
        function togglePeristalticPump() {
            const btn = document.getElementById("peristalticPumpBtn");
            const isOn = btn.classList.contains("btn-primary");
            btn.disabled = true;
            secureFetch("api/peristaltic-pump", {
                method: "POST",
                headers: {"Content-Type": "application/x-www-form-urlencoded"},
                body: "action=" + (isOn ? "off" : "on")
            })
            .then(function(r) { if (!r) return null; return r.json(); })
            .then(function(data) {
                if (data && data.success) {
                    if (data.active) peristalticOnTimeMs = Date.now();
                    updatePeristalticPumpButton(data.active);
                } else if (data) console.error("Peristaltic pump error:", data.error);
                btn.disabled = false;
            })
            .catch(function(e) { console.error("Peristaltic pump error:", e); btn.disabled = false; });
        }

        function startPeristalticCountdown() {
            if (peristalticCountdownTimer) clearInterval(peristalticCountdownTimer);
            peristalticCountdownTimer = setInterval(function() {
                var elapsed = Math.floor((Date.now() - peristalticOnTimeMs) / 1000);
                var remaining = PERISTALTIC_PUMP_DURATION_S - elapsed;
                var btn = document.getElementById("peristalticPumpBtn");
                if (!btn || !btn.classList.contains("btn-primary")) {
                    clearInterval(peristalticCountdownTimer); peristalticCountdownTimer = null; return;
                }
                if (remaining <= 0) { clearInterval(peristalticCountdownTimer); peristalticCountdownTimer = null; return; }
                btn.textContent = "Peristaltic ON " + remaining + "s";
            }, 1000);
        }

        function updatePeristalticPumpButton(isOn, kalkState) {
            var btn = document.getElementById("peristalticPumpBtn");
            if (!btn) return;
            if (isOn && kalkState === "DIRECT_DOSE") {
                if (!peristalticOnTimeMs) peristalticOnTimeMs = Date.now();
                if (!peristalticCountdownTimer) startPeristalticCountdown();
                var remaining = Math.max(0, PERISTALTIC_PUMP_DURATION_S - Math.floor((Date.now() - peristalticOnTimeMs) / 1000));
                btn.textContent = "Peristaltic ON " + remaining + "s";
                btn.className = "btn btn-primary";
                btn.disabled = false;
            } else if (isOn) {
                // Scheduled cycle — do not interfere
                btn.textContent = "Peristaltic ON (auto)";
                btn.className = "btn btn-primary";
                btn.disabled = true;
                if (peristalticCountdownTimer) { clearInterval(peristalticCountdownTimer); peristalticCountdownTimer = null; }
                peristalticOnTimeMs = 0;
            } else {
                btn.textContent = "Peristaltic OFF";
                btn.className = "btn btn-off";
                btn.disabled = false;
                if (peristalticCountdownTimer) { clearInterval(peristalticCountdownTimer); peristalticCountdownTimer = null; }
                peristalticOnTimeMs = 0;
            }
        }

        // ============================================
        // ATO CALIBRATION (bistable toggle)
        // ============================================
        let calibrationActive  = false;
        let calibrationTimer   = null;
        let calibrationElapsed = 0;

        function toggleAtoCalibration() {
            const btn = document.getElementById("extendedBtn");
            if (!calibrationActive) {
                btn.disabled = true;
                fetch("api/pump/calibration-on", { method: "POST" })
                    .then(r => r.json())
                    .then(data => {
                        if (data.success) {
                            calibrationActive  = true;
                            calibrationElapsed = 0;
                            document.getElementById("calibrationSeconds").value = 0;
                            updateAtoCalibBtn(true);
                            calibrationTimer = setInterval(() => {
                                calibrationElapsed++;
                                document.getElementById("calibrationSeconds").value = calibrationElapsed;
                            }, 1000);
                        }
                    })
                    .catch(e => console.error("Calibration start error:", e))
                    .finally(() => { btn.disabled = false; });
            } else {
                btn.disabled = true;
                clearInterval(calibrationTimer);
                calibrationTimer  = null;
                calibrationActive = false;
                updateAtoCalibBtn(false);
                fetch("api/pump/calibration-off", { method: "POST" })
                    .catch(e => console.error("Calibration stop error:", e))
                    .finally(() => { btn.disabled = false; });
            }
        }

        function updateAtoCalibBtn(isOn) {
            const btn = document.getElementById("extendedBtn");
            if (!btn) return;
            btn.textContent = isOn ? "Calibration ON" : "Calibration OFF";
            btn.className   = "btn " + (isOn ? "btn-primary" : "btn-off");
        }

        // ============================================
        // VOLUME SETTINGS
        // ============================================
        function loadVolumePerSecond() {
            secureFetch("api/pump-settings")
                .then(r => { if (!r) return null; return r.json(); })
                .then(data => {
                    if (!data || !data.success) return;
                    const vps = parseFloat(data.volume_per_second);
                    if (vps > 0) {
                        const el = document.getElementById("calibrationCurrent");
                        if (el) el.textContent = "current: " + vps.toFixed(2) + " ml/s";
                    }
                })
                .catch(e => console.error("Failed to load pump settings:", e));
        }

        function updateVolumePerSecond() {
            const ml  = parseInt(document.getElementById("calibrationMl").value, 10);
            const sec = parseInt(document.getElementById("calibrationSeconds").value, 10);

            if (isNaN(ml) || ml <= 0 || isNaN(sec) || sec <= 0) {
                alert("Run calibration first: Mililiters and Seconds must both be greater than 0.");
                return;
            }

            fetch("api/pump-settings", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "volume_per_second=" + (ml / sec)
            })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        loadVolumePerSecond();
                    }
                })
                .catch(e => console.error("Failed to update pump settings:", e));
        }

        // ============================================
        // STATUS BADGE UPDATES
        // ============================================
        function updateSensorBadge(badgeId, isActive) {
            const el = document.getElementById(badgeId);
            if (!el) return;
            el.textContent = 'Sensors: ' + (isActive ? 'ON' : 'OFF');
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

        function updateKalkPumpBadge(isActive) {
            const el = document.getElementById('kalkPumpBadge');
            if (!el) return;
            el.textContent = 'Kalk Pump: ' + (isActive ? 'ON' : 'OFF');
            el.className = isActive ? 'sub-on' : 'sub-off';
        }

        function updateMixingPumpBadge(isActive) {
            const el = document.getElementById('mixingPumpBadge');
            if (!el) return;
            el.textContent = 'Mix Pump: ' + (isActive ? 'ON' : 'OFF');
            el.className = isActive ? 'sub-on' : 'sub-off';
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
                    
                    // Low reservoir alarm badges
                    const hasLowResError   = !!data.low_reservoir_error;
                    const hasLowResWarning = !!data.low_reservoir_warning && !hasLowResError;
                    const resCount         = data.low_reservoir_count || 0;
                    const reservoirSep   = document.getElementById("reservoirSep");
                    const reservoirBadge = document.getElementById("reservoirAlarmBadge");
                    if (hasLowResError) {
                        reservoirSep.style.display   = '';
                        reservoirBadge.style.display = '';
                        reservoirBadge.className     = 'sub-danger';
                        reservoirBadge.textContent   = '\u26a0 Reservoir empty \u2014 refill & reset';
                    } else if (hasLowResWarning) {
                        reservoirSep.style.display   = '';
                        reservoirBadge.style.display = '';
                        reservoirBadge.className     = 'sub-warn';
                        reservoirBadge.textContent   = '\u26a0 Low water reservoir (' + resCount + '/3)';
                    } else {
                        reservoirSep.style.display   = 'none';
                        reservoirBadge.style.display = 'none';
                    }

                    // Badges
                    updateSensorBadge("sensor1Badge", data.sensor_active);
                    updateSensorBadge("sensor2Badge", data.sensor_active);
                    updatePumpBadge("pumpBadge", data.pump_active, data.pump_attempt || 0);
                    updateKalkPumpBadge(data.peristaltic_pump_active || false);
                    updateMixingPumpBadge(data.mixing_pump_active || false);
                    updateSystemBadge("systemBadge", data.system_error, data.system_disabled, hasLowResWarning);

                    // Process status
                    document.getElementById("processDescription").textContent = data.state_description || "IDLE - Waiting for sensors";

                    // System toggle sync
                    if (typeof data.system_disabled !== 'undefined') {
                        systemEnabled = !data.system_disabled;
                        updateSystemToggleButton(!data.system_disabled);
                    }

                    // Sync manual pump button with actual pump state
                    updatePumpButton(data.pump_active);
                    updateMixingPumpButton(data.mixing_pump_active || false, data.kalk_state || "");
                    updatePeristalticPumpButton(data.peristaltic_pump_active || false, data.kalk_state || "");
                    if (typeof data.audio_muted  !== 'undefined') updateAlarmButton(data.audio_muted);
                    if (typeof data.audio_volume !== 'undefined') { alarmLevel = volToLevel(data.audio_volume); updateVolBtn(); }

                    // WiFi status
                    const wifiItem = document.getElementById("wifiItem");
                    wifiItem.className = 'status-main-wifi ' + (data.wifi_connected ? 'wifi-on' : 'wifi-off');

                    // RTC
                    const rtcItem = document.getElementById("rtcItem");
                    const rtcTime = document.getElementById("rtcTime");
                    const rtcHint = document.getElementById("rtcHint");
                    rtcTime.textContent = data.rtc_time || "Error";
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

                    // Note: manualPumpBtn is NOT disabled when system_disabled — direct pump bypasses system state

                    const extendedBtn = document.getElementById("extendedBtn");
                    if (extendedBtn) {
                        // Sync calibration state on page reload
                        if (data.direct_pump_mode && !calibrationActive) {
                            calibrationActive = true;
                            updateAtoCalibBtn(true);
                        } else if (!data.direct_pump_mode && calibrationActive) {
                            calibrationActive = false;
                            clearInterval(calibrationTimer);
                            calibrationTimer = null;
                            updateAtoCalibBtn(false);
                        }
                        // Disable only when algo pump is active (not during user calibration)
                        extendedBtn.disabled = data.pump_active && !calibrationActive;
                    }

                    // Kalkwasser schedule update
                    if (data.rtc_ts) {
                        updateKalkSchedule(data.rtc_ts,
                                           data.kalk_mix_done_bits  || 0,
                                           data.kalk_dose_done_bits || 0,
                                           data.kalk_state || 'IDLE',
                                           data.kalk_enabled || false,
                                           data.kalk_alarm || false);
                    }
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

                    // Rolling 24h Refill Limit bar
                    const pctUsed = limit > 0 ? Math.min((rolling / limit) * 100, 100) : 0;
                    document.getElementById("volumeBarFill").style.width = pctUsed + "%";
                    document.getElementById("volumeText").textContent = rolling + " ml / " + limit + " ml";

                    // Populate inputs with current saved values (skip if user is editing)
                    const limitInp = document.getElementById("dailyLimitInput");
                    if (document.activeElement !== limitInp) limitInp.value = limit;
                    const doseInp = document.getElementById("doseInput");
                    if (document.activeElement !== doseInp) doseInp.value = dose;
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
                    else console.error("Set daily limit failed:", data.error);
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
        // SET SINGLE DOSE
        // ============================================
        function setDose() {
            const input = document.getElementById("doseInput");
            const value = parseInt(input.value);
            if (isNaN(value) || value < 1 || value > 2000) return;
            if (!confirm("Set single dose to " + value + " ml?")) return;

            secureFetch("api/set-dose", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data) return;
                    if (data.success) { loadDailyVolume(); }
                    else console.error("Set dose failed:", data.error);
                })
                .catch((e) => console.error("Set dose error:", e));
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

        function drawTimeAxisScrollable(ctx, minTs, maxTs, W, H, p) {
            var DAY_S = 86400;
            var tsRange = Math.max(maxTs - minTs, 1);
            var cw = W - p.l - p.r;
            function xTs(ts) { return p.l + (ts - minTs) / tsRange * cw; }

            // Alternating day background bands (every other day slightly lighter)
            var firstDay = Math.floor(minTs / DAY_S) * DAY_S;
            for (var d = firstDay, idx = 0; d < maxTs; d += DAY_S, idx++) {
                if (idx % 2 === 0) continue;
                var x1 = Math.max(p.l, xTs(d));
                var x2 = Math.min(p.l + cw, xTs(d + DAY_S));
                if (x2 > x1) {
                    ctx.fillStyle = 'rgba(255,255,255,0.025)';
                    ctx.fillRect(x1, p.t, x2 - x1, H - p.t - p.b);
                }
            }

            // Hour tick labels (HH:MM) — always 1h intervals since view = 6h
            var firstTick = Math.ceil(minTs / 3600) * 3600;
            ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
            for (var ts = firstTick; ts <= maxTs; ts += 3600) {
                var d = new Date(ts * 1000);
                ctx.fillStyle = '#94a3b8';
                ctx.fillText(
                    String(d.getHours()).padStart(2,'0') + ':' + String(d.getMinutes()).padStart(2,'0'),
                    xTs(ts), H - p.b + 13
                );
            }

            // Midnight separators + day date labels (DD.MM)
            var firstMid = Math.ceil(minTs / DAY_S) * DAY_S;
            for (var ts = firstMid; ts <= maxTs; ts += DAY_S) {
                var x = xTs(ts);
                ctx.save();
                ctx.strokeStyle = 'rgba(148,163,184,0.18)';
                ctx.lineWidth = 1; ctx.setLineDash([3, 4]);
                ctx.beginPath(); ctx.moveTo(x, p.t); ctx.lineTo(x, H - p.b); ctx.stroke();
                ctx.setLineDash([]); ctx.restore();
                var d = new Date(ts * 1000);
                var lbl = String(d.getDate()).padStart(2,'0') + '.' + String(d.getMonth()+1).padStart(2,'0');
                ctx.fillStyle = '#f97316'; ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
                ctx.fillText(lbl, x, H - p.b + 26);
            }

            // Left-edge date label for the starting day (when it's not at midnight)
            var d0 = new Date(minTs * 1000);
            if (d0.getHours() !== 0 || d0.getMinutes() !== 0) {
                var lbl0 = String(d0.getDate()).padStart(2,'0') + '.' + String(d0.getMonth()+1).padStart(2,'0');
                ctx.fillStyle = 'rgba(249,115,22,0.6)'; ctx.font = '10px sans-serif'; ctx.textAlign = 'left';
                ctx.fillText(lbl0, p.l + 3, H - p.b + 26);
            }
        }

        var CHART_Y_MAX_FACTOR = 3;   // auto Y-max = ema_rate * CHART_Y_MAX_FACTOR
        var chartYMaxOverride = null; // null = auto; set by user in algSettingsPanel (RAM only)

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

            // Y-max: user override or auto (ema_rate * factor, fallback 300)
            var hasEma = ema.ema_rate > 0.01;
            var autoYMax = hasEma ? ema.ema_rate * CHART_Y_MAX_FACTOR : 300;
            var yMax = (chartYMaxOverride !== null && chartYMaxOverride > 0)
                       ? chartYMaxOverride : autoYMax;

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
                        var cv = document.getElementById('chartRate');
                        if (cv) { var ctx = cv.getContext('2d'); ctx.clearRect(0,0,cv.width,cv.height); }
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
                        document.getElementById('algAlpha').value     = d.ema_alpha;
                        document.getElementById('algClamp').value     = d.ema_clamp;
                        document.getElementById('algP1').value        = d.alarm_p1;
                        document.getElementById('algP2').value        = d.alarm_p2;
                        document.getElementById('algZoneGreen').value = d.zone_green;
                        document.getElementById('algZoneYellow').value= d.zone_yellow;
                        document.getElementById('algInitEma').value   = d.initial_ema;
                        document.getElementById('algChartYMax').value = chartYMaxOverride !== null ? chartYMaxOverride : '';
                    })
                    .catch(function(e) { console.error('alg-config GET error:', e); });
            }
        }

        function saveAlgSettings() {
            var errEl = document.getElementById('algSettingsError');
            errEl.style.display = 'none';

            var alpha   = parseFloat(document.getElementById('algAlpha').value);
            var clamp   = parseFloat(document.getElementById('algClamp').value);
            var p1      = parseFloat(document.getElementById('algP1').value);
            var p2      = parseFloat(document.getElementById('algP2').value);
            var zGreen  = parseFloat(document.getElementById('algZoneGreen').value);
            var zYellow = parseFloat(document.getElementById('algZoneYellow').value);
            var initEma = parseFloat(document.getElementById('algInitEma').value);
            var chartYMaxRaw = document.getElementById('algChartYMax').value.trim();
            var chartYMaxVal = chartYMaxRaw === '' ? NaN : parseFloat(chartYMaxRaw);

            var err = '';
            if (isNaN(alpha)   || alpha < 0.05 || alpha > 0.50)        err = 'EMA Alpha: 0.05–0.50';
            else if (isNaN(clamp) || clamp < 0.10 || clamp > 0.80)     err = 'EMA Clamp: 0.10–0.80';
            else if (isNaN(p1)  || p1 < 0.01 || p1 > 2.0)             err = 'P1: 0.01–2.0';
            else if (isNaN(p2)  || p2 < 0.05 || p2 > 0.99)            err = 'P2: 0.05–0.99';
            else if (isNaN(zGreen) || zGreen < 1 || zGreen > 200)      err = 'Zone Green: 1–200';
            else if (isNaN(zYellow) || zYellow <= zGreen || zYellow > 500) err = 'Zone Yellow: must be > Zone Green and ≤ 500';
            else if (isNaN(initEma) || initEma < 1 || initEma > 500)   err = 'Initial EMA: 1–500 ml/h';
            else if (!isNaN(chartYMaxVal) && (chartYMaxVal < 10 || chartYMaxVal > 5000)) err = 'Chart Y-Max: 10–5000 ml/h (or blank for auto)';

            if (err) {
                errEl.textContent = err;
                errEl.style.display = '';
                return;
            }

            var btn = document.getElementById('algSaveBtn');
            btn.disabled = true; btn.textContent = 'Saving...';

            var body = 'ema_alpha='   + alpha   +
                       '&ema_clamp='  + clamp   +
                       '&alarm_p1='   + p1      +
                       '&alarm_p2='   + p2      +
                       '&zone_green=' + zGreen  +
                       '&zone_yellow='+ zYellow +
                       '&initial_ema='+ initEma;

            secureFetch('api/alg-config', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: body
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(d) {
                if (d && d.success) {
                    // chart Y-max is RAM-only — store before closing panel
                    chartYMaxOverride = isNaN(chartYMaxVal) ? null : chartYMaxVal;
                    toggleAlgSettings();
                    alert('Algorithm settings saved.');
                } else {
                    errEl.textContent = (d && d.error) ? d.error : 'Save failed';
                    errEl.style.display = '';
                }
            })
            .catch(function(e) {
                errEl.textContent = 'Network error';
                errEl.style.display = '';
            })
            .finally(function() { btn.disabled=false; btn.textContent='Save'; });
        }

        // ============================================
        // KALKWASSER
        // ============================================

        var KALK_MIX_BASES  = [0, 6, 12, 18];
        var KALK_DOSE_HOURS = [2,3,4,5,8,9,10,11,14,15,16,17,20,21,22,23];

        function updateKalkSchedule(rtcTs, mixDoneBits, doseDoneBits, kalkState, kalkEnabled, kalkAlarm) {
            var wrap = document.getElementById('kalkScheduleWrap');
            if (!wrap) return;

            var banner = document.getElementById('kalkAlarmBanner');
            if (banner) banner.style.display = (kalkEnabled && kalkAlarm) ? '' : 'none';

            var now = new Date(rtcTs * 1000);
            var nowH = now.getHours();
            var nowTotalMin = nowH * 60 + now.getMinutes();

            function setTile(id, cls) {
                var el = document.getElementById(id);
                if (el) el.className = 'kalk-tile kalk-ev-' + cls;
            }

            // Mix slots: [0,6,12,18]:15
            // Bit i in mixDoneBits = slot i was executed today (set by firmware).
            var isMixState = (kalkState === 'MIXING' || kalkState === 'WAIT_MIX');
            KALK_MIX_BASES.forEach(function(base, i) {
                var slotMin   = base * 60 + 15;
                var isNowSlot = (nowTotalMin >= slotMin && nowTotalMin < slotMin + 360);
                var slotDone  = !!(mixDoneBits & (1 << i));
                var cls;
                if      (isMixState && isNowSlot) cls = 'active';   // yellow — mixing/settling now
                else if (slotDone)                cls = 'done';     // green — confirmed executed today
                else if (slotMin > nowTotalMin)   cls = 'pending';  // white — future
                else                              cls = 'missed';   // red — past, not executed
                setTile('kalkMix' + i, cls);
            });

            // Dose slots: 02-05, 08-11, 14-17, 20-23
            // Bit i in doseDoneBits = slot i was executed today (set by firmware).
            var isDosing = (kalkState === 'DOSING' || kalkState === 'WAIT_DOSE');
            KALK_DOSE_HOURS.forEach(function(h, i) {
                var slotDone = !!(doseDoneBits & (1 << i));
                var cls;
                if      (isDosing && nowH === h) cls = 'active';   // yellow — dosing now
                else if (slotDone)               cls = 'done';     // green — confirmed executed today
                else if (h > nowH)               cls = 'pending';  // white — future
                else                             cls = 'missed';   // red — past, not executed
                setTile('kalkDose' + i, cls);
            });
        }

        var kalkCalibActive    = false;
        var kalkCalibTimer     = null;
        var kalkCalibRemaining = 0;

        function updateKalkCalibBtn(isOn) {
            var btn = document.getElementById('kalkCalibBtn');
            if (!btn) return;
            btn.textContent = isOn ? ('Calibration ON ' + kalkCalibRemaining + 's') : 'Calibration OFF';
            btn.className   = 'btn ' + (isOn ? 'btn-primary' : 'btn-off');
        }

        function toggleKalkCalibration() {
            if (kalkCalibActive) return;
            var btn = document.getElementById('kalkCalibBtn');
            btn.disabled = true;
            secureFetch('api/kalkwasser-calibrate', { method: 'POST' })
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (data && data.success) {
                        kalkCalibActive    = true;
                        kalkCalibRemaining = data.duration_s || 30;
                        btn.disabled = false;
                        updateKalkCalibBtn(true);
                        kalkCalibTimer = setInterval(function() {
                            kalkCalibRemaining--;
                            if (kalkCalibRemaining <= 0) {
                                clearInterval(kalkCalibTimer);
                                kalkCalibTimer  = null;
                                kalkCalibActive = false;
                                updateKalkCalibBtn(false);
                            } else {
                                updateKalkCalibBtn(true);
                            }
                        }, 1000);
                    } else {
                        alert('Calibration failed: ' + (data && data.error ? data.error : 'pump busy'));
                        btn.disabled = false;
                    }
                })
                .catch(function() { btn.disabled = false; });
        }

        function saveKalkFlowRate() {
            var ml = parseFloat(document.getElementById('kalkMeasuredMl').value);
            if (!ml || ml <= 0) { alert('Enter measured volume in ml'); return; }
            secureFetch('api/kalkwasser-flow-rate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'measured_ml=' + ml
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
                if (data && data.success) {
                    var fr = data.flow_rate_ul_s || 0;
                    if (fr > 0) {
                        document.getElementById('kalkMeasuredMl').placeholder = (fr / 1000 * 30).toFixed(1);
                    }
                }
            });
        }

        var kalkIsEnabled = false;

        function updateKalkButton(enabled) {
            kalkIsEnabled = enabled;
            var btn = document.getElementById('kalkwasserBtn');
            if (!btn) return;
            if (enabled) {
                btn.className = 'btn btn-kalk-on';
                btn.textContent = 'Kalkwasser ON';
            } else {
                btn.className = 'btn btn-kalk-off';
                btn.textContent = 'Kalkwasser OFF';
            }
        }

        function toggleKalkwasser() {
            secureFetch('api/kalkwasser-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'enabled=' + (kalkIsEnabled ? 0 : 1)
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
                if (data && data.success) updateKalkButton(!!data.enabled);
            });
        }

        function saveKalkConfig() {
            var dose = parseInt(document.getElementById('kalkDailyDose').value);
            if (isNaN(dose) || dose < 1 || dose > 5000) return;
            if (!confirm("Set Kalkwasser daily dose to " + dose + " ml?")) return;
            secureFetch('api/kalkwasser-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'enabled=' + (kalkIsEnabled ? 1 : 0) + '&daily_dose_ml=' + dose
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
            });
        }

        function loadKalkConfig() {
            secureFetch('api/kalkwasser-config')
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (!data || !data.success) return;
                    updateKalkButton(!!data.enabled);
                    var dose = data.daily_dose_ml || 0;
                    var inp = document.getElementById('kalkDailyDose');
                    if (inp && document.activeElement !== inp) inp.value = dose;
                    var fr = data.flow_rate_ul_per_s || 0;
                    if (fr > 0) {
                        document.getElementById('kalkMeasuredMl').placeholder = (fr / 1000 * 30).toFixed(1);
                    }
                });
        }

        // ============================================
        // INITIALIZATION
        // ============================================

        // Register all intervals for cleanup on session expiry
        pollingIntervals.push(setInterval(updateStatus, 2000));
        pollingIntervals.push(setInterval(loadSystemState, 30000));
        pollingIntervals.push(setInterval(loadDailyVolume, 10000));
        pollingIntervals.push(setInterval(loadKalkConfig, 30000));

        // Initial loads
        updateVolBtn();
        updateStatus();
        loadSystemState();
        loadVolumePerSecond();
        loadDailyVolume();
        loadKalkConfig();
    </script>
</body>
</html>
)rawliteral";

