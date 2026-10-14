param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$Profile,
 [string]$OutputRoot
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$oldLocation=Get-Location
$oldPath=$env:PATH
$base='841e0ada1a1e2867a3fdb7af692ca0e7a8081719'
$r1Hash='77053742D68B68088D6F99D4AD7D42241B6A8D87B5F7B3EDD1360B99108B5B5A'
try {
 Set-Location -LiteralPath $root
 $head=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Unexpected source commit; no build started.'}
 $branch=(& git branch --show-current)
 if($LASTEXITCODE -ne 0 -or $branch -ne 'chatgpt/stage3-audio-recovery-20261013'){throw 'Wrong recovery branch.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source changes exist; do not reset or overwrite them.'}
 & git merge-base --is-ancestor $base HEAD
 if($LASTEXITCODE -ne 0){throw 'R1 is not an ancestor; do not build an unrelated revision.'}
 # R2 is one transport change. All DSP and egress algorithms must remain R1.
 & git diff --quiet $base HEAD -- DSP Egress Engine/Egress Engine/AudioGraph.cpp Engine/AudioScheduler.cpp Engine/Control Transport Integration Drivers Shared UI local CMakeLists.txt cmake tools/Build-MusicCinema.ps1 tools/Start-MusicCinema.ps1 tools/Stop-MusicCinema.ps1
 if($LASTEXITCODE -ne 0){throw 'Protected R1 audio/UI/dependency/build source changed. Stop; do not substitute another revision.'}
 $ui=Join-Path $root 'build/qt-recovery/EntropyUI.exe'
 $expectedUi='0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D'
 if(!(Test-Path -LiteralPath $ui) -or (Get-FileHash -LiteralPath $ui).Hash -ne $expectedUi){throw 'Expected already-built recovery UI missing/different; do not substitute another UI.'}
 $r1=Join-Path (Split-Path -Parent $root) 'live/R1-64k'
 $r1Core=Join-Path $r1 'runtime/EntropyAudioCore.exe'
 if(!(Test-Path -LiteralPath $r1Core) -or (Get-FileHash -LiteralPath $r1Core).Hash -ne $r1Hash){throw 'The preserved R1 runtime does not match the supplied build report; stop and report the path/hash.'}
 # Use the SAVED R1 listening profile, not a possibly different mother profile.
 # Unsaved UI adjustments cannot be inferred here. Never change gain or mode.
 if(!$Profile){$Profile=Join-Path $r1 'profile/listening.entropy'}
 if(!(Test-Path -LiteralPath $Profile -PathType Leaf)){throw 'Saved R1 profile missing; supply -Profile for an exact existing saved profile. Do not invent settings.'}
 $Profile=(Resolve-Path -LiteralPath $Profile).Path
 $profileHash=(Get-FileHash -LiteralPath $Profile).Hash
 if(!$OutputRoot){$OutputRoot=Join-Path (Split-Path -Parent $root) 'live'}
 $OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
 $bundle=Join-Path $OutputRoot 'R2-64k'
 if(Test-Path -LiteralPath $bundle){throw 'R2 runtime already exists; preserve it. Use a new explicitly recorded -OutputRoot.'}
 $deploy='C:/Qt/6.11.2/mingw_64/bin/windeployqt.exe'
 foreach($required in @($deploy,(Join-Path $root 'tools/Start-MusicCinema.ps1'),(Join-Path $root 'tools/Stop-MusicCinema.ps1'))){
  if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required file missing: $required"}
 }
 & (Join-Path $root 'tools/Build-MusicCinema.ps1') -FifoFrames 65536 -BaselineOnly -Targets @('EntropyAudioCore') -Parallel 2
 if(!$?){throw 'Core build failed.'}
 $core=Join-Path $root 'build/msvc-music-cinema-65536/EntropyAudioCore.exe'
 if(!(Test-Path -LiteralPath $core)){throw 'New Core executable missing.'}
 $coreHash=(Get-FileHash -LiteralPath $core).Hash
 if($coreHash -eq $r1Hash -or $coreHash -eq '93888B199884115F0EAF605503C36FF49AD1224F474136C676EC84CE16176E69'){throw 'Core is still R1 or the original baseline; do not package.'}
 $after=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $after -ne $head){throw 'Source commit changed during build.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source changed during build.'}
 $runtime=Join-Path $bundle 'runtime';$tools=Join-Path $bundle 'tools';$profileDir=Join-Path $bundle 'profile'
 New-Item -ItemType Directory -Path $runtime,$tools,$profileDir -Force|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'Packaging incomplete. Do not launch.'
 Copy-Item -LiteralPath $core,$ui -Destination $runtime
 $profileCopy=Join-Path $profileDir 'listening.entropy'
 Copy-Item -LiteralPath $Profile -Destination $profileCopy
 if((Get-FileHash -LiteralPath $Profile).Hash -ne $profileHash -or (Get-FileHash -LiteralPath $profileCopy).Hash -ne $profileHash){throw 'Profile changed during copy; do not launch.'}
 $env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$oldPath
 $preference=$ErrorActionPreference
 try {
  $ErrorActionPreference='Continue'
  & $deploy --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-ffmpeg --compiler-runtime (Join-Path $runtime 'EntropyUI.exe') *> (Join-Path $bundle 'qt-deploy.log')
  $deployExit=$LASTEXITCODE
 } finally {$ErrorActionPreference=$preference}
 if($deployExit -ne 0){throw "Qt deployment failed ($deployExit); see qt-deploy.log. No launcher was created."}
 if(!(Test-Path -LiteralPath (Join-Path $runtime 'platforms/qwindows.dll'))){throw 'Qt windows platform plugin missing.'}
 foreach($name in @('Start-MusicCinema.ps1','Stop-MusicCinema.ps1')){Copy-Item -LiteralPath (Join-Path $root "tools/$name") -Destination $tools}
 Copy-Item -LiteralPath (Join-Path $root 'RECOVERY-R2.md') -Destination (Join-Path $bundle 'README.md')
 $manifest=[ordered]@{version='Stage3-Recovery-R2';variant='64k';sourceCommit=$head;sourceBaseCommit=$base;fifoCapacityFrames=65536;targetFrames=4096;ingressCapacityBlocks=16;coreSha256=$coreHash;uiSha256=$expectedUi;automaticArm=$false;liveAudioVerified=$false;originalProfile=$Profile;profileCopy='profile/listening.entropy';profileSha256=$profileHash;changeScope='Input SPSC and independent telemetry; DSP and egress remain R1'}
 $manifest|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 Set-Content -LiteralPath (Join-Path $bundle 'Cold-Start.cmd') -Encoding ascii -Value @('@echo off','if exist "%~dp0INCOMPLETE.txt" (echo Packaging incomplete. & exit /b 1)','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-MusicCinema.ps1" -Profile "%~dp0profile\listening.entropy" %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Encoding ascii -Value @('@echo off','call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Stop-AudioCore.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-MusicCinema.ps1"','exit /b %ERRORLEVEL%')
 $hashes=Get-ChildItem -LiteralPath $bundle -Recurse -File|Where-Object {$_.Name -notin @('SHA256SUMS.txt','INCOMPLETE.txt')}|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/')
 }
 $hashes|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 if((Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyAudioCore.exe')).Hash -ne $coreHash -or (Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyUI.exe')).Hash -ne $expectedUi){throw 'Packaged executable identity mismatch.'}
 if((Get-FileHash -LiteralPath $r1Core).Hash -ne $r1Hash){throw 'Preserved R1 executable changed unexpectedly.'}
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "Prepared candidate: $bundle"
 Write-Host "Source commit: $head"
 Write-Host "Core SHA256: $coreHash"
 Write-Host 'STOP. No DAC/playback was tested. The owner starts Live-Test.cmd and selects ARM manually.'
} finally {
 $env:PATH=$oldPath
 Set-Location -LiteralPath $oldLocation.Path
}
