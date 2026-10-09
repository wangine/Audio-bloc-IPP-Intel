@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-AudioCore.ps1" %*
set "stopResult=%ERRORLEVEL%"
if not "%stopResult%"=="0" pause
exit /b %stopResult%
