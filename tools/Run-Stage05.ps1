param([string]$Endpoint='EntropyAudioCore.Control.v1',[switch]$OfflineTestSignal)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$core=Join-Path $root 'build/msvc-stage05/EntropyAudioCore.exe'
$ui=Join-Path $root 'build/qt-stage05/EntropyUI.exe'
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;'+$env:PATH
$arguments=@('--control-server','--endpoint',$Endpoint)
if($OfflineTestSignal){$arguments+='--offline-test-signal'}
$coreProcess=Start-Process -FilePath $core -ArgumentList $arguments -PassThru -WindowStyle Hidden -WorkingDirectory $root
try {Start-Process -FilePath $ui -ArgumentList @('--endpoint',$Endpoint) -Wait -WorkingDirectory $root}
finally {if(!$coreProcess.HasExited){Stop-Process -Id $coreProcess.Id}}
