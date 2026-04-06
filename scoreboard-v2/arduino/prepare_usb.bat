@echo off
echo Droylsden Cricket Club - Prepare USB Drive
echo.
powershell.exe -ExecutionPolicy Bypass -File "%~dp0prepare_usb.ps1"
pause
