param(
    [string]$OriginalRuntime = 'D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 2 DSP/versions/v0006-parametric-eq',
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [string]$BuiltBinary,
    [string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
if (!$BuiltBinary) { $BuiltBinary = Join-Path $RepositoryRoot 'local/stage011/stage2-build/Host/EntropyDspEngine/EntropyDspEngine.exe' }
if (!$OutputRoot) { $OutputRoot = Join-Path $RepositoryRoot 'local/stage011/stage2-v0008' }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$approvedOutputs = @('stage2-v0008','stage2-v0008-final') | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $RepositoryRoot "local/stage011/$_")) }
if ($OutputRoot -notin $approvedOutputs) { throw 'Package output must be an isolated stage2-v0008 or stage2-v0008-final scratch folder' }
if (Test-Path -LiteralPath $OutputRoot) { throw "Package output already exists; preserve it and select a fresh approved folder: $OutputRoot" }
$baseline = Get-Content -LiteralPath (Join-Path $OriginalRuntime 'package-manifest.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
# Copy the original immutable dependencies. Retired launchers and test harnesses
# are omitted so no copied shortcut can point back to the installed version.
foreach ($entry in $baseline.files) {
    if ($entry.path -match '^(EntropyDspEngine\.exe|ColdStart-DSP\.|Dsp.*Harness\.exe|README\.txt|rollback\.json|Verify-Stage2\.ps1)') { continue }
    $source = Join-Path $OriginalRuntime $entry.path
    if (!(Test-Path -LiteralPath $source)) { if ($entry.optional) { continue }; throw "Missing baseline file $source" }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw "Baseline hash differs: $source" }
    $destination = Join-Path $OutputRoot $entry.path
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}
Copy-Item -LiteralPath $BuiltBinary -Destination (Join-Path $OutputRoot 'EntropyDspEngine.exe') -Force
$state = Join-Path $OriginalRuntime 'state'
if (Test-Path -LiteralPath $state) { Copy-Item -LiteralPath $state -Destination $OutputRoot -Recurse -Force }
# The copied source keeps repository-relative CMake paths reproducible.
foreach ($relative in @('Integration/Stage2/source', 'Integration/Stage2/tools', 'Transport/StagePublishV2', 'Transport/StageBusSDK', 'Transport/StageLink')) {
    $destination = Join-Path $OutputRoot "source/$relative"
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot $relative) -Destination (Split-Path $destination) -Recurse -Force
}
# CMake's native route tests and process harness resolve ../runtime/stage2/modules
# beside Integration/Stage2/source. Include the already SHA-verified original
# dependencies there so this saved source snapshot can rebuild independently.
$snapshotRuntime = Join-Path $OutputRoot 'source/Integration/Stage2/runtime/stage2'
New-Item -ItemType Directory -Path $snapshotRuntime -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $OutputRoot 'modules') -Destination $snapshotRuntime -Recurse
$instructions = @'
Stage2 v0008-complete-independence
Compatible AudioCore version: 0.1.0 using StagePublish V2.

Based on the copied v0006-parametric-eq source snapshot. No original source commit is asserted.
Source DSP executor, Center/EQ/LRC math and native module DLLs remain unchanged.
Production uses --automatic. Stage1 input and Stage2 output are passive StagePublish V2 publications.
Stage3 connects passively to StagePublish V2; V1 Attach/Release endpoints are absent.
Legacy --source-session demand discovery is explicitly refused.

Verify:
  EntropyDspEngine.exe --verify-only
Normal GUI launch:
  Double-click ColdStart-DSP.cmd. It verifies the package before opening the original GUI controls.
  The launcher never stops/retries Stage1 and never arms physical output.
Launch after stopping the previous Stage2 owner (do not stop/retry Stage1):
  EntropyDspEngine.exe --headless --automatic --runtime-root <this folder> --diagnostic-file <this folder>/diagnostics.json --stop-file <this folder>/stop.request
