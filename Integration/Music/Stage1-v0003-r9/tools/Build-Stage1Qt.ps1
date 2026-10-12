param()
$ErrorActionPreference='Stop'
$stage1ProjectRoot=Split-Path $PSScriptRoot -Parent
$env:PATH='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.2\mingw_64\bin;'+$env:PATH
$stage1Cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
& $stage1Cmake -S "$stage1ProjectRoot\UI\Qt" -B "$stage1ProjectRoot\build-qt" -G Ninja '-DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe' '-DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe' '-DCMAKE_PREFIX_PATH=C:\Qt\6.11.2\mingw_64' -DCMAKE_BUILD_TYPE=Release
if($LASTEXITCODE){throw 'Qt configure failed'}
& $stage1Cmake --build "$stage1ProjectRoot\build-qt" --parallel 4 *> "$stage1ProjectRoot\evidence\build-qt.txt"
$stage1Code=$LASTEXITCODE
Get-Content "$stage1ProjectRoot\evidence\build-qt.txt" -Tail 20
if($stage1Code){throw 'Qt build failed'}
