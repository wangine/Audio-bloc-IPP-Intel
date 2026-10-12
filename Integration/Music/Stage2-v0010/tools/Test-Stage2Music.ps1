param([switch]$IncludeLegacy)
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$env:PATH='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;C:/Qt/6.11.2/mingw_64/bin;'+$env:PATH
$taskCtest='C:/Qt/Tools/CMake_64/bin/ctest.exe'
& $taskCtest --test-dir (Join-Path $taskRoot 'build') -R '^DspMusic' --output-on-failure --parallel 2
if($LASTEXITCODE){throw 'Stage2 MUSIC tests failed'}
& $taskCtest --test-dir (Join-Path $taskRoot 'build') -R '^(DspState|DspCenterState|DspEqState|DspCenterNative|DspEqNative|DspAlignment|DspCenterContract|StageBusContract|StageBusDemand|StageBusProcess)$' --output-on-failure
if($LASTEXITCODE){throw 'Retained Stage2 functional checks failed'}
if($IncludeLegacy){
 & $taskCtest --test-dir (Join-Path $taskRoot 'build') --output-on-failure --parallel 4
 if($LASTEXITCODE){throw 'Full suite has retained V1/10ch/11ch fixture failures; see STAGE2-MUSIC-v0009-REPORT.md'}
}
