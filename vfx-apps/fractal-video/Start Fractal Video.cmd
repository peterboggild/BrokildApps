@echo off
rem Starts the Fractal Video server (same as "npm start") and opens the app in the browser.
rem Close the server window to stop it. If it is already running, this just opens the app.
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js was not found. Install it from https://nodejs.org/ and try again.
  pause
  exit /b 1
)
start "Fractal Video server" /min node server.js
timeout /t 2 /nobreak >nul
start "" http://localhost:4173/
