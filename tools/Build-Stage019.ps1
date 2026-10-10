param([ValidateSet(32768,65536)][int]$FifoFrames=32768,[switch]$Verify,[string]$ReferenceDirectory="D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference")
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=Join-Path $root ('build/msvc-egress-'+$FifoFrames)
$logs=Join-Path $root ('local/stage019/build-'+$FifoFrames)
New-Item -ItemType Directory -Force $logs | Out-Null
$runner=Join-Path $logs 'build.cmd'
$cmd=@('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1','set "PATH=C:\Qt\Tools\Ninja;%PATH%"',('"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S "'+$root+'" -B "'+$build+'" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DSTAGE04_REFERENCE="'+$ReferenceDirectory+'" -DENTROPY_STAGE04_EGRESS=ON -DENTROPY_STAGE05_CONTROL=ON -DENTROPY_LIVE_FIFO_FRAMES='+$FifoFrames+' -DENTROPY_STABLE_STAGE3_DIRECTORY="D:/Projects/3Stage-3/Stage3" -DENTROPY_V07_INTEGRATION=OFF -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe" || exit /b 1'),('"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "'+$build+'" --target EntropyAudioCore EntropyStage019Egress --parallel 2 || exit /b 1'))
Set-Content $runner $cmd -Encoding ascii
$previousPreference=$ErrorActionPreference
try {$ErrorActionPreference='Continue'; & $runner *> (Join-Path $logs 'build.txt');$buildExit=$LASTEXITCODE}
finally {$ErrorActionPreference=$previousPreference}
if($buildExit){Get-Content (Join-Path $logs 'build.txt') -Tail 35;throw 'Build failed'}
if($Verify){& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $build -R '^EntropyStage019Egress$' --output-on-failure;if($LASTEXITCODE){throw 'Chronology verification failed'}}
Get-FileHash (Join-Path $build 'EntropyAudioCore.exe')
