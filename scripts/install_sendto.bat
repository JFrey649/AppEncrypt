@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

REM Usage: install_sendto.bat <InstallDir>
if "%~1"=="" (
    echo Usage: install_sendto.bat InstallDir
    echo Example: install_sendto.bat "C:\Program Files\AppEncrypt"
    exit /b 1
)

set "INSTALL_DIR=%~1"
set "TARGET=%INSTALL_DIR%\AppEncrypt.exe"
if not exist "%TARGET%" (
    echo [ERROR] Not found: %TARGET%
    exit /b 1
)

set "SENDTO=%APPDATA%\Microsoft\Windows\SendTo"
set "LNK=%SENDTO%\AppEncrypt 加密.lnk"

cscript //nologo "%~dp0create_shortcut.vbs" "%TARGET%" "%LNK%" "%INSTALL_DIR%" "使用 AppEncrypt 加密所选 exe"
if errorlevel 1 exit /b 1

echo Created: %LNK%
echo In Explorer: right-click exe -^> Send To -^> AppEncrypt 加密
exit /b 0
