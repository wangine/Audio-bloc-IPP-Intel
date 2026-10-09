param(
 [string]$BuildFolder='build/msvc-thiran',
 [string]$Stage04Reference,
 [string]$StableStage3Directory,
 [string]$Profile,
 [string]$ExistingCapture,
 [switch]$CreateManualComparison,
 [double]$ManualComparisonGainDb=-12,
 [string]$Output
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if([bool]$Profile -ne [bool]$ExistingCapture){throw 'Provide both saved profile and existing raw capture, or neither'}
if($CreateManualComparison -and !$Profile){throw 'Manual comparison requires an existing profile/capture'}
if(!$Output){$Output=Join-Path $root ('local/stage014/reproduce-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
New-Item -ItemType Directory -Path $Output -Force | Out-Null
$buildArguments=@{BuildFolder=$BuildFolder;Stage04=$true;Stage05=$true;CompileOnly=$true}
if($Stage04Reference){$buildArguments.Stage04Reference=$Stage04Reference}
if($StableStage3Directory){$buildArguments.StableStage3Directory=$StableStage3Directory}
& (Join-Path $PSScriptRoot 'Build-Windows.ps1') @buildArguments *> (Join-Path $Output 'build.txt')
$probe=Join-Path (Join-Path $root $BuildFolder) 'EntropyStage014Pcm24.exe'
$arguments=@()
if($Profile){
 $actual=(Resolve-Path -LiteralPath $Profile).Path;$raw=(Resolve-Path -LiteralPath $ExistingCapture).Path
 $arguments=@($actual,$raw,(Join-Path $Output 'numeric.json'))
}
& $probe @arguments *> (Join-Path $Output 'probes.txt');$original=$LASTEXITCODE
$comparison=$null
if($CreateManualComparison){
 & $probe --manual-copy $actual $raw (Join-Path $Output 'manual-copy-numeric.json') $ManualComparisonGainDb *> (Join-Path $Output 'manual-copy.txt');$comparison=$LASTEXITCODE
}
[ordered]@{originalExit=$original;manualComparisonExit=$comparison;hardwareOpened=$false;liveProfileChanged=$false;logs=$Output} | ConvertTo-Json | Tee-Object -FilePath (Join-Path $Output 'summary.json')
# A passing comparison never conceals the original policy's overload.
if($original -or $comparison){exit 1}
