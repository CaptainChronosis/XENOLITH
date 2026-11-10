@echo off
setlocal
title XX29 / XENOLITH DISPATCH
color 04
cd /d "%~dp0"
if exist ".venv-updater\Scripts\python.exe" (
  ".venv-updater\Scripts\python.exe" tools\update.py %*
) else (
  echo Run Setup updater.bat once before using this menu.
  pause
  exit /b 2
)
set "xenolithExit=%errorlevel%"
if not "%xenolithExit%"=="0" echo Operation did not complete. Read the message above.
pause
exit /b %xenolithExit%