Delete any previous stop.request before launching. The launcher never arms physical output.
Diagnostics are read-only worker snapshots written atomically by the main thread every 200ms.
Source UUID is source_session_id; blocks_in/blocks_out/frames_out must keep advancing.
Stop/restart/kill Stage2 or Stage3 independently. Stage2 owns no upstream lease and sends no demand/fault/reset.
Rollback: close this Stage2 and launch the preserved original v0006. Stage2 v0008 restart subscribes to the continuing Stage1 v011 source UUID without Retry. Original v0006 uses the incompatible V1 lease contract.

Rebuild source/Integration/Stage2/source with CMake/Ninja, Qt 6.11.2 and MinGW 13.1.
Use -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 and the bundled C:/Qt/Tools/mingw1310_64 compilers.
Tests: DspPassiveInput, DspPassiveNative, DspPassiveAllocation, DspBypassIdentity, DspCenterNative, DspEqNative, DspAlignment. Retired V1 integration tests remain visible and fail with explicit incompatibility.
Windows sandbox tokens may deny named-pipe clients; run isolated tests with normal same-user pipe access.
Transport service performs control allocation/locking/liveness checks outside DSP step.
The producer publish path copies committed planes into bounded queues without allocation or locks.
'@
Set-Content -LiteralPath (Join-Path $OutputRoot 'README-v0008.txt') -Value $instructions -Encoding utf8
$launcher = @'
@echo off
setlocal
start "" /wait "%~dp0EntropyDspEngine.exe" --verify-only
if errorlevel 1 (
  echo Stage2 package verification failed. See README-v0008.txt.
  pause
  exit /b 1
)
start "" "%~dp0EntropyDspEngine.exe" --runtime-root "%~dp0."
exit /b 0
'@
Set-Content -LiteralPath (Join-Path $OutputRoot 'ColdStart-DSP.cmd') -Value $launcher -Encoding ascii
foreach ($module in $baseline.native_modules) {
    $module.optional = $module.name -notin @('EntropyCenterExtractorV2FP64.dll','EntropyLrcAlignerFP64.dll','EntropyParametricEQ.dll')
    $module.loaded = !$module.optional
}
$manifest = [ordered]@{
    schema = 2; stage = 'DSP_STAGE2'; version = 'v0008-complete-independence'; dsp_mode = 'CENTER_V2_LRC_STATIC'
    lrc_startup_mode = 'BYPASS_EXACT'; lrc_alignment_available = $true; lrc_alignment_active = $false
    parametric_eq_available = $true; built_utc = [DateTime]::UtcNow.ToString('o'); hardware_auto_arm = $false
    baseline_commit_locally_verified = $false; baseline_executable_sha256 = (Get-FileHash -LiteralPath (Join-Path $OriginalRuntime 'EntropyDspEngine.exe')).Hash.ToLowerInvariant()
    stagebus_contract_sha256 = $baseline.stagebus_contract_sha256; stagepublish_protocol = 'StagePublishV2'
    compatible_audio_core_version = '0.1.0'
    source_commit = (& git -C $RepositoryRoot rev-parse HEAD).Trim()
    source_input = 'PASSIVE_STAGE1_V2'; stage2_output = 'PASSIVE_STAGE2_V2'; v1_external_takeover = 'Refused; subscribe to StagePublish V2'
    native_modules = $baseline.native_modules
}
$manifest.files = @(Get-ChildItem -LiteralPath $OutputRoot -File -Recurse | Where-Object {
    $relative = [IO.Path]::GetRelativePath($OutputRoot, $_.FullName).Replace('\','/')
    $relative -ne 'package-manifest.json' -and $relative -notmatch '^(state/|diagnostics\.json$|stop\.request$)'
} | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($OutputRoot, $_.FullName).Replace('\','/')
    [ordered]@{path = $relative; sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant(); optional = $relative -in @('modules/EntropyCenterExtractorFP64.dll', 'modules/EntropyLoudness.dll')}
})
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputRoot 'package-manifest.json') -Encoding utf8
Write-Output "Prepared $OutputRoot"
Write-Output "Executable SHA256 $((Get-FileHash -LiteralPath (Join-Path $OutputRoot 'EntropyDspEngine.exe')).Hash)"
