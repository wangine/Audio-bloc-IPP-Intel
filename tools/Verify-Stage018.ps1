param([string]$NativeBuild='build/msvc-thiran')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    $cache=Get-Content -LiteralPath (Join-Path $NativeBuild 'CMakeCache.txt')
    $python=($cache | Where-Object {$_ -match '^_?Python3_EXECUTABLE:(FILEPATH|INTERNAL)='} | Select-Object -First 1) -replace '^[^=]+=',''
    if(!$python){throw 'Configure the native build first.'}
    $cmake=(Get-Command cmake.exe -ErrorAction Stop).Source
    $ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
    & $ctest --test-dir $NativeBuild -R '^EntropyStage018View$' --output-on-failure
    if($LASTEXITCODE){throw 'FFT/axis/time regression failed'}
    & $python Tests/Stage018Integrity.py
    if($LASTEXITCODE){throw 'DSP source identity failed'}
    Write-Host 'PASS Stage018 display checks only. No live benchmark or physical output.'
} finally {Pop-Location}
