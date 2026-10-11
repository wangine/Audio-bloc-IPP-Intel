param([ValidatePattern('^[a-z0-9-]+$')][string]$Version='stage1-native-music-v0003')
$ErrorActionPreference='Stop'
$stage1CandidateRoot=Split-Path $PSScriptRoot -Parent
$stage1Runtime=Join-Path $stage1CandidateRoot ('release/'+$Version)
if(Test-Path -LiteralPath $stage1Runtime){throw 'Versioned candidate is preserved; choose a fresh version'}
foreach($stage1Folder in @('core','ui','state','docs','evidence','licenses')){New-Item -ItemType Directory -Path (Join-Path $stage1Runtime $stage1Folder) -Force | Out-Null}
Copy-Item -LiteralPath (Join-Path $stage1CandidateRoot 'build/EntropyMusicSourceCore.exe') -Destination (Join-Path $stage1Runtime 'core')
Copy-Item -LiteralPath (Join-Path $stage1CandidateRoot 'build/providers') -Destination (Join-Path $stage1Runtime 'core') -Recurse
$stage1DriverRoot=Join-Path $stage1CandidateRoot '../Audio-bloc-IPP-Intel/Drivers/EntropyMusicRenderV2'
if(!(Test-Path -LiteralPath $stage1DriverRoot)){$stage1DriverRoot=Join-Path $stage1CandidateRoot '../../../Drivers/EntropyMusicRenderV2'}
Copy-Item -LiteralPath (Join-Path $stage1DriverRoot 'build/model/EntropyMusicRawProviderV2.dll') -Destination (Join-Path $stage1Runtime 'core/providers')
foreach($stage1Library in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')){Copy-Item -LiteralPath (Join-Path $env:SystemRoot ('System32/'+$stage1Library)) -Destination (Join-Path $stage1Runtime 'core')}
$stage1Reference=Join-Path $stage1CandidateRoot '../Stage1/release/stage1-native-music-v0002'
if(!(Test-Path -LiteralPath $stage1Reference)){throw 'Preparation requires the preserved Stage1 v0002 runtime next to this source checkout; use the ready v0003 runtime archive for standalone operation'}
# Copy the established dynamic Qt runtime, then replace only the rebuilt client.
Get-ChildItem -LiteralPath (Join-Path $stage1Reference 'ui') | Copy-Item -Destination (Join-Path $stage1Runtime 'ui') -Recurse
Copy-Item -LiteralPath (Join-Path $stage1CandidateRoot 'build-qt/EntropyMusicSourceQt.exe') -Destination (Join-Path $stage1Runtime 'ui/EntropyMusicSourceQt.exe')
Get-ChildItem -LiteralPath (Join-Path $stage1Reference 'licenses') | Copy-Item -Destination (Join-Path $stage1Runtime 'licenses') -Recurse
foreach($stage1Launcher in @('Stage1-Launch.ps1','Start-Core.ps1','Start-UI.ps1','Stop-Core.ps1','Start-Stage1-Music.cmd','ColdStart-Stage1-Music.cmd','Stop-Stage1-Music.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $stage1Launcher) -Destination $stage1Runtime}
Copy-Item -LiteralPath (Join-Path $stage1Reference 'state/Stage1Music.ini') -Destination (Join-Path $stage1Runtime 'state/Stage1Music.ini')
Write-Output "Prepared $stage1Runtime; exact existing selections copied; not started."
