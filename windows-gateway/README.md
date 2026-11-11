# CYD Satellite Tracker — Windows Gateway

Windows 10/11 background relay. CYD serves its local HTTP API; this program forwards telemetry to Cloudflare independently of the browser.

## Requirements
- Windows 10/11 and Python 3.11+
- CYD and PC on the same Wi-Fi/LAN
- Internet access on the PC

## First setup
1. Install Python 3.11+ and enable Add Python to PATH.
2. Run gateway.py once. It creates %LOCALAPPDATA%\CYDSatelliteTracker\config.json.
3. Edit that file. Set cyd_url to http://cyd-tracker.local or the CYD IP (for example http://192.168.0.48). Enter the one-time 8-digit website pairing code in pairing_code.
4. Run gateway.py from PowerShell and confirm gateway.log says the local API and cloud sync are working.
5. In this folder, run PowerShell as the current Windows user:
   Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
   .\install-startup.ps1
6. Start immediately:
   Start-ScheduledTask -TaskName 'CYD Satellite Tracker Gateway'

The scheduled task starts at Windows sign-in. It is independent of the browser, but stops after Windows sign-out/shutdown. It is a background logon task, not a Windows service.

## Security and recovery
- The device bearer token is encrypted using Windows DPAPI for the current Windows user.
- Telemetry is buffered in SQLite and retried after outages; up to 500 recent samples are retained.
- Logs are stored in %LOCALAPPDATA%\CYDSatelliteTracker\gateway.log.
- The gateway makes outbound HTTPS requests only; no public inbound port is opened.
- Do not expose the CYD local HTTP API to the public Internet.
- If the device token is revoked, delete telemetry.sqlite3, enter a fresh pairing code and restart. Deleting the database also clears queued telemetry.

## Scope of this first version
Telemetry relay only. Remote command relay and a Windows tray/settings interface are future work.
