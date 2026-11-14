@echo off
cd /d "%~dp0"
echo Friends Library - first Google connection
echo This launcher applies its PowerShell execution policy only to this process.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Connect-Windows.ps1"
echo.
pause
