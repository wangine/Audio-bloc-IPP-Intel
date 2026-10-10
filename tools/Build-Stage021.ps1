param([ValidateSet(32768,65536)][int]$FifoFrames=32768,[string]$BuildDir,[switch]$Verify,[string]$HistoricalProfile,[string]$HistoricalTaps)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$BuildDir){$BuildDir=if($FifoFrames -eq 32768){'build/msvc-thiran'}else{'build/msvc-egress-65536'}}
$build=Join-Path $root $BuildDir
$logs=Join-Path $root ('local/stage021/build-'+$FifoFrames)
New-Item -ItemType Directory -Force $logs | Out-Null
$runner=Join-Path $logs 'build.cmd'
$cmd=@('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1','set "PATH=C:\Qt\Tools\Ninja;%PATH%"',('"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "'+$root+'" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DSTAGE04_REFERENCE="D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference" -DENTROPY_STAGE04_EGRESS=ON -DENTROPY_STAGE05_CONTROL=ON -DENTROPY_LIVE_FIFO_FRAMES='+$FifoFrames+' -DENTROPY_STABLE_STAGE3_DIRECTORY="D:/Projects/3Stage-3/Stage3" -DENTROPY_V07_INTEGRATION=OFF -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe" || exit /b 1'),('"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "'+$build+'" --target EntropyAudioCore EntropyStage021Headroom --parallel 2 || exit /b 1'))
Set-Content $runner $cmd -Encoding ascii
$oldPreference=$ErrorActionPreference
try {$ErrorActionPreference='Continue'; & $runner *> (Join-Path $logs 'build.txt');$buildExit=$LASTEXITCODE}
finally {$ErrorActionPreference=$oldPreference}
if($buildExit){Get-Content (Join-Path $logs 'build.txt') -Tail 30;throw 'Build failed'}
& (Join-Path $build 'EntropyAudioCore.exe') --probe *> (Join-Path $logs 'probe.txt');if($LASTEXITCODE){throw 'Probe failed'}
if($Verify){
 $arguments=@((Join-Path $logs 'results'))
 if($HistoricalProfile -or $HistoricalTaps){if(!$HistoricalProfile -or !$HistoricalTaps){throw 'Specify both historical paths'};$arguments+=@($HistoricalProfile,$HistoricalTaps)}
 & (Join-Path $build 'EntropyStage021Headroom.exe') @arguments *> (Join-Path $logs 'test.txt');if($LASTEXITCODE){Get-Content (Join-Path $logs 'test.txt');throw 'Targeted Headroom policy regression failed'}
}
Get-Content (Join-Path $logs 'probe.txt')
if($Verify){Get-Content (Join-Path $logs 'test.txt')}
Get-FileHash (Join-Path $build 'EntropyAudioCore.exe')
