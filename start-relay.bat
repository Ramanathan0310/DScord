@echo off
rem Double-click to start the DScord relay (runs the setup the first time).
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js is not installed. Get the LTS version from https://nodejs.org and try again.
  pause
  exit /b 1
)
node relay\relay.mjs %*
echo.
echo The relay has stopped.
pause
