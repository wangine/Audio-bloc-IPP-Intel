@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_MAKE_PROGRAM="C:\Qt\Tools\Ninja\ninja.exe" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "%~dp0build" --config Release
exit /b %errorlevel%
