@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

REM Usage: register_shell.bat [InstallDir] [--system]
set "INSTALL_DIR=%~1"
set "SYSTEM=0"

if /i "%~1"=="--system" (
    set "SYSTEM=1"
    set "INSTALL_DIR="
)
if /i "%~2"=="--system" set "SYSTEM=1"

if "%INSTALL_DIR%"=="" set "INSTALL_DIR=%CD%\build\Release"

set "EXE=%INSTALL_DIR%\AppEncrypt.exe"
if not exist "%EXE%" (
    echo [ERROR] Not found: %EXE%
    echo Build first or pass InstallDir as first argument.
    exit /b 1
)

if "%SYSTEM%"=="1" (
    echo Requesting admin for system-wide registration...
    powershell -NoProfile -Command "Start-Process -FilePath '%EXE%' -ArgumentList 'register-shell --system' -Verb RunAs -Wait"
) else (
    "%EXE%" register-shell
)

if errorlevel 1 exit /b 1
echo Done. Right-click any .exe to see AppEncrypt menu items.
exit /b 0
