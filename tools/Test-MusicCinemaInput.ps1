param([int]$Seconds=10,[string]$OutputDirectory='validation/music-cinema-v2/live-32768',[int]$FifoFrames=32768)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$out=if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $root $OutputDirectory}
& 'C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe' (Join-Path $root 'Tests/MusicCinemaLiveInput.py') (Join-Path $root "build/msvc-music-cinema-$FifoFrames/EntropyAudioCore.exe") $out $Seconds
if($LASTEXITCODE){throw 'Real input validation failed; DACs remained closed'}
