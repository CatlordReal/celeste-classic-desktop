@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall-CelesteClassic.ps1" %*
exit /b %ERRORLEVEL%
