param(
 [string]$BuildFolder='build/msvc-thiran',
 [string]$Profile,
 [string]$Python='C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe',
 [string]$Output
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$Output){$Output=Join-Path $root ('local/stage013/reproduce-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
New-Item -ItemType Directory -Path $Output -Force | Out-Null
& (Join-Path $PSScriptRoot 'Build-Windows.ps1') -BuildFolder $BuildFolder -Stage04 -Stage05 -CompileOnly *> (Join-Path $Output 'build.txt')
$build=Join-Path $root $BuildFolder
$arguments=@()
if($Profile){
 $actual=(Resolve-Path -LiteralPath $Profile).Path
 $reference=Join-Path $Output 'reference-mono.entropy'
 & $Python (Join-Path $PSScriptRoot 'Create-CrossoverMonoReference.py') $actual $reference *> (Join-Path $Output 'profile-copy.txt')
 if($LASTEXITCODE){throw 'Comparison profile creation failed; original remains unchanged'}
 $arguments=@($actual,$reference,(Join-Path $Output 'numeric.json'))
}
& (Join-Path $build 'EntropyStage013Crossover.exe') @arguments *> (Join-Path $Output 'probes.txt');$probes=$LASTEXITCODE
& (Join-Path $build 'EntropyAudioCore.exe') --probe *> (Join-Path $Output 'core-probe.txt');$core=$LASTEXITCODE
[ordered]@{probesExit=$probes;coreProbeExit=$core;hardwareOpened=$false;logs=$Output} | ConvertTo-Json | Tee-Object -FilePath (Join-Path $Output 'summary.json')
if($probes -or $core){exit 1}
