@echo off
title TruckDash - ponte USB (adb)
cd /d "%~dp0"
set "ADB=adb"
if exist "%~dp0platform-tools\adb.exe" set "ADB=%~dp0platform-tools\adb.exe"
if exist "%~dp0adb.exe" set "ADB=%~dp0adb.exe"
"%ADB%" version >nul 2>&1
if errorlevel 1 (
  echo ERRO: adb nao encontrado.
  echo Extraia o "platform-tools" do Android nesta pasta: %~dp0platform-tools\adb.exe
  pause
  exit /b
)
echo ============================================================
echo  TruckDash - ponte USB. Deixe esta janela aberta enquanto joga.
echo ============================================================
set "OK="
:loop
"%ADB%" get-state >nul 2>&1
if errorlevel 1 (
  if defined OK echo [%time:~0,8%] Tablet desconectado. Aguardando...
  if not defined OK echo Ligue o tablet no cabo USB com a "Depuracao USB" ativada...
  set "OK="
  "%ADB%" wait-for-device
)
"%ADB%" forward tcp:45456 tcp:45456 >nul 2>&1
if not defined OK (
  "%ADB%" shell am start -n com.fantasma.truckdash/android.app.NativeActivity >nul 2>&1
  echo [%time:~0,8%] Tablet conectado! Ponte USB ativa - pode abrir o ETS2.
  set "OK=1"
)
timeout /t 5 /nobreak >nul
goto loop
