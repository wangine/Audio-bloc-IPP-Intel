param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$Profile='C:\Users\bobi\Documents\entropy.entropy',
 [string]$Ui='D:\Projects\Entropy-Stage3-Recovery-20261013\source\build\qt-recovery\EntropyUI.exe',
 [string]$OutputRoot='D:\Projects\Entropy-Stage3-Recovery-20261013\live'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$old=Get-Location
$oldPath=$env:PATH
$base='3e985007885c527b5e3bf6580f853b82bf314d59'
$branch='chatgpt/stage3-live-renderer-v1-20261014'
$expectedUi='0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D'
function Run-Native([string]$Program,[string[]]$Arguments,[string]$Log) {
 $saved=$ErrorActionPreference
 try {$ErrorActionPreference='Continue'; & $Program @Arguments *> $Log; $code=$LASTEXITCODE}
 finally {$ErrorActionPreference=$saved}
 Get-Content -LiteralPath $Log -Tail 15
 if($code -ne 0){throw "Command failed ($code): $Program; see $Log. No automatic source repair."}
}
try {
 Set-Location -LiteralPath $root
 $head=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Wrong source SHA; no build.'}
 $actualBranch=(& git branch --show-current)
 if($LASTEXITCODE -ne 0 -or $actualBranch -ne $branch){throw 'Wrong live-renderer branch.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source changes exist; do not reset/stash/discard them.'}
 & git merge-base --is-ancestor $base HEAD
 if($LASTEXITCODE -ne 0){throw 'The verified Stage A report commit is not an ancestor.'}
 # These algorithms and the existing UI/transport contracts are not this rewrite.
 & git diff --quiet $base HEAD -- DSP Egress UI Transport Shared Engine/Input Engine/AudioGraph.cpp Engine/AudioScheduler.cpp Engine/Control/ProfileStore.cpp tools/Start-MusicCinema.ps1 tools/Stop-MusicCinema.ps1
 if($LASTEXITCODE -ne 0){throw 'Protected DSP, PCM24, UI, input, profile or launcher source changed.'}
 foreach($file in @($Profile,$Ui)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Required existing file missing: $file"}}
 if((Get-FileHash -LiteralPath $Ui -Algorithm SHA256).Hash -ne $expectedUi){throw 'The previously verified Qt UI differs; do not substitute another binary.'}
 $Profile=(Resolve-Path -LiteralPath $Profile).Path
 $Ui=(Resolve-Path -LiteralPath $Ui).Path
 $profileHash=(Get-FileHash -LiteralPath $Profile -Algorithm SHA256).Hash
 $bundle=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) 'LiveV1-20261014'
 if(Test-Path -LiteralPath $bundle){throw 'LiveV1 package exists. Preserve it; choose a new explicit OutputRoot.'}
 $cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
 $vcvars='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
 $deploy='C:\Qt\6.11.2\mingw_64\bin\windeployqt.exe'
 foreach($file in @($cmake,$vcvars,$deploy,'C:\Qt\Tools\Ninja\ninja.exe')){
  if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Build dependency missing: $file"}
 }
 $runtime=Join-Path $bundle 'runtime';$tools=Join-Path $bundle 'tools';$profileDir=Join-Path $bundle 'profile'
 New-Item -ItemType Directory -Path $bundle,$runtime,$tools,$profileDir|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'LiveV1 build/package incomplete. Do not launch.'
 $build=Join-Path $root 'build/live-v1'
 $runner=Join-Path $bundle 'build.cmd'
 $commands=@('@echo off',('call "'+$vcvars+'" || exit /b 1'),'set "PATH=C:\Qt\Tools\Ninja;%PATH%"',
  ('"'+$cmake+'" -S "'+$root+'" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBUILD_TESTING=OFF -DENTROPY_STAGE02_VALIDATION=OFF -DENTROPY_STAGE04_EGRESS=ON -DENTROPY_STAGE05_CONTROL=ON -DENTROPY_LIVE_FIFO_FRAMES=65536 -DENTROPY_V07_INTEGRATION=OFF -DENTROPY_STABLE_STAGE3_DIRECTORY="D:/Projects/3Stage-3/Stage3" -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe" || exit /b 1'),
  ('"'+$cmake+'" --build "'+$build+'" --parallel 2 --target EntropyAudioCore || exit /b 1'))
 $commands|Set-Content -LiteralPath $runner -Encoding ascii
 # Configure may fetch the exact Cockos WDL commit pinned in Wdl.cmake.
 # No Rust/Faust, driver or system package installation is performed.
 Run-Native -Program 'cmd.exe' -Arguments @('/d','/c',('"'+$runner+'"')) -Log (Join-Path $bundle 'build.log')
 $core=Join-Path $build 'EntropyAudioCore.exe'
 if(!(Test-Path -LiteralPath $core -PathType Leaf)){throw 'New live Core executable missing.'}
 if(!(Select-String -LiteralPath (Join-Path $bundle 'build.log') -SimpleMatch 'Stage3 physical output: LiveV1' -Quiet)){
  throw 'Configuration did not identify the new physical renderer.'
 }
 Run-Native -Program $core -Arguments @('--probe') -Log (Join-Path $bundle 'probe.log')
 $coreHash=(Get-FileHash -LiteralPath $core).Hash
 if($coreHash -in @('E915D0080BAF555E35F209BBA68133A70AEBAA0B47C0F726CD4C1A4AB0F16C73','3E4DAFD030E01EFA2BDF4C2B5C81E866D747519A6ECA6908BA5E3B61D1188A86','77053742D68B68088D6F99D4AD7D42241B6A8D87B5F7B3EDD1360B99108B5B5A')){
  throw 'Core hash still matches an old recovery runtime.'
 }
 Copy-Item -LiteralPath $core,$Ui -Destination $runtime
 $profileCopy=Join-Path $profileDir 'listening.entropy'
 Copy-Item -LiteralPath $Profile -Destination $profileCopy
 if((Get-FileHash -LiteralPath $Profile).Hash -ne $profileHash -or (Get-FileHash -LiteralPath $profileCopy).Hash -ne $profileHash){throw 'Saved profile changed during copy.'}
 $env:PATH='C:\Qt\6.11.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;'+$oldPath
 Run-Native -Program $deploy -Arguments @('--release','--no-translations','--no-system-d3d-compiler','--no-system-dxc-compiler','--no-opengl-sw','--no-ffmpeg','--compiler-runtime',(Join-Path $runtime 'EntropyUI.exe')) -Log (Join-Path $bundle 'qt-deploy.log')
 if(!(Test-Path -LiteralPath (Join-Path $runtime 'platforms/qwindows.dll'))){throw 'Qt platform plugin missing.'}
 foreach($name in @('Start-MusicCinema.ps1','Stop-MusicCinema.ps1')){
  Copy-Item -LiteralPath (Join-Path $root "tools/$name") -Destination $tools
 }
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Start-LiveV1.ps1') -Destination $tools
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LIVE-V1.md') -Destination (Join-Path $bundle 'README.md')
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'WDL-NOTICE.txt') -Destination (Join-Path $bundle 'WDL-NOTICE.txt')
 $scriptHashes=[ordered]@{}
 foreach($name in @('Start-MusicCinema.ps1','Stop-MusicCinema.ps1','Start-LiveV1.ps1')){$scriptHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $tools $name)).Hash}
 $manifest=[ordered]@{
  schema='entropy-live-v1-package-v1';version='0.2.0-live-v1';variant='FP64-192-prepared-output';sourceCommit=$head;sourceBase=$base;
  coreSha256=$coreHash;uiSha256=$expectedUi;scriptSha256=$scriptHashes;initialProfileSha256=$profileHash;originalProfile=$Profile;profileCopy='profile/listening.entropy';
  wdlCommit='2b260b915f8ca7cb7859b9c0b7ba7e48b166f0af';asrc='WDL FP64 sinc128 fixed bank, 0.999..1.001';
  sourceQueueCapacity=65536;targetUnsubmittedFrames=8192;readyPackets=4;independentDacWorkers=$true;
  automaticArm=$false;liveAudioVerified=$false;twoDacAlignmentVerified=$false
 }
 $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-LiveV1.ps1"','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Stop-AudioCore.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-MusicCinema.ps1"','exit /b %ERRORLEVEL%')
 $after=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $after -ne $head){throw 'Source revision changed during build.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked sources changed during build.'}
 if((Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyAudioCore.exe')).Hash -ne $coreHash -or (Get-FileHash -LiteralPath (Join-Path $runtime 'EntropyUI.exe')).Hash -ne $expectedUi){throw 'Packaged executable identity mismatch.'}
 if((Get-FileHash -LiteralPath $Profile).Hash -ne $profileHash){throw 'Original profile changed during packaging.'}
 Get-ChildItem -LiteralPath $bundle -File -Recurse|Where-Object {$_.Name -notin @('INCOMPLETE.txt','SHA256SUMS.txt')}|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/')
 }|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "LiveV1 package prepared: $bundle"
 Write-Host "Source: $head"
 Write-Host "Core SHA256: $coreHash"
 Write-Host 'STOP. No playback, DAC, ARM, source restart or process termination was performed.'
 Write-Host 'The owner closes the old session and starts Live-Test.cmd; the existing UI starts DISARMED.'
} finally {$env:PATH=$oldPath;Set-Location -LiteralPath $old.Path}
