param(
    [string]$IppRoot='C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0',
    [string]$BuildFolder='build\msvc-stage03',
    [string]$Python='python',
    [ValidateRange(1,8)][int]$Shards=8
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$revision='3b2120d980a7d6d4aa2d0b9a3d79ad3927b4dfd7'
$baseline=Join-Path $root 'local/stage03-before'
New-Item -ItemType Directory -Force $baseline | Out-Null
$archive=Join-Path $root 'local/stage03-before.tar'
& git -C $root archive --format=tar "--output=$archive" $revision
if($LASTEXITCODE){throw 'Stage02 baseline archive failed'}
& tar -xf $archive -C $baseline
if($LASTEXITCODE){throw 'Stage02 baseline extraction failed'}
# Validate the frozen Stage02 DSP, not the explicitly authorized Stage03 edits.
& (Join-Path $PSScriptRoot 'Stage02-References.ps1') -ImportedRoot $baseline
New-Item -ItemType Directory -Force (Join-Path $baseline 'local') | Out-Null
foreach($name in @('ref-crossover','ref-native','ref-v07','reference-repo')){
    $target=Join-Path $root "local/$name"
    $link=Join-Path $baseline "local/$name"
    if((Test-Path -LiteralPath $target) -and !(Test-Path -LiteralPath $link)){
        New-Item -ItemType Junction -Path $link -Target $target | Out-Null
    }
}
# Identical measurement code, while baseline DSP/AudioGraph stay at the pinned SHA.
Copy-Item -LiteralPath (Join-Path $root 'Tests/Stage02/Validation.cpp') -Destination (Join-Path $baseline 'Tests/Stage02/Validation.cpp')
Copy-Item -LiteralPath (Join-Path $root 'Tests/Stage03FadeBenchmark.cpp') -Destination (Join-Path $baseline 'Tests/Stage03FadeBenchmark.cpp')
$fadeCmake=@'
add_executable(EntropyStage03Fade Tests/Stage03FadeBenchmark.cpp)
target_link_libraries(EntropyStage03Fade PRIVATE entropy_graph)
entropy_native_flags(EntropyStage03Fade)
'@
Add-Content -LiteralPath (Join-Path $baseline 'CMakeLists.txt') -Value $fadeCmake
$build=Join-Path $root $BuildFolder
$results=Join-Path $build 'stage03-results'
New-Item -ItemType Directory -Force $results | Out-Null
& (Join-Path $PSScriptRoot 'Build-Windows.ps1') -IppRoot $IppRoot -BuildFolder $BuildFolder -Stage02 -CompileOnly *> (Join-Path $results 'build.txt')
& (Join-Path $baseline 'tools/Build-Windows.ps1') -IppRoot $IppRoot -BuildFolder 'build/msvc-before' -Stage02 -CompileOnly *> (Join-Path $results 'build-before.txt')
$exe=Join-Path $build 'EntropyStage02.exe'
$before=Join-Path $baseline 'build/msvc-before/EntropyStage02.exe'
$gold=Join-Path $root 'local/ref-v07/releases/crossover/V07/EntropyCrossover.dll'
$goldAvailable=(Test-Path -LiteralPath $gold) -and ((Get-FileHash -LiteralPath $gold -Algorithm SHA256).Hash -eq '1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A')
if(!$goldAvailable){Write-Warning 'V07 diagnostics BLOCKED; all other checks continue'}
$ctest='C:\Qt\Tools\CMake_64\bin\ctest.exe'
if(Get-Command ctest.exe -ErrorAction SilentlyContinue){$ctest=(Get-Command ctest.exe).Source}
$status=@()
& $ctest --test-dir $build --output-on-failure *> (Join-Path $results 'ctest.txt')
$status += [pscustomobject]@{Check='CTest';ExitCode=$LASTEXITCODE}
& (Join-Path $build 'EntropyAudioCore.exe') --probe *> (Join-Path $results 'probe.txt')
$status += [pscustomobject]@{Check='Probe';ExitCode=$LASTEXITCODE}
$jobs=@()
for($i=0;$i -lt $Shards;$i++){
    $dir=Join-Path $results "shard$i"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $arguments=@('--numerical',('"'+$dir+'"'),"--shard=$i/$Shards")
    if($goldAvailable){$arguments+=('"'+$gold+'"')}
    $jobs+=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $dir 'run.txt') -RedirectStandardError (Join-Path $dir 'error.txt')
}
for($i=0;$i -lt $jobs.Count;$i++){
    $jobs[$i].WaitForExit()
    $status += [pscustomobject]@{Check="numerical-$i";ExitCode=$jobs[$i].ExitCode}
}
foreach($mode in @('identity','controls','transitions','reactivation','realtime','cpu-baseline')){
    $dir=$results
    if($mode -eq 'cpu-baseline'){$dir=Join-Path $results 'sse42';New-Item -ItemType Directory -Force $dir | Out-Null}
    $arguments=@("--$mode",$dir)
    if($goldAvailable -and $mode -in @('transitions','reactivation')){$arguments+=$gold}
    & $exe @arguments *> (Join-Path $results "$mode.txt")
    $status += [pscustomobject]@{Check=$mode;ExitCode=$LASTEXITCODE}
}
# Benchmarks run serially after every validation worker has exited.
foreach($measurement in @(@{Name='before';Exe=$before},@{Name='after';Exe=$exe})){
    $dir=Join-Path $results $measurement.Name
    New-Item -ItemType Directory -Force $dir | Out-Null
    & $measurement.Exe --benchmark $dir *> (Join-Path $dir 'run.txt')
    $status += [pscustomobject]@{Check=('benchmark-'+$measurement.Name);ExitCode=$LASTEXITCODE}
}
foreach($measurement in @(@{Name='before';Exe=(Join-Path $baseline 'build/msvc-before/EntropyStage03Fade.exe')},@{Name='after';Exe=(Join-Path $build 'EntropyStage03Fade.exe')})){
    $dir=Join-Path $results $measurement.Name
    & $measurement.Exe $dir *> (Join-Path $dir 'fade-run.txt')
    $status += [pscustomobject]@{Check=('fade-'+$measurement.Name);ExitCode=$LASTEXITCODE}
}
$status | ConvertTo-Json | Set-Content (Join-Path $results 'exit-codes.json') -Encoding utf8
Get-FileHash -LiteralPath $exe,$before,(Join-Path $build 'EntropyAudioCore.exe') -Algorithm SHA256 | Export-Csv (Join-Path $results 'executable-hashes.csv') -NoTypeInformation
& $Python (Join-Path $PSScriptRoot 'Summarize-Stage03.py') $results *> (Join-Path $results 'aggregation.txt')
$aggregationExit=$LASTEXITCODE
if($aggregationExit){throw "Aggregation failed ($aggregationExit): incomplete or malformed measurements"}
& $Python (Join-Path $PSScriptRoot 'Render-Stage03Benchmark.py') $results (Join-Path $root 'docs/BENCHMARK-STAGE03.md')
if($LASTEXITCODE){throw 'Benchmark rendering failed'}
Write-Host 'STOP: inspect all statuses; deadline FAIL and V07 BLOCKED are retained. No push, merge or device connection.'
if($status.ExitCode | Where-Object {$_ -ne 0}){exit 1}
