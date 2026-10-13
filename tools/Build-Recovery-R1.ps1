param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$Profile,
 [string]$OutputRoot
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$oldLocation=Get-Location
$oldPath=$env:PATH
try {
 Set-Location -LiteralPath $root
 $head=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Unexpected source commit; no build started.'}
 $branch=(& git branch --show-current)
 if($LASTEXITCODE -ne 0 -or $branch -ne 'chatgpt/stage3-audio-recovery-20261013'){throw 'Wrong recovery branch.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source changes exist; do not build or overwrite them.'}
 & git diff --quiet 1b7a29d6d9f5aedb860c31aaa3374ad15b64bca7 HEAD -- UI
 if($LASTEXITCODE -ne 0){throw 'UI sources changed; do not reuse the baseline UI.'}
 $ui=Join-Path $root 'build/qt-recovery/EntropyUI.exe'
 $expectedUi='0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D'
 if(!(Test-Path -LiteralPath $ui) -or (Get-FileHash -LiteralPath $ui).Hash -ne $expectedUi){
  throw 'Expected already-built recovery UI is missing or different. Report it; do not substitute another UI.'
 }
 if(!$Profile){$Profile=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'entropy.entropy'}
 if(!(Test-Path -LiteralPath $Profile -PathType Leaf)){throw 'Saved profile not found. Supply -Profile with the existing local profile path; do not invent settings.'}
 $Profile=(Resolve-Path -LiteralPath $Profile).Path
 if(!$OutputRoot){$OutputRoot=Join-Path (Split-Path -Parent $root) 'live'}
 $OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
 $bundle=Join-Path $OutputRoot 'R1-64k'
 if(Test-Path -LiteralPath $bundle){throw 'R1 runtime already exists; preserve it. Select a new -OutputRoot.'}
 $deploy='C:/Qt/6.11.2/mingw_64/bin/windeployqt.exe'
 foreach($required in @($deploy,(Join-Path $root 'tools/Start-MusicCinema.ps1'),(Join-Path $root 'tools/Stop-MusicCinema.ps1'))){
  if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Required file missing: $required"}
 }
 # Build only the changed Core; the existing wrapper also runs its offline --probe.
 & (Join-Path $root 'tools/Build-MusicCinema.ps1') -FifoFrames 65536 -BaselineOnly -Targets @('EntropyAudioCore') -Parallel 2
 if(!$?){throw 'Core build failed.'}
 $core=Join-Path $root 'build/msvc-music-cinema-65536/EntropyAudioCore.exe'
 if(!(Test-Path -LiteralPath $core)){throw 'New Core executable missing.'}
 $coreHash=(Get-FileHash -LiteralPath $core).Hash
 if($coreHash -eq '93888B199884115F0EAF605503C36FF49AD1224F474136C676EC84CE16176E69'){
  throw 'Core is still the unmodified baseline executable; do not package.'
 }
 $after=(& git rev-parse HEAD);$dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $after -ne $head -or $dirty.Count){throw 'Source identity changed during build.'}
 $runtime=Join-Path $bundle 'runtime';$tools=Join-Path $bundle 'tools';$profileDir=Join-Path $bundle 'profile'
 New-Item -ItemType Directory -Path $runtime,$tools,$profileDir -Force|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'Packaging incomplete. Do not launch.'
 Copy-Item -LiteralPath $core,$ui -Destination $runtime
 Copy-Item -LiteralPath $Profile -Destination (Join-Path $profileDir 'listening.entropy')
 $env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$oldPath
 $preference=$ErrorActionPreference
 try {
  $ErrorActionPreference='Continue'
  & $deploy --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-ffmpeg --compiler-runtime (Join-Path $runtime 'EntropyUI.exe') *> (Join-Path $bundle 'qt-deploy.log')
  $deployExit=$LASTEXITCODE
 } finally {$ErrorActionPreference=$preference}
 if($deployExit -ne 0){throw "Qt deployment failed ($deployExit); see qt-deploy.log. No launcher was created."}
 if(!(Test-Path -LiteralPath (Join-Path $runtime 'platforms/qwindows.dll'))){throw 'Qt windows platform plugin missing.'}
 foreach($name in @('Start-MusicCinema.ps1','Stop-MusicCinema.ps1')){
  Copy-Item -LiteralPath (Join-Path $root "tools/$name") -Destination $tools
 }
 Copy-Item -LiteralPath (Join-Path $root 'RECOVERY-R1.md') -Destination (Join-Path $bundle 'README.md')
 $manifest=[ordered]@{version='Stage3-Recovery-R1';variant='64k';sourceCommit=$head;sourceBaseCommit='1b7a29d6d9f5aedb860c31aaa3374ad15b64bca7';fifoCapacityFrames=65536;targetFrames=4096;coreSha256=$coreHash;uiSha256=$expectedUi;automaticArm=$false;liveAudioVerified=$false;originalProfile=$Profile;profileCopy='profile/listening.entropy'}
 $manifest|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 # No smoke/live run, no automatic process stop, no DAC access.
 Set-Content -LiteralPath (Join-Path $bundle 'Cold-Start.cmd') -Encoding ascii -Value @('@echo off','if exist "%~dp0INCOMPLETE.txt" (echo Packaging incomplete. & exit /b 1)','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-MusicCinema.ps1" -Profile "%~dp0profile\listening.entropy" %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Encoding ascii -Value @('@echo off','call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Stop-AudioCore.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-MusicCinema.ps1"','exit /b %ERRORLEVEL%')
 $hashes=Get-ChildItem -LiteralPath $bundle -Recurse -File|Where-Object {$_.Name -notin @('SHA256SUMS.txt','INCOMPLETE.txt')}|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/')
 }
 $hashes|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 if((Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyAudioCore.exe')).Hash -ne $coreHash -or (Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyUI.exe')).Hash -ne $expectedUi){throw 'Packaged executable identity mismatch.'}
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "Prepared candidate: $bundle"
 Write-Host "Core SHA256: $coreHash"
 Write-Host 'STOP. No audio was tested. The owner starts Live-Test.cmd and selects ARM manually.'
} finally {
 $env:PATH=$oldPath
 Set-Location -LiteralPath $oldLocation.Path
}
