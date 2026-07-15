@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

REM Usage: unregister_shell.bat [InstallDir] [--system]
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
    exit /b 1
)

if "%SYSTEM%"=="1" (
    powershell -NoProfile -Command "Start-Process -FilePath '%EXE%' -ArgumentList 'unregister-shell --system' -Verb RunAs -Wait"
) else (
    "%EXE%" unregister-shell
)

if errorlevel 1 exit /b 1
echo Context menu removed.
exit /b 0
