param([string]$QtRoot='C:/Qt/6.11.2/mingw_64', [string]$MingwRoot='C:/Qt/Tools/mingw1310_64')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source=Join-Path $root 'Integration/Stage2/source';$build=Join-Path $root 'local/stage011/stage2-build'
$env:PATH="$MingwRoot/bin;C:/Qt/Tools/Ninja;$QtRoot/bin;"+$env:PATH
$cmake='C:/Qt/Tools/CMake_64/bin/cmake.exe'
& $cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$MingwRoot/bin/g++.exe" "-DCMAKE_C_COMPILER=$MingwRoot/bin/gcc.exe"
if($LASTEXITCODE){throw 'Stage2 v0008 configure failed'}
& $cmake --build $build --parallel 4
if($LASTEXITCODE){throw 'Stage2 v0008 compile failed'}
# Keep the entire suite visible. V1 lease/demand integration fixtures are
# incompatible by design; never remove, skip or rewrite their expectations.
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $build --output-on-failure
if($LASTEXITCODE){throw 'Stage2 suite contains failures; inspect the named tests, including retained legacy V1 fixtures'}
