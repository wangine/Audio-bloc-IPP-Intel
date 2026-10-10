param([string]$NativeBuild='build/msvc-thiran',[string]$UiBuild='build/qt-thiran',[string]$QtRoot='C:/Qt/6.11.2/mingw_64',[string]$MingwRoot='C:/Qt/Tools/mingw1310_64')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    $cache=Get-Content -LiteralPath (Join-Path $NativeBuild 'CMakeCache.txt')
    $python=($cache | Where-Object {$_ -match '^_?Python3_EXECUTABLE:(FILEPATH|INTERNAL)='} | Select-Object -First 1) -replace '^[^=]+=',''
    if(!$python){throw 'Configure the native build first (tools/Build-Windows.ps1).'}
    $cmake=(Get-Command cmake.exe -ErrorAction Stop).Source
    $ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
    & $ctest --test-dir $NativeBuild -R '^EntropyStage017(Control|Lifecycle|Preparation|Transport)$' --output-on-failure
    if($LASTEXITCODE){throw 'Targeted native regressions failed'}
    $env:PATH="$QtRoot/bin;$MingwRoot/bin;"+$env:PATH
    $env:QT_QPA_PLATFORM='offscreen'
    & (Join-Path $UiBuild 'EntropyStage017Ui.exe')
    if($LASTEXITCODE){throw 'UI backend regression failed'}
    & $python Tests/Stage017Integrity.py
    if($LASTEXITCODE){throw 'Source integrity regression failed'}
    Write-Host 'PASS targeted Stage01 checks. No hardware, source transport or timed qualification opened.'
} finally {Pop-Location}
