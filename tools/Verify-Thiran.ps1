param([string]$BuildFolder='build/msvc-thiran',[string]$Stage04Reference)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    # Fixed immutable source, never a baseline generated from the candidate.
    $baseline='3e36e367741ab47f7ed861a21662866001ea8f13'
    New-Item -ItemType Directory -Force local/thiran-baseline | Out-Null
    & git archive --format=tar --output=local/thiran-baseline.tar $baseline DSP/Crossover Shared/include
    if($LASTEXITCODE){throw 'Cannot extract immutable 0.0.3 baseline'}
    & tar -xf local/thiran-baseline.tar -C local/thiran-baseline
    if($LASTEXITCODE){throw 'Cannot extract baseline tar'}
    if(!$Stage04Reference){
        $common=(& git rev-parse --path-format=absolute --git-common-dir).Trim()
        foreach($candidate in @((Join-Path $root 'local/stage04-ref/Stage3/native-reference'),
            (Join-Path (Split-Path -Parent $common) 'local/stage04-ref/Stage3/native-reference'))){
            if(Test-Path -LiteralPath (Join-Path $candidate 'dependencies/SRC/R8Brain/src/r8brain-free-src/CDSPResampler.h')){$Stage04Reference=$candidate;break}
        }
    }
    if(!$Stage04Reference){throw 'Provide -Stage04Reference pointing to the existing pinned native egress dependency cache (6cbac372...).'}
    $common=(& git rev-parse --path-format=absolute --git-common-dir).Trim()
    $stableStage3=Join-Path (Split-Path -Parent $common) '../Stage3'
    & "$PSScriptRoot/Build-Windows.ps1" -BuildFolder $BuildFolder -Stage04 -Stage05 -Stage04Reference $Stage04Reference -StableStage3Directory $stableStage3 -CompileOnly
    $ctest='C:/Qt/Tools/CMake_64/bin/ctest.exe'
    # This task validates the new delay, not the historical scheduling gates.
    & $ctest --test-dir $BuildFolder --output-on-failure -R 'Entropy(Thiran|Stage03Reset|OfflineConfig|GraphSmoke|Matrix|Headroom|Pcm24|IppFir|NativeState|AudioCoreProbe|Stage05Control|Stage05Boundary|Stage05Protocol|Stage05Transport)'
    if($LASTEXITCODE){throw 'Focused Thiran/native/control regression FAIL'}
    Write-Host 'PASS focused offline regression. Historical Stage04 FAIL REALTIME and V07 BLOCKED are unchanged. No physical output started.'
} finally {Pop-Location}
