param([string]$BuildFolder='build/msvc-stage04-blockers',[string]$ResultsFolder='local/stage04-blockers/etw')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'BLOCKED: kernel CSwitch ETW requires an elevated Windows token'}
$results=[IO.Path]::GetFullPath((Join-Path $root $ResultsFolder))
if(!$results.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Trace results must stay within this repository'}
New-Item -ItemType Directory -Force $results | Out-Null
$xperf='C:/Program Files (x86)/Windows Kits/10/Windows Performance Toolkit/xperf.exe'
$exe=Join-Path $root "$BuildFolder/EntropyStage04Validation.exe"
try {
$loggers=& $xperf -loggers 2>&1
$loggers | Set-Content (Join-Path $results 'existing-loggers.txt')
if(($loggers -join "`n") -match 'NT Kernel Logger'){throw 'BLOCKED: an existing NT Kernel Logger must not be replaced/stopped'}
$trace=Join-Path $results 'scheduler.etl'
& $xperf -on PROC_THREAD+CSWITCH+DISPATCHER+DPC+INTERRUPT -clocktype perfcounter -f $trace *> (Join-Path $results 'start.txt')
if($LASTEXITCODE){throw 'Kernel scheduler trace start failed'}
try {
    & $exe (Join-Path $results 'normal') --benchmark *> (Join-Path $results 'normal-execution.txt')
    $normal=$LASTEXITCODE
    & $exe (Join-Path $results 'mmcss') --mmcss-benchmark *> (Join-Path $results 'mmcss-execution.txt')
    $mmcss=$LASTEXITCODE
    [pscustomobject]@{NormalExit=$normal;MmcssExit=$mmcss} | ConvertTo-Json | Set-Content (Join-Path $results 'exits.json')
} finally {
    # Stop only the logger successfully started by this invocation.
    & $xperf -stop 'NT Kernel Logger' *> (Join-Path $results 'stop.txt')
    if($LASTEXITCODE){Write-Error 'Kernel trace stop failed; inspect stop.txt'}
}
Get-FileHash -LiteralPath $trace -Algorithm SHA256 | Select-Object Path,Hash | Export-Csv (Join-Path $results 'trace-sha256.csv') -NoTypeInformation
} catch {$_ | Out-String | Set-Content (Join-Path $results 'fatal-error.txt');exit 1}
