param([string]$Version='0.1.9-v2-r2')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$destination=Join-Path $root ('release/'+$Version)
if(Test-Path -LiteralPath $destination){throw 'Preserve existing candidates; destination must be fresh'}
$baseline=Get-Content -LiteralPath (Join-Path $root 'validation/music-cinema-v2/protected-baseline.json') -Raw|ConvertFrom-Json
$changed=@($baseline|Where-Object {(Get-FileHash -LiteralPath (Join-Path $root $_.path)).Hash -ne $_.sha256})
$allowed=@('Engine\Egress\LiveSinkWorkers.cpp','Engine\Egress\LiveSinkWorkers.h')
foreach($file in $changed){if($file.path -notin $allowed){throw "Unexpected protected-source change: $($file.path)"}}
$results=@()
foreach($variant in @('32k','64k')){
 $log=Join-Path $root ('validation/music-cinema-v2/source-wait-final-full-'+$variant+'.txt')
 $content=Get-Content -LiteralPath $log -Raw
 $match=[regex]::Match($content,'(\d+)% tests passed, (\d+) tests failed out of (\d+)')
 if(!$match.Success -or $match.Groups[2].Value -ne '1' -or !$content.Contains('EntropyStage04Offline (Failed)')){throw "Unexpected native result: $log"}
 $failures=@([regex]::Matches($content,'(?m)^\s*\d+ - (\S+) \(Failed\)')|ForEach-Object {$_.Groups[1].Value})
 if($failures.Count -ne 1 -or $failures[0] -ne 'EntropyStage04Offline'){throw "Additional native failure: $log"}
 $results+=[ordered]@{variant=$variant;passed=([int]$match.Groups[3].Value-1);total=[int]$match.Groups[3].Value;inheritedFailure=$failures;reportSha256=(Get-FileHash -LiteralPath $log).Hash}
}
$uiReport=Join-Path $root 'validation/music-cinema-v2/source-wait-final-EntropyMusicCinemaUi.txt'
if(!(Get-Content -LiteralPath $uiReport -Raw).StartsWith('PASS')){throw 'Qt source-wait presentation test required'}
New-Item -ItemType Directory -Path $destination|Out-Null
$qtReference=Join-Path $root 'release/0.1.9-v2/EntropyAudioCore-0.1.9-v2-64k-win-x64/runtime'
foreach($variant in @(@{name='32k';frames=32768},@{name='64k';frames=65536})){
 $bundle=Join-Path $destination ("EntropyAudioCore-$Version-$($variant.name)-win-x64")
 $runtime=Join-Path $bundle 'runtime';$launchTools=Join-Path $bundle 'tools'
 New-Item -ItemType Directory -Path $runtime,$launchTools|Out-Null
 foreach($item in Get-ChildItem -LiteralPath $qtReference){if($item.Name -notin @('EntropyAudioCore.exe','EntropyUI.exe')){Copy-Item -LiteralPath $item.FullName -Destination $runtime -Recurse}}
 Copy-Item -LiteralPath (Join-Path $root "build/msvc-music-cinema-$($variant.frames)/EntropyAudioCore.exe") -Destination $runtime
 Copy-Item -LiteralPath (Join-Path $root 'build/qt-music-cinema/EntropyUI.exe') -Destination $runtime
 Copy-Item -LiteralPath (Join-Path $root 'tools/Start-MusicCinema.ps1'),(Join-Path $root 'tools/Stop-MusicCinema.ps1') -Destination $launchTools
 Copy-Item -LiteralPath (Join-Path $root 'release/0.1.9-v2/EntropyAudioCore-0.1.9-v2-64k-win-x64/notices') -Destination $bundle -Recurse
 Set-Content -LiteralPath (Join-Path $bundle 'Cold-Start.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-MusicCinema.ps1" %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Encoding ascii -Value @('@echo off','call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Stop-AudioCore.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-MusicCinema.ps1" %*','exit /b %ERRORLEVEL%')
 $manifest=[ordered]@{version=$Version;variant=$variant.name;fifoCapacityFrames=$variant.frames;sourceWorkingTreeBaseCommit=(& git -C $root rev-parse HEAD);sourceModifiedLocal=$true;initialAutomaticArm=$false;resumeWithinRetainedUserArm=$true;sourceWaitTimeout=$null;sourceWaitPhysicalDacs='Stopped/closed; no fabricated Graph or source PCM';liveValidation='PENDING';nativeResults=$results;protectedFilesChecked=$baseline.Count;protectedFilesUnchanged=($baseline.Count-$changed.Count);intentionalLifecycleAdapterChanges=@($changed.path);originalDspAlgorithmsChanged=$false;v07='BLOCKED';stage04='Inherited offline failure retained; not qualified';coreSha256=(Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyAudioCore.exe')).Hash;uiSha256=(Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyUI.exe')).Hash}
 $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 @(('Local Stage3 source-wait candidate '+$Version),'','Explicit ARM is required. Source interruption retains user ARM indefinitely as MUTE / WAITING INPUT. DAC owners stop and close while waiting; Graph receives no fabricated PCM. Valid fresh input resumes the same Core with native preroll. Explicit DISARM cancels resumption. Serious DAC/module faults invalidate queued ARM and require a new explicit ARM.','','MUSIC: Stage2 3ch L/R/C FP64/192k. CINEMA: independent 6/8ch contract; real decoder remains pending. Original DSP, gain and clock-controller algorithms are unchanged.','','Full native results and inherited Stage04 failure are in manifest.json. Live verification is PENDING when initially packaged. Accepted 0.1.9-v2 and its profiles remain preserved. No Git upload is performed by this tool.')|Set-Content -LiteralPath (Join-Path $bundle 'README.md') -Encoding utf8
 Write-Output $bundle
}
