param([string]$BuildFolder='build/msvc-stage04', [string]$ResultsFolder='local/stage04-results')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    New-Item -ItemType Directory -Force $ResultsFolder | Out-Null
    $gate=Join-Path $ResultsFolder 'offline-pass.gate'
    # Invalidate only the exact gate file inside this workspace before rerunning.
    $gateAbsolute=[IO.Path]::GetFullPath((Join-Path $root $gate))
    if (!$gateAbsolute.StartsWith($root+[IO.Path]::DirectorySeparatorChar)) { throw 'Results must stay inside repository' }
    if(Test-Path -LiteralPath $gateAbsolute){Remove-Item -LiteralPath $gateAbsolute}
    & "$PSScriptRoot/Stage04-References.ps1" *> "$ResultsFolder/source-check.txt"
    & "$PSScriptRoot/Build-Windows.ps1" -BuildFolder $BuildFolder -Stage02 -Stage04 -CompileOnly *> "$ResultsFolder/build.txt"
    & 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $BuildFolder --output-on-failure *> "$ResultsFolder/ctest.txt"
    $ctestExit=$LASTEXITCODE
    & "$BuildFolder/EntropyAudioCore.exe" --probe *> "$ResultsFolder/probe.txt"
    $probeExit=$LASTEXITCODE
    & "$BuildFolder/EntropyStage04Validation.exe" $ResultsFolder --benchmark *> "$ResultsFolder/execution.txt"
    $validationExit=$LASTEXITCODE
    $numerical=@(Import-Csv "$ResultsFolder/numerical.csv")
    $benchmark=@(Import-Csv "$ResultsFolder/benchmark.csv")
    $clocks=@(Import-Csv "$ResultsFolder/clock-domains.csv")
    $hash=(Get-FileHash "$BuildFolder/EntropyAudioCore.exe" -Algorithm SHA256).Hash
    Get-ChildItem $BuildFolder -Filter '*.exe' | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | Export-Csv "$ResultsFolder/executable-sha256.csv" -NoTypeInformation
    $pass=$ctestExit -eq 0 -and $probeExit -eq 0 -and $validationExit -eq 0 -and $numerical.Count -eq 96 -and $benchmark.Count -eq 80 -and $clocks.Count -eq 8 -and @($clocks | Where-Object status -ne PASS).Count -eq 0 -and @($numerical | Where-Object status -ne PASS).Count -eq 0 -and @($benchmark | Where-Object status -ne PASS).Count -eq 0
    [pscustomobject]@{CTestExit=$ctestExit;ProbeExit=$probeExit;ValidationExit=$validationExit;NumericalRows=$numerical.Count;BenchmarkRows=$benchmark.Count;OfflinePass=$pass;AudioCoreSHA256=$hash} | ConvertTo-Json | Set-Content "$ResultsFolder/summary.json"
    if($pass){[IO.File]::WriteAllText($gateAbsolute,"ENTROPY_STAGE04_OFFLINE_PASS_V1`n$hash`nFIRST_DAC_INTERNAL_SINE_192000_2CH_IIR_HEADROOM20DB`n",[Text.UTF8Encoding]::new($false))}
    Write-Output "Stage04 offline PASS=$pass; numeric=$($numerical.Count); benchmark=$($benchmark.Count); gate created=$pass. No hardware was invoked."
    if(!$pass){exit 1}
} finally { Pop-Location }
