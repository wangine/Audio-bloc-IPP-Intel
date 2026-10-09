@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" 10.0.26100.0
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=10.0.26100.0
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "%~dp0build" --parallel 4
