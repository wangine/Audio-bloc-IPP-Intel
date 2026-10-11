@echo off
setlocal
"%~dp0EntropyDspEngine.exe" --verify-only
if errorlevel 1 (
 echo Stage2 MUSIC package verification failed.
 pause
 exit /b 1
)
start "" "%~dp0EntropyDspEngine.exe" --runtime-root "%~dp0."