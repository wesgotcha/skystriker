@echo off
cd /d "%~dp0"
if not exist build\SkyStriker.exe (
    echo Not built yet. Run build.bat first.
    pause
    exit /b 1
)
build\SkyStriker.exe %*
