@echo off
cd /d "%~dp0"
echo Instalando o plugin TruckDash no Euro Truck Simulator 2 / American Truck Simulator...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar.ps1"
echo.
pause
