@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
set "DR_ROOT=%~dp0.."
set "DR_BUILD=%DR_ROOT%\build\dynamic-recovery-msvc"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "%DR_ROOT%\source\Modules\EntropyDynamicRecovery" -B "%DR_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/Ninja/ninja.exe"
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "%DR_BUILD%" --parallel 4
if errorlevel 1 exit /b %errorlevel%
"C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir "%DR_BUILD%" --output-on-failure
exit /b %errorlevel%
