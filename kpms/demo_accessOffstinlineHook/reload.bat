@echo off
REM Quick reload script for Windows

set MODULE_NAME=kpm-inline-access
set MODULE_FILE=accessOffstinlineHook.kpm

echo ===================================
echo Module Reload Script
echo ===================================
echo.

if not exist %MODULE_FILE% (
    echo Error: %MODULE_FILE% not found
    echo Please run 'make' first
    exit /b 1
)

echo [1/4] Unloading old module...
adb shell su -c "kpm unload %MODULE_NAME%" 2>nul
timeout /t 1 /nobreak >nul

echo [2/4] Pushing new module...
adb push %MODULE_FILE% /data/local/tmp/

echo [3/4] Loading new module...
adb shell su -c "kpm load /data/local/tmp/%MODULE_FILE%"

echo [4/4] Verifying...
adb shell su -c "kpm info %MODULE_NAME%"

echo.
echo ===================================
echo Done! Module reloaded.
echo ===================================
echo.
echo Test with:
echo   adb shell /data/local/tmp/kpm_control pengxintu123 help
