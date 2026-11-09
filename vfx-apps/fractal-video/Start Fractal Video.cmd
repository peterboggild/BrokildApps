@echo off
rem Starts the Fractal Video server (same as "npm start") and opens the app in the browser.
rem Close the server window to stop it. If it is already running, this just opens the app.
rem
rem It also brings the folder up to date first, when this is a git clone and git is installed.
rem The usual way this goes wrong is running a months-old copy without noticing: the app looks
rem almost right, the newest fractals are missing, and a project exported from the current
rem version does not come back the same. A fast-forward-only pull cannot overwrite local edits -
rem it just declines, says so, and starts what you have.
cd /d "%~dp0"

where node >nul 2>&1
if errorlevel 1 (
  echo Node.js was not found. Install it from https://nodejs.org/ and try again.
  pause
  exit /b 1
)

where git >nul 2>&1
if not errorlevel 1 call :update

start "Fractal Video server" /min node server.js
timeout /t 2 /nobreak >nul
start "" http://localhost:4173/
exit /b 0

:update
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 exit /b 0
echo Updating Fractal Video...
git pull --ff-only
if errorlevel 1 (
  echo.
  echo   Could not update - offline, or you have local changes here.
  echo   Starting the version you already have.
  timeout /t 4 /nobreak >nul
)
exit /b 0
