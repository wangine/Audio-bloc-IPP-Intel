param(
    [string]$BuildFolder='build/msvc-stage04-blockers',
    [string]$ResultsFolder='local/stage04-blockers/etw',
    [string]$Python='python'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    $results=[IO.Path]::GetFullPath((Join-Path $root $ResultsFolder))
    if(!$results.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Trace analysis results must stay inside this repository'}
    $tids=@('normal','mmcss') | ForEach-Object {
        (Get-Content (Join-Path $results "$_/scheduling.json") -Raw | ConvertFrom-Json).thread_id
    }
    & "$BuildFolder/EntropyStage04EtwDecode.exe" (Join-Path $results 'scheduler.etl') (Join-Path $results 'decoded') @tids *> (Join-Path $results 'decode.txt')
    if($LASTEXITCODE){throw 'ETW decode failed: no attribution is accepted'}
    & $Python -B -O Tests/Stage04EtwCorrelationTests.py *> (Join-Path $results 'correlation-tests.txt')
    if($LASTEXITCODE){throw 'ETW interval positive controls failed'}
    & $Python -B tools/Correlate_Stage04_Etw.py $results *> (Join-Path $results 'correlation.txt')
    if($LASTEXITCODE){throw 'ETW correlation failed: no attribution is accepted'}
    Get-Content (Join-Path $results 'attribution-summary.json')
    Write-Output 'Analysis complete. Wall-clock deadline outcomes are unchanged. No hardware was invoked.'
} finally {Pop-Location}
