param([ValidateSet('pcm','pipeline','architecture','all')][string]$Case='all')
$stage1ProjectRoot=Split-Path $PSScriptRoot -Parent
if($Case -eq 'all'){
 & 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir "$stage1ProjectRoot\build" --output-on-failure
}else{ & "$stage1ProjectRoot\build\Stage1Acceptance.exe" "--$Case" }
if($LASTEXITCODE){throw "Stage1 acceptance failed: $Case"}
