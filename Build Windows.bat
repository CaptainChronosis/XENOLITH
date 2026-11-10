@echo off
cd /d "%~dp0"
set "XENOLITH_PYTHON=python"
if exist ".venv\Scripts\python.exe" set "XENOLITH_PYTHON=.venv\Scripts\python.exe"
"%XENOLITH_PYTHON%" tools\build_windows.py %*
if errorlevel 1 (echo Build failed. No firmware was launched. & pause & exit /b 1)
echo Firmware is in app\build.
pause
