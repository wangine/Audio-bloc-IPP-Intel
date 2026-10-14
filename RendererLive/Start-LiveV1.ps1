$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(Test-Path -LiteralPath (Join-Path $root 'INCOMPLETE.txt')){throw 'Live renderer package incomplete; no process started.'}
$manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw|ConvertFrom-Json
if($manifest.schema -ne 'entropy-live-v1-package-v1'){throw 'Wrong live package manifest.'}
$core=Join-Path $root 'runtime/EntropyAudioCore.exe';$ui=Join-Path $root 'runtime/EntropyUI.exe'
if((Get-FileHash -LiteralPath $core).Hash -ne $manifest.coreSha256 -or (Get-FileHash -LiteralPath $ui).Hash -ne $manifest.uiSha256){throw 'Core/UI hash mismatch; no process started.'}
foreach($name in @('Start-MusicCinema.ps1','Stop-MusicCinema.ps1','Start-LiveV1.ps1')){
 if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $name)).Hash -ne $manifest.scriptSha256.$name){throw "Launcher hash mismatch: $name"}
}
$profile=Join-Path $root 'profile/listening.entropy'
if(!(Test-Path -LiteralPath $profile -PathType Leaf)){throw 'Saved package profile missing.'}
Write-Host ($manifest.version+': visible Qt UI, live Stage2 input, existing crossover/matrix/gain settings.')
Write-Host 'Start is DISARMED. MUSIC/CINEMA selection does not require a publishing source.'
Write-Host 'A program change stops the old output, selects the saved scene, and waits for real input.'
Write-Host 'No PCM capture is enabled. Precise inter-DAC phase alignment is not certified.'
$audit=$env:ENTROPY_CHAIN_AUDIT_DIR
try {
 Remove-Item Env:ENTROPY_CHAIN_AUDIT_DIR -ErrorAction SilentlyContinue
 & (Join-Path $PSScriptRoot 'Start-MusicCinema.ps1') -Profile $profile -AuthorizedLiveTest
 if(!$?){throw 'Live launcher failed; inspect the package logs.'}
} finally {
 if($null -eq $audit){Remove-Item Env:ENTROPY_CHAIN_AUDIT_DIR -ErrorAction SilentlyContinue}else{$env:ENTROPY_CHAIN_AUDIT_DIR=$audit}
}
