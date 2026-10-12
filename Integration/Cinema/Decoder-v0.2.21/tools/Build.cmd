@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "%~dp0.." -B "%~dp0..\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "%~dp0..\build" --parallel 2
exit /b %errorlevel%

