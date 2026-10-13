param([string]$OutputRoot,[string]$Version='stage2-music-v0010')
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(!$OutputRoot){$OutputRoot=Join-Path $taskRoot ('release/'+$Version)}
$taskOutput=[IO.Path]::GetFullPath($OutputRoot)
$taskRelease=[IO.Path]::GetFullPath((Join-Path $taskRoot 'release'))
if(!$taskOutput.StartsWith($taskRelease+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must stay inside Stage2/release'}
if(Test-Path -LiteralPath $taskOutput){throw 'Package already exists; choose a fresh version folder'}
$taskBinary=Join-Path $taskRoot 'build/Host/EntropyDspEngine/EntropyDspEngine.exe'
if(!(Test-Path -LiteralPath $taskBinary)){throw 'Build Stage2 MUSIC first'}
$taskBaseline=Get-Content -LiteralPath (Join-Path $taskRoot 'baseline-v0008-manifest.json') -Raw | ConvertFrom-Json
$taskDependencies=Join-Path $taskRoot 'runtime/stage2'
$taskRecovery=Join-Path $taskDependencies 'modules/EntropyDynamicRecovery.dll'
$taskRecoverySha='84fbb4f7e2519eaaa13c68159fb596d44ffda2f0de9b8bde2b2d72de7573973d'
if(!(Test-Path -LiteralPath $taskRecovery)){throw 'Build and stage EntropyDynamicRecovery.dll before packaging the candidate'}
if((Get-FileHash -LiteralPath $taskRecovery).Hash.ToLowerInvariant() -ne $taskRecoverySha){throw 'Dynamic Recovery DLL differs from the SHA-pinned candidate'}
foreach($taskModule in $taskBaseline.native_modules){
 $taskModuleFile=Join-Path $taskDependencies ('modules/'+$taskModule.name)
 if((Get-FileHash -LiteralPath $taskModuleFile).Hash.ToLowerInvariant() -ne $taskModule.sha256){throw "Original DLL differs: $($taskModule.name)"}
}
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
foreach($taskEntry in Get-ChildItem -LiteralPath $taskDependencies){
 if($taskEntry.Name -in @('state','sdk','EntropyDspEngine.exe','package-manifest.json')){continue}
 Copy-Item -LiteralPath $taskEntry.FullName -Destination $taskOutput -Recurse
}
Copy-Item -LiteralPath $taskBinary -Destination (Join-Path $taskOutput 'EntropyDspEngine.exe')
Copy-Item -LiteralPath (Join-Path $taskRoot 'source/Shared/StageBusSDK') -Destination (Join-Path $taskOutput 'sdk') -Recurse
Copy-Item -LiteralPath (Join-Path $taskRoot 'contract/MusicStageBusContract.json') -Destination (Join-Path $taskOutput 'sdk/MusicStageBusContract.json')
Copy-Item -LiteralPath (Join-Path $taskRoot 'README.md') -Destination $taskOutput
New-Item -ItemType Directory -Path (Join-Path $taskOutput 'state') -Force | Out-Null
$taskLauncher=@'
@echo off
setlocal
"%~dp0EntropyDspEngine.exe" --verify-only
if errorlevel 1 (
 echo Stage2 MUSIC package verification failed.
 pause
 exit /b 1
)
start "" "%~dp0EntropyDspEngine.exe" --runtime-root "%~dp0."
'@
[IO.File]::WriteAllText((Join-Path $taskOutput 'ColdStart-DSP-Music.cmd'),$taskLauncher,[Text.Encoding]::ASCII)
$taskManifest=[ordered]@{
 schema=3;stage='DSP_STAGE2';version=$Version;dsp_mode='CENTER_V2_LRC_STATIC'
 input_channels=2;output_channels=3;input_layout='MUSIC_STEREO2_V1';output_layout='MUSIC_LRC3_V1'
 sample_rate=192000;frames_per_block=1024;stagepublish_protocol='StagePublishV2';hardware_auto_arm=$false
 parametric_eq_available=$true;lrc_alignment_available=$true;lrc_alignment_active=$false
 dynamic_recovery_available=$true;dynamic_recovery_default_enabled=$false
 stagebus_contract_sha256=$taskBaseline.stagebus_contract_sha256
 music_stagebus_contract_sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot 'contract/MusicStageBusContract.json')).Hash.ToLowerInvariant()
 baseline_executable_sha256=(($taskBaseline.files | Where-Object path -eq 'EntropyDspEngine.exe').sha256)
 source_archive_sha256='fac90e5483f7c062b805afcdc4b786397adf5ce4808e025e4fc0bb266bf52d7e'
 source_release='Local Stage2 MUSIC v0010 source archive';native_modules=@($taskBaseline.native_modules)+@([ordered]@{name='EntropyDynamicRecovery.dll';sha256=$taskRecoverySha;identifier='audio.entropy.dynamic-recovery.fp64.v1'})
 built_utc=[DateTime]::UtcNow.ToString('o');legacy_stage1_10ch_compatible=$false;legacy_stage3_11ch_compatible=$false
}
$taskManifest.files=@(Get-ChildItem -LiteralPath $taskOutput -File -Recurse | Where-Object {
 [IO.Path]::GetRelativePath($taskOutput,$_.FullName).Replace('\','/') -notmatch '^(state/|diagnostics\.json$|stop\.request$)'
} | ForEach-Object {
 $taskRelative=[IO.Path]::GetRelativePath($taskOutput,$_.FullName).Replace('\','/')
 [ordered]@{path=$taskRelative;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant();optional=$taskRelative -in @('modules/EntropyCenterExtractorFP64.dll','modules/EntropyLoudness.dll','modules/EntropyDynamicRecovery.dll')}
})
[IO.File]::WriteAllText((Join-Path $taskOutput 'package-manifest.json'),($taskManifest | ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false))
& (Join-Path $taskOutput 'EntropyDspEngine.exe') --verify-only
if($LASTEXITCODE){throw 'Packaged MUSIC binary rejected manifest/dependencies'}
Write-Output "Verified Stage2 MUSIC package: $taskOutput"
Write-Output "SHA256: $((Get-FileHash -LiteralPath (Join-Path $taskOutput 'EntropyDspEngine.exe')).Hash)"
