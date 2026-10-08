@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Run-Stage05.ps1" %*
set "launchResult=%ERRORLEVEL%"
if "%launchResult%"=="0" exit /b 0
echo.
echo Cold start failed. See the error and log paths above.
if /i not "%~1"=="-SmokeTest" pause
exit /b %launchResult%
