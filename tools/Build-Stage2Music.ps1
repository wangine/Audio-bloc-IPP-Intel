param([string]$QtRoot='C:/Qt/6.11.2/mingw_64', [string]$MingwRoot='C:/Qt/Tools/mingw1310_64', [switch]$SkipTests)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskBuild=Join-Path $taskRoot 'build'
$env:PATH="$MingwRoot/bin;C:/Qt/Tools/Ninja;$QtRoot/bin;"+$env:PATH
$taskCmake='C:/Qt/Tools/CMake_64/bin/cmake.exe'
& $taskCmake -S (Join-Path $taskRoot 'source') -B $taskBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$MingwRoot/bin/g++.exe" "-DCMAKE_C_COMPILER=$MingwRoot/bin/gcc.exe"
if($LASTEXITCODE){throw 'Stage2 MUSIC configure failed'}
& $taskCmake --build $taskBuild --parallel 4
if($LASTEXITCODE){throw 'Stage2 MUSIC build failed'}
if(!$SkipTests){
 & 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $taskBuild --output-on-failure
 if($LASTEXITCODE){throw 'Inspect Stage2 suite failures; retained legacy incompatibilities must be reported'}
}
