param(
    [string]$OriginalRelease='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 1 Sources/releases/stage1-independence-fix-candidate-20261005',
    [string]$RepositoryRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [string]$OutputRoot,
    [string]$BuiltBinary
)
$ErrorActionPreference='Stop'
if(!$OutputRoot){$OutputRoot=Join-Path $RepositoryRoot 'local/stage011/stage1-passive-release'}
if(!$BuiltBinary){$BuiltBinary=Join-Path $RepositoryRoot 'local/stage011/stage1-build/Host/EntropySourceEngine/EntropySourceEngine.exe'}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
if(Test-Path -LiteralPath $OutputRoot){throw "Preserve existing version folder: $OutputRoot"}
$originalRuntime=Join-Path $OriginalRelease 'Stage1/runtime/stage1'
$baseline=Get-Content -LiteralPath (Join-Path $originalRuntime 'package-manifest.json') -Raw | ConvertFrom-Json
$runtime=Join-Path $OutputRoot 'Stage1/runtime/stage1'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach($entry in $baseline.files){
    if($entry.path -match '^(EntropySourceEngine\.exe|ColdStart-Source\.|Verify-Stage1\.ps1|README)'){continue}
    $source=Join-Path $originalRuntime $entry.path
    if(!(Test-Path -LiteralPath $source)){throw "Missing original dependency: $source"}
    if((Get-FileHash -LiteralPath $source).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Original dependency changed: $source"}
    $destination=Join-Path $runtime $entry.path
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}
Copy-Item -LiteralPath $BuiltBinary -Destination (Join-Path $runtime 'EntropySourceEngine.exe')
# Private state stays local and is excluded from the immutable source/dependency manifest.
$state=Join-Path $originalRuntime 'state'
if(Test-Path -LiteralPath $state){Copy-Item -LiteralPath $state -Destination $runtime -Recurse}
foreach($relative in @('source','tests')){
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot "Integration/Stage1/$relative") -Destination (Join-Path $OutputRoot 'Stage1') -Recurse
}
# Original replay artifacts stay local; the source snapshot contains no build tree.
$nativeDestination=Join-Path $OutputRoot 'Stage1/native-src'
New-Item -ItemType Directory -Path $nativeDestination -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'Integration/Stage1/native-src') | Where-Object {$_.Name -ne 'build'} | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $nativeDestination -Recurse
}
New-Item -ItemType Directory -Path (Join-Path $OutputRoot 'Transport') -Force | Out-Null
foreach($relative in @('StagePublishV2','StageBusSDK','StageLink')){
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot "Transport/$relative") -Destination (Join-Path $OutputRoot 'Transport') -Recurse
}
@'
@echo off
setlocal
start "" /wait "%~dp0EntropySourceEngine.exe" --verify-only
if errorlevel 1 (
  echo Stage1 package verification failed.
  pause
  exit /b 1
)
start "" "%~dp0EntropySourceEngine.exe" --runtime-root "%~dp0."
exit /b 0
'@ | Set-Content -LiteralPath (Join-Path $runtime 'ColdStart-Source.cmd') -Encoding ascii
@'
@echo off
call "%~dp0Stage1\runtime\stage1\ColdStart-Source.cmd"
'@ | Set-Content -LiteralPath (Join-Path $OutputRoot 'ColdStart-Source.cmd') -Encoding ascii
$instructions=@'
Stage1 passive publication — isolated release

Launch ColdStart-Source.cmd. Same folder structure: Stage1/runtime/stage1.
Stage1 continuously processes its existing capture/SRC at its own cadence.
Stage2 v0008 subscribes to StagePublish.Source.V2. No V1 demand/lease fallback.
Closing Stage2/Stage3 only closes their subscription. It cannot fault or reset Stage1.
Capture, SRC/native module DLLs and source DSP mathematics are unchanged.
Full finite queues report missed deliveries for that subscriber; source continues.
No DAC is opened by this launcher. Preserve the original release for rollback.

Rebuild:
  cmake -S Stage1/source -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe -DSTAGE_PUBLISH_ROOT=<this-release>/Transport/StagePublishV2
  cmake --build build --parallel 4
  ctest --test-dir build --output-on-failure
Legacy V1 ownership tests are expected to fail explicitly; consult validation report.
'@
$instructions | Set-Content -LiteralPath (Join-Path $OutputRoot 'README.txt') -Encoding utf8
$manifest=[ordered]@{
    schema=2; stage='SOURCE_STAGE1_LINK'; version='stage1-passive-publish-v0002'; built_utc=[DateTime]::UtcNow.ToString('o')
    hardware_auto_arm=$false; stagebus_contract_sha256=$baseline.stagebus_contract_sha256
    source_commit=(& git -C $RepositoryRoot rev-parse HEAD).Trim()
    stagepublish_protocol='StagePublishV2'; publication_layout='MIX_CINEMA10_V1'; continuous_processing=$true
    subscriber_controls_source=$false; compatible_stage2='v0008-passive-source'; native_modules=$baseline.native_modules
}
$manifest.files=@(Get-ChildItem -LiteralPath $runtime -File -Recurse | Where-Object {
    $relative=[IO.Path]::GetRelativePath($runtime,$_.FullName).Replace('\','/')
    $relative -ne 'package-manifest.json' -and $relative -notmatch '^(state/|status\.json$|stop\.request$)'
} | ForEach-Object {
    [ordered]@{path=[IO.Path]::GetRelativePath($runtime,$_.FullName).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant(); optional=$false}
})
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $runtime 'package-manifest.json') -Encoding utf8
Write-Output "Prepared $OutputRoot"
Write-Output "Stage1 SHA256 $((Get-FileHash -LiteralPath (Join-Path $runtime 'EntropySourceEngine.exe')).Hash)"
