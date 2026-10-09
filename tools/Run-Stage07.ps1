param([string]$NativeBuild='build/msvc-stage07',[string]$UiBuild='build/qt-stage07',[string]$ResultsFolder='local/stage07')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root
$results=[IO.Path]::GetFullPath((Join-Path $root $ResultsFolder))
if(!$results.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Results must stay inside repository'}
New-Item -ItemType Directory -Force $results | Out-Null
$gate=Join-Path $results 'live-offline-pass.gate'
if(Test-Path -LiteralPath $gate){Remove-Item -LiteralPath $gate}
$python='C:/Users/bobi/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
$env:PYTHONUTF8='1'
& "$PSScriptRoot/Build-Windows.ps1" -BuildFolder $NativeBuild -Stage02 -Stage04 -Stage05 -CompileOnly *> "$results/build.txt"
& "$PSScriptRoot/Build-Stage05-UI.ps1" -BuildFolder $UiBuild *> "$results/build-ui.txt"
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $NativeBuild --output-on-failure *> "$results/ctest-final.txt"
if($LASTEXITCODE){throw 'Native suite FAIL; no live gate created'}
& "$NativeBuild/EntropyAudioCore.exe" --probe *> "$results/probe.txt"
if($LASTEXITCODE){throw 'Core probe FAIL'}
& "$PSScriptRoot/Test-Stage05-UI.ps1" -NativeBuild $NativeBuild -UiBuild $UiBuild *> "$results/qt-stage05.txt"
& "$PSScriptRoot/Test-Stage06-UI.ps1" -NativeBuild $NativeBuild -UiBuild $UiBuild *> "$results/qt-stage06.txt"
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
foreach($test in @('EntropyStage07Ui','EntropyStage07IpcTimeout','EntropyStage07DeviceInventory')){& "$UiBuild/$test.exe" *> "$results/$test.txt";if($LASTEXITCODE){throw "$test FAIL"}}
& $python "$PSScriptRoot/Verify-Stage07.py" *> "$results/source-integrity.txt"
if($LASTEXITCODE){throw 'Integrity FAIL'}
# Preserve every earlier execution file and historical FAIL. One fresh run, no retry-until-PASS.
$bench=Join-Path $results ('offline-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $bench | Out-Null
& "$NativeBuild/EntropyStage04Validation.exe" $bench --mmcss-benchmark *> "$bench/execution.txt"
$validationExit=$LASTEXITCODE
$numeric=@(Import-Csv "$bench/numerical.csv");$clocks=@(Import-Csv "$bench/clock-domains.csv");$benchmark=@(Import-Csv "$bench/benchmark.csv")
$dispatch=Get-Content "$bench/scheduling.json" -Raw | ConvertFrom-Json
$misses=($benchmark | Measure-Object -Property deadline_misses -Sum).Sum
$pass=$validationExit -eq 0 -and $numeric.Count -eq 96 -and $clocks.Count -eq 8 -and $benchmark.Count -eq 80 -and $misses -eq 0 -and @($numeric|Where-Object status -ne PASS).Count -eq 0 -and @($clocks|Where-Object {$_.status -ne 'PASS' -or [double]$_.max_error_ppm -gt .05 -or [int]$_.missing_after_four_seconds -ne 0 -or [int]$_.valid_after_four_seconds -le 0 -or ([double]$_.estimated_ppm*[double]$_.drift_ppm) -le 0}).Count -eq 0 -and @($benchmark|Where-Object status -ne PASS).Count -eq 0 -and $dispatch.mmcss_registered -and $dispatch.mmcss_priority_critical -and $dispatch.self_test_pass -and $dispatch.snapshot_errors -eq 0 -and $dispatch.snapshot_reads -eq 1600002
$summary=[ordered]@{format='ENTROPY_NATIVE_LIVE_OFFLINE_PASS_V1';audioCoreSHA256=(Get-FileHash "$NativeBuild/EntropyAudioCore.exe" -Algorithm SHA256).Hash;ctestPass=$true;numericRows=$numeric.Count;clockRows=$clocks.Count;benchmarkRows=$benchmark.Count;deadlineMisses=$misses;realtimeAuditPass=($validationExit -eq 0);offlinePass=$pass;results=$bench;physicalExecution=$false}
$json=$summary | ConvertTo-Json
[IO.File]::WriteAllText("$results/offline-summary.json",$json,[Text.UTF8Encoding]::new($false))
& $python "$PSScriptRoot/Summarize-Stage04.py" $bench 'validation/stage07' *> "$results/numeric-summary.txt"
if(!$pass){throw 'Stage07 offline FAIL; physical authorization remains BLOCKED; all evidence retained'}
[IO.File]::WriteAllText($gate,$json,[Text.UTF8Encoding]::new($false))
Write-Output "PASS Stage07 full native/Qt suite, numeric96, clock8, benchmark80, strict deadlines and audit. Gate: $gate. No hardware opened."
