@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\apply_update.ps1"
exit /b %errorlevel%
