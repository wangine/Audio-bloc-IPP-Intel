param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$Profile='C:\Users\bobi\Documents\entropy.entropy',
 [string]$Capture='D:\Projects\Entropy-Stage3-Recovery-20261013\pcm-frontier-20261014\stage2-fp64.bin',
 [string]$OutputRoot='D:\Projects\Entropy-Stage3-Recovery-20261013\live'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$old=Get-Location
$base='3e985007885c527b5e3bf6580f853b82bf314d59'
$branch='chatgpt/stage3-renderer-b1-20261014'
$expectedCapture='737dad1df5d0f2236219caa954c46d501e10254041acd4c51470d2beea89c051'
function Run-Native([string]$Program,[string[]]$Arguments,[string]$Log){
 $before=$ErrorActionPreference
 try {$ErrorActionPreference='Continue'; & $Program @Arguments *> $Log; $code=$LASTEXITCODE}
 finally {$ErrorActionPreference=$before}
 Get-Content -LiteralPath $Log -Tail 12
 if($code -ne 0){throw "Command failed ($code): $Program; see $Log. Stop; no source repair."}
}
try {
 Set-Location -LiteralPath $root
 $head=& git rev-parse HEAD
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Wrong source SHA; no build.'}
 $actualBranch=& git branch --show-current
 if($LASTEXITCODE -ne 0 -or $actualBranch -ne $branch){throw 'Wrong source branch; do not repurpose the recovery checkout.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked changes present. Do not reset/stash/discard them.'}
 & git merge-base --is-ancestor $base HEAD
 if($LASTEXITCODE -ne 0){throw 'Report commit is not an ancestor.'}
 $changed=@(& git diff --name-only $base HEAD)
 if($LASTEXITCODE -ne 0){throw 'Cannot inspect source scope.'}
 foreach($name in $changed){if($name -ne 'AGENTS.md' -and !$name.StartsWith('RendererB1/')){throw "Protected source changed: $name"}}
 foreach($file in @($Profile,$Capture)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Required local input missing: $file"}}
 if((Get-FileHash -LiteralPath $Capture -Algorithm SHA256).Hash -ne $expectedCapture){throw 'Stage2 capture hash differs from the supplied report. No substitute capture.'}
 $profileHash=(Get-FileHash -LiteralPath $Profile -Algorithm SHA256).Hash
 $bundle=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) 'B1-20261014'
 if(Test-Path -LiteralPath $bundle){throw 'B1 destination exists. Preserve it; supply a new explicit OutputRoot.'}
 $cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
 $vcvars='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
 foreach($file in @($cmake,$vcvars,'C:\Qt\Tools\Ninja\ninja.exe')){if(!(Test-Path -LiteralPath $file)){throw "Build dependency missing: $file"}}
 New-Item -ItemType Directory -Path $bundle,(Join-Path $bundle 'runtime'),(Join-Path $bundle 'tools'),(Join-Path $bundle 'profile')|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'Candidate not ready. Do not play.'
 $build=Join-Path $root 'build/renderer-b1'
 $runner=Join-Path $bundle 'build.cmd'
 $commands=@('@echo off',('call "'+$vcvars+'" || exit /b 1'),'set "PATH=C:\Qt\Tools\Ninja;%PATH%"',
  ('"'+$cmake+'" -S "'+$PSScriptRoot+'" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DENTROPY_STABLE_STAGE3_DIRECTORY="D:/Projects/3Stage-3/Stage3" -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe" || exit /b 1'),
  ('"'+$cmake+'" --build "'+$build+'" --parallel 2 --target EntropyPrepareB1 EntropyPlayB1 EntropyPacketB1Tests || exit /b 1'))
 $commands|Set-Content -LiteralPath $runner -Encoding ascii
 Run-Native -Program 'cmd.exe' -Arguments @('/d','/c',('"'+$runner+'"')) -Log (Join-Path $bundle 'build.log')
 Run-Native -Program (Join-Path $build 'EntropyPacketB1Tests.exe') -Arguments @() -Log (Join-Path $bundle 'packet-tests.log')
 $profileCopy=Join-Path $bundle 'profile/listening.entropy'
 Copy-Item -LiteralPath $Profile -Destination $profileCopy
 if((Get-FileHash -LiteralPath $Profile).Hash -ne $profileHash -or (Get-FileHash -LiteralPath $profileCopy).Hash -ne $profileHash){throw 'Profile changed during copying.'}
 $prepared=Join-Path $bundle 'prepared'
 Run-Native -Program (Join-Path $build 'EntropyPrepareB1.exe') -Arguments @($profileCopy,$Capture,$prepared) -Log (Join-Path $bundle 'prepare.log')
 if(Test-Path -LiteralPath (Join-Path $prepared 'INCOMPLETE.txt')){throw 'Prepared PCM is incomplete.'}
 $pcmManifest=Get-Content -LiteralPath (Join-Path $prepared 'prepared.json') -Raw|ConvertFrom-Json
 if(!$pcmManifest.inputCopyExact -or !$pcmManifest.outputs.Count){throw 'Missing input/mapping result.'}
 $outputs=@()
 foreach($entry in $pcmManifest.outputs){
  if($entry.file -notin @('dac-a.pcm','dac-b.pcm')){throw 'Unexpected prepared filename.'}
  $pcmPath=Join-Path $prepared $entry.file
  if(!(Test-Path -LiteralPath $pcmPath -PathType Leaf)){throw 'Prepared PCM missing.'}
  $entry|Add-Member -NotePropertyName sha256 -NotePropertyValue (Get-FileHash -LiteralPath $pcmPath -Algorithm SHA256).Hash
  $outputs+=$entry
 }
 $player=Join-Path $bundle 'runtime/EntropyPlayB1.exe'
 Copy-Item -LiteralPath (Join-Path $build 'EntropyPlayB1.exe') -Destination $player
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Play-B1.ps1') -Destination (Join-Path $bundle 'tools/Play-B1.ps1')
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DESIGN.md') -Destination (Join-Path $bundle 'README.md')
 $manifest=[ordered]@{schema='entropy-b1-package-v1';sourceCommit=$head;sourceBase=$base;captureSha256=$expectedCapture;profileSha256=$profileHash;playerSha256=(Get-FileHash -LiteralPath $player).Hash;playScriptSha256=(Get-FileHash -LiteralPath (Join-Path $bundle 'tools/Play-B1.ps1')).Hash;outputs=$outputs;sourceFrames=$pcmManifest.sourceFrames;tailFrames=$pcmManifest.tailFrames;headroomMode=$pcmManifest.headroomMode;manualGainDb=$pcmManifest.manualGainDb;liveReplacement=$false;audioQualityVerified=$false}
 $manifest|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 foreach($dac in @('A','B')){
  if(@($outputs|Where-Object {$_.dac -eq $dac}).Count){
   Set-Content -LiteralPath (Join-Path $bundle "Play-DAC-$dac.cmd") -Encoding ascii -Value @('@echo off','if exist "%~dp0INCOMPLETE.txt" (echo Package incomplete. & exit /b 1)',('powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Play-B1.ps1" -Dac '+$dac),'exit /b %ERRORLEVEL%')
  }
 }
 $after=& git rev-parse HEAD
 if($LASTEXITCODE -ne 0 -or $after -ne $head){throw 'Source revision changed during preparation.'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source changed during preparation.'}
 if((Get-FileHash -LiteralPath $Capture).Hash -ne $expectedCapture -or (Get-FileHash -LiteralPath $Profile).Hash -ne $profileHash){throw 'An original input changed during preparation.'}
 Get-ChildItem -LiteralPath $bundle -File -Recurse|Where-Object {$_.Name -notin @('INCOMPLETE.txt','SHA256SUMS.txt')}|Sort-Object FullName|ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/') }|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "Prepared isolated B1 candidate: $bundle"
 Write-Host 'STOP. Do not run Play-DAC-A/B. No live source, driver, DAC or audio process has been controlled.'
} finally {Set-Location -LiteralPath $old.Path}
