@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Stage1-Launch.ps1" -Action Start %*
if errorlevel 1 (
 echo Stage1 MUSIC start failed. See the error above and state\core-stderr.txt.
 if not defined STAGE1_NO_PAUSE pause
 exit /b 1
)
exit /b 0
