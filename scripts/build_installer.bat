@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

REM Usage: build_installer.bat [QtDir] [AppVersion] [Configuration] [IsccPath]
set "CONFIG=Release"
set "QTDIR=D:\Qt\6.10.2\msvc2022_64"
set "APPVER=1.0.0"
set "ISCC="

if not "%~1"=="" set "QTDIR=%~1"
if not "%~2"=="" set "APPVER=%~2"
if not "%~3"=="" set "CONFIG=%~3"
if not "%~4"=="" set "ISCC=%~4"

set "BUILD=build"
set "STAGING=dist\staging"
set "DIST=dist"
set "ISS=installer\AppEncrypt.iss"
set "MAIN=%BUILD%\%CONFIG%\AppEncrypt.exe"
set "STUB=%BUILD%\stub\%CONFIG%\AppEncryptStub.exe"
set "WINDEPLOY=%QTDIR%\bin\windeployqt.exe"

echo ==^> Check build outputs...
if not exist "%MAIN%" (
    echo [ERROR] Missing %MAIN%
    echo Run: cmake --build build --config %CONFIG%
    exit /b 1
)
if not exist "%STUB%" (
    echo [ERROR] Missing %STUB%
    exit /b 1
)
if not exist "%WINDEPLOY%" (
    echo [ERROR] Missing windeployqt: %WINDEPLOY%
    exit /b 1
)

echo ==^> Prepare staging: %STAGING%
if exist "%STAGING%" rmdir /s /q "%STAGING%"
mkdir "%STAGING%"

copy /Y "%MAIN%" "%STAGING%\AppEncrypt.exe" >nul
copy /Y "%STUB%" "%STAGING%\AppEncryptStub.exe" >nul
if exist "README.md" copy /Y "README.md" "%STAGING%\" >nul

echo ==^> Run windeployqt...
"%WINDEPLOY%" "%STAGING%\AppEncrypt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime --dir "%STAGING%"
if errorlevel 1 (
    echo [ERROR] windeployqt failed
    exit /b 1
)

echo ==^> Copy MSVC runtime if available...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%I"
    if defined VSPATH (
        for /f "delims=" %%D in ('dir /b /ad /o-n "%VSPATH%\VC\Redist\MSVC" 2^>nul') do (
            set "CRT=%VSPATH%\VC\Redist\MSVC\%%D\x64\Microsoft.VC143.CRT"
            if exist "!CRT!" (
                copy /Y "!CRT!\*.dll" "%STAGING%\" >nul 2>&1
                echo     Copied VC runtime from !CRT!
                goto crt_done
            )
        )
    )
)
:crt_done

echo ==^> Find Inno Setup compiler...
if defined ISCC goto have_iscc
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "D:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=D:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"

:have_iscc
if not defined ISCC (
    echo Staging ready: %CD%\%STAGING%
    echo Inno Setup 6 not found. Install it and rerun this script.
    exit /b 0
)

if not exist "%DIST%" mkdir "%DIST%"
set "STAGING_ABS=%CD%\%STAGING%"

echo ==^> Compile installer with ISCC...
"%ISCC%" "%ISS%" "/DStagingDir=%STAGING_ABS%" "/DMyAppVersion=%APPVER%"
if errorlevel 1 (
    echo [ERROR] ISCC failed
    exit /b 1
)

for /f "delims=" %%F in ('dir /b /o-d "%DIST%\AppEncrypt-Setup-*.exe" 2^>nul') do (
    echo Installer created: %CD%\%DIST%\%%F
    goto done
)
:done
exit /b 0
