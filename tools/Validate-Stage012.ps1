param(
 [string]$BuildFolder='build/msvc-headroom-mask',
 [string]$Profile,
 [switch]$FullSuite,
 [string]$Output
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$Output){$Output=Join-Path $root ('local/stage012/reproduce-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
New-Item -ItemType Directory -Path $Output -Force | Out-Null
& (Join-Path $PSScriptRoot 'Build-Windows.ps1') -BuildFolder $BuildFolder -Stage04 -Stage05 -CompileOnly *> (Join-Path $Output 'build.txt')
$build=Join-Path $root $BuildFolder
$ctest='C:/Qt/Tools/CMake_64/bin/ctest.exe'
$arguments=@('--test-dir',$build,'--output-on-failure')
if(!$FullSuite){$arguments+=@('-R','EntropyStage012HeadroomMask|EntropyStage05Control|EntropyStage07Live|EntropyHeadroom')}
& $ctest @arguments *> (Join-Path $Output 'ctest.txt');$tests=$LASTEXITCODE
& (Join-Path $build 'EntropyAudioCore.exe') --probe *> (Join-Path $Output 'probe.txt');$probe=$LASTEXITCODE
$reference=$null;$safe=$null
if($Profile){
 $actual=(Resolve-Path -LiteralPath $Profile).Path
 $original=Join-Path $Output 'original-limit'
 & (Join-Path $build 'EntropyStage012Reference.exe') $actual $original *> (Join-Path $Output 'original-limit.txt');$reference=$LASTEXITCODE
 # An explicit policy on private comparison copies; no live profile is edited.
 $comparison=Join-Path $Output 'comparison-minus12'
 if(Test-Path -LiteralPath (Join-Path $original 'stage2-fragment.raw')){
  New-Item -ItemType Directory -Path $comparison -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $original 'stage2-fragment.raw') -Destination $comparison
  & (Join-Path $build 'EntropyStage012Reference.exe') $actual $comparison -12 *> (Join-Path $Output 'comparison-minus12.txt');$safe=$LASTEXITCODE
 }
}
[ordered]@{ctestExit=$tests;probeExit=$probe;originalLimitComparisonExit=$reference;explicitMinus12ComparisonExit=$safe;hardwareOpened=$false;logs=$Output} | ConvertTo-Json | Tee-Object -FilePath (Join-Path $Output 'summary.json')
# Do not conceal retained suite failures or insufficient AUTO capacity.
if($tests -or $probe -or $reference -or $safe){exit 1}
