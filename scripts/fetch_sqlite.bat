@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

set "DEST=third_party\sqlite"
set "VERSION=3450100"
set "URL=https://www.sqlite.org/2024/sqlite-amalgamation-%VERSION%.zip"
set "ZIP=%TEMP%\sqlite-amalgamation.zip"
set "EXTRACT=%TEMP%\sqlite-amalgamation"

if not exist "%DEST%" mkdir "%DEST%"

echo Downloading SQLite amalgamation...
curl -fsSL -o "%ZIP%" "%URL%"
if errorlevel 1 (
    echo [ERROR] curl download failed: %URL%
    exit /b 1
)

if exist "%EXTRACT%" rmdir /s /q "%EXTRACT%"
mkdir "%EXTRACT%"

tar -xf "%ZIP%" -C "%EXTRACT%" 2>nul
if errorlevel 1 (
    powershell -NoProfile -Command "Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%EXTRACT%' -Force"
    if errorlevel 1 (
        echo [ERROR] failed to extract zip
        exit /b 1
    )
)

set "FOUND=0"
for /d %%D in ("%EXTRACT%\*") do (
    if exist "%%D\sqlite3.c" (
        copy /Y "%%D\sqlite3.c" "%DEST%\" >nul
        copy /Y "%%D\sqlite3.h" "%DEST%\" >nul
        copy /Y "%%D\sqlite3ext.h" "%DEST%\" >nul
        set "FOUND=1"
    )
)

if "!FOUND!"=="0" (
    echo [ERROR] sqlite3.c not found in archive
    exit /b 1
)

echo Done: %CD%\%DEST%
exit /b 0
