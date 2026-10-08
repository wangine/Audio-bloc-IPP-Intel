param(
    [string]$IppRoot='C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0',
    [string]$BuildFolder='build\msvc-release'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& (Join-Path $PSScriptRoot 'Stage02-References.ps1')
$build=Join-Path $root $BuildFolder
$results=Join-Path $build 'stage02-results'
New-Item -ItemType Directory -Force -Path $results | Out-Null
& (Join-Path $PSScriptRoot 'Build-Windows.ps1') -IppRoot $IppRoot -BuildFolder $BuildFolder -Stage02 -CompileOnly *> (Join-Path $results 'build.log')
$exe=Join-Path $build 'EntropyStage02.exe'
$gold=Join-Path $root 'local/ref-v07/releases/crossover/V07/EntropyCrossover.dll'
$goldAvailable=(Test-Path -LiteralPath $gold) -and ((Get-FileHash -LiteralPath $gold -Algorithm SHA256).Hash -eq '1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A')
if (!$goldAvailable) { Write-Warning 'V07 comparison BLOCKED; continuing native, controls, realtime and benchmark checks' }
$ctest=(Get-Command ctest.exe -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (!$ctest) { $ctest='C:\Qt\Tools\CMake_64\bin\ctest.exe' }
$status=@()
& $ctest --test-dir $build --output-on-failure *> (Join-Path $results 'ctest.log')
$status += [pscustomobject]@{Check='CTest';ExitCode=$LASTEXITCODE}
& (Join-Path $build 'EntropyAudioCore.exe') --probe *> (Join-Path $results 'probe.log')
$status += [pscustomobject]@{Check='Probe';ExitCode=$LASTEXITCODE}
foreach ($mode in @('numerical','transitions','controls','realtime','benchmark','cpu-baseline')) {
    $destination=$results
    if ($mode -eq 'cpu-baseline') { $destination=Join-Path $results 'sse42';New-Item -ItemType Directory -Force -Path $destination | Out-Null }
    $arguments=@("--$mode",$destination)
    if ($goldAvailable -and $mode -in @('numerical','transitions')) { $arguments += $gold }
    & $exe @arguments *> (Join-Path $results ($mode+'.log'))
    $status += [pscustomobject]@{Check=$mode;ExitCode=$LASTEXITCODE}
    Write-Host "$mode exit=$LASTEXITCODE"
}
$status | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $results 'exit-codes.json') -Encoding utf8
Get-FileHash -LiteralPath $exe,(Join-Path $build 'EntropyAudioCore.exe') -Algorithm SHA256 | Export-Csv -LiteralPath (Join-Path $results 'executable-hashes.csv') -NoTypeInformation
& (Join-Path $PSScriptRoot 'Stage02-References.ps1') -VerifyOnly
Write-Host 'STOP: offline runs finished; inspect per-check exits and result files. No push, DAC, Stage2 or Qt connection.'
if ($status.ExitCode | Where-Object { $_ -ne 0 }) { exit 1 }
