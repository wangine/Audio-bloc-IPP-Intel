param([Parameter(Mandatory=$true)][ValidateSet('A','B')][string]$Dac,[ValidateSet(1024,2048,4096)][int]$Buffer=1024)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(Test-Path -LiteralPath (Join-Path $root 'INCOMPLETE.txt')){throw 'Package incomplete; no playback.'}
if(Test-Path -LiteralPath (Join-Path $root 'prepared/INCOMPLETE.txt')){throw 'PCM preparation incomplete.'}
$manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw|ConvertFrom-Json
if($manifest.schema -ne 'entropy-b1-package-v1'){throw 'Invalid B1 manifest.'}
$selected=@($manifest.outputs|Where-Object {$_.dac -eq $Dac})
if($selected.Count -ne 1){throw 'The saved profile has no unique matching DAC.'}
$entry=$selected[0]
if($entry.file -notin @('dac-a.pcm','dac-b.pcm')){throw 'Invalid PCM filename.'}
$pcm=Join-Path $root ('prepared/'+$entry.file)
$player=Join-Path $root 'runtime/EntropyPlayB1.exe'
if((Get-FileHash -LiteralPath $pcm).Hash -ne $entry.sha256){throw 'Prepared PCM SHA256 mismatch.'}
if((Get-FileHash -LiteralPath $player).Hash -ne $manifest.playerSha256){throw 'Player SHA256 mismatch.'}
if((Get-FileHash -LiteralPath $PSCommandPath).Hash -ne $manifest.playScriptSha256){throw 'Playback script SHA256 mismatch.'}
Write-Host "B1, DAC $Dac only. No ASRC, clock controller, live FIFO or playback DSP."
Write-Host "Saved Headroom mode $($manifest.headroomMode), manual gain $($manifest.manualGainDb) dB. Original saved mapping is retained."
Write-Host 'The executable requires typing ARM. Close the current Stage3 Core with its own stop command first. No process is stopped here.'
& $player --pcm $pcm --endpoint $entry.endpoint --buffer $Buffer --authorized-manual-play
exit $LASTEXITCODE
