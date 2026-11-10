@echo off
setlocal
cd /d "%~dp0"
echo Creates a local Python environment and installs MIDI dependencies from PyPI.
echo Python 3.12 with the Windows py launcher must already be installed.
py -3.12 -c "import sys; assert sys.version_info[:2] == (3, 12)" >nul 2>&1
if errorlevel 1 goto missingpython
set "xenolithSetup="
set /p "xenolithSetup=Continue? [y/N] "
if /I not "%xenolithSetup%"=="y" exit /b 0
if exist .venv-updater\Scripts\python.exe (
  .venv-updater\Scripts\python.exe -c "import sys; assert sys.version_info[:2] == (3, 12)" >nul 2>&1
  if errorlevel 1 goto wrongenvironment
)
py -3.12 -m venv .venv-updater
if errorlevel 1 goto failed
.venv-updater\Scripts\python.exe -m pip install --only-binary=:all: -r tools\updater-requirements.txt
if errorlevel 1 goto failed
echo Setup complete. Open Update.bat.
pause
exit /b 0
:failed
echo Setup failed. Check the network connection and the error above, then retry.
pause
exit /b 1
:missingpython
echo Python 3.12 was not found. Install it with the Windows py launcher, then retry.
pause
exit /b 1
:wrongenvironment
echo The existing .venv-updater uses another Python version.
echo Rename that folder to preserve it, then run this setup again.
pause
exit /b 1
