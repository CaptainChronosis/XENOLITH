@echo off
setlocal
cd /d "%~dp0"
echo Creates a local Python environment and installs MIDI dependencies from PyPI.
echo Python 3.11 or newer must already be installed.
set "xenolithSetup="
set /p "xenolithSetup=Continue? [y/N] "
if /I not "%xenolithSetup%"=="y" exit /b 0
py -3 -m venv .venv-updater
if errorlevel 1 goto failed
.venv-updater\Scripts\python.exe -m pip install -r tools\updater-requirements.txt
if errorlevel 1 goto failed
echo Setup complete. Open Update.bat.
pause
exit /b 0
:failed
echo Setup failed. Install Python 3.11 or newer with the Windows py launcher, then retry.
pause
exit /b 1
