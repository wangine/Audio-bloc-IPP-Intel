param(
 [ValidateSet(32768,65536)][int]$FifoFrames=32768,
 [switch]$BaselineOnly,
 [switch]$Verify,
 [string[]]$Targets=@(),
 [int]$Parallel=2
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=Join-Path $root ('build/msvc-music-cinema-'+$FifoFrames)
$logs=Join-Path $root 'validation/music-cinema-v2'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root 'local/stage05') | Out-Null
$runner=Join-Path $logs ('build-'+$FifoFrames+'.cmd')
$cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
$reference=Join-Path $root 'local/stage04-ref/Stage3/native-reference'
if(!(Test-Path -LiteralPath $reference)){$reference='D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference'}
$thiran=Join-Path $root 'local/thiran-baseline'
$configure='"'+$cmake+'" -S "'+$root+'" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBUILD_TESTING=ON -DENTROPY_STAGE04_EGRESS=ON -DENTROPY_STAGE05_CONTROL=ON -DENTROPY_LIVE_FIFO_FRAMES='+$FifoFrames+' -DENTROPY_STABLE_STAGE3_DIRECTORY="D:/Projects/3Stage-3/Stage3" -DSTAGE04_REFERENCE="'+$reference+'" -DTHIRAN_BASELINE="'+$thiran+'" -DENTROPY_V07_INTEGRATION=OFF -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe" || exit /b 1'
$buildCommand='"'+$cmake+'" --build "'+$build+'" --parallel '+$Parallel
if($Targets.Count){
 foreach($target in $Targets){if($target -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid build target'}}
 $buildCommand+=' --target '+($Targets -join ' ')
}
$commands=@('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1','set "PATH=C:\Qt\Tools\Ninja;%PATH%"',$configure,($buildCommand+' || exit /b 1'))
Set-Content -LiteralPath $runner -Value $commands -Encoding ascii
$log=Join-Path $logs ('build-'+$FifoFrames+'.log')
$previousPreference=$ErrorActionPreference
try {
 $ErrorActionPreference='Continue'
 & cmd.exe /d /c ('"'+$runner+'"') *> $log
 $result=$LASTEXITCODE
}
finally {$ErrorActionPreference=$previousPreference}
Get-Content -LiteralPath $log -Tail 12
if($result){throw "Native build failed ($result); see $log"}
if($Verify){
 $testLog=Join-Path $logs ('ctest-'+$FifoFrames+'.log')
 & 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $build --output-on-failure *> $testLog
 $testResult=$LASTEXITCODE
 Get-Content -LiteralPath $testLog -Tail 28
 if($testResult){Write-Warning "CTest returned $testResult; failures retained in $testLog"}
}
$core=Join-Path $build 'EntropyAudioCore.exe'
if(Test-Path -LiteralPath $core){
 & $core --probe *> (Join-Path $logs ('probe-'+$FifoFrames+'.log'))
 if($LASTEXITCODE){throw 'Core probe failed'}
 Get-FileHash -LiteralPath $core
}
if($BaselineOnly){Write-Output 'Baseline build only; no producer, UI or physical output started.'}
if($Verify -and $testResult){exit $testResult}

