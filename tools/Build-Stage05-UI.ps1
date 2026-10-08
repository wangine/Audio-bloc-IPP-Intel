param([string]$QtRoot='C:\Qt\6.11.2\mingw_64',[string]$BuildFolder='build\qt-stage05')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$env:PATH='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;'+$env:PATH
$cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
& $cmake -S (Join-Path $root 'UI') -B (Join-Path $root $BuildFolder) -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" '-DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe'
if($LASTEXITCODE){throw 'Qt configure FAIL'}
& $cmake --build (Join-Path $root $BuildFolder) --parallel 4
if($LASTEXITCODE){throw 'Qt build FAIL'}
