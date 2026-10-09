@echo off
rem Explicit manual hardware test session. No automatic ARM or fixed DAC/profile.
call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*
exit /b %ERRORLEVEL%
