@echo off
REM Build script for Windows using NDK

set NDK_PATH=D:\moywdr\temp_D\android_studio\SDK\ndk\29.0.14033849
set NDK_BUILD=%NDK_PATH%\ndk-build.cmd

echo ==========================================
echo Building kpm_control with NDK
echo ==========================================
echo.

REM Check if NDK exists
if not exist "%NDK_BUILD%" (
    echo Error: NDK not found at %NDK_BUILD%
    echo Please update NDK_PATH in this script
    exit /b 1
)

REM Clean previous build
echo Cleaning previous build...
if exist libs rmdir /s /q libs
if exist obj rmdir /s /q obj

REM Build
echo Building...
call "%NDK_BUILD%" NDK_PROJECT_PATH=. NDK_APPLICATION_MK=jni/Application.mk

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed!
    exit /b 1
)

echo.
echo ==========================================
echo Build successful!
echo ==========================================
echo.
echo Output: libs\arm64-v8a\kpm_control
echo.
echo To install to device:
echo   adb push libs\arm64-v8a\kpm_control /data/local/tmp/
echo   adb shell chmod 755 /data/local/tmp/kpm_control
echo.
