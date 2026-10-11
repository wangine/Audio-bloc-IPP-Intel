param([string]$CoreBuild='build/msvc-music-cinema-32768',[string]$UiBuild='build/qt-music-cinema',[string]$CoreExecutable,[string[]]$Tests=@('EntropyMusicCinemaUi','EntropyStage05Ui','EntropyStage06Ui','EntropyStage07Ui','EntropyStage07IpcTimeout','EntropyStage07DeviceInventory','EntropyStage017Ui'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
$results=@()
if(!$CoreExecutable){$CoreExecutable=Join-Path $root "$CoreBuild/EntropyAudioCore.exe"}
foreach($test in $Tests){
 $process=$null;$endpoint='EntropyAudioCore.UiRegression.'+[Guid]::NewGuid().ToString('N')
 try {
  if($test -in @('EntropyStage05Ui','EntropyStage06Ui')){
   $args=@('--control-server','--endpoint',$endpoint,'--run-seconds','20')
   if($test -eq 'EntropyStage05Ui'){$args+=@('--offline-test-signal','--offline-test-sinks')}else{$args+=@('--stage2-input','--profile',('"'+(Join-Path $root 'local/stage05/absent-ui-test.entropy')+'"'))}
   $process=Start-Process -FilePath $CoreExecutable -ArgumentList $args -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root "validation/music-cinema-v2/$test-core.log") -RedirectStandardError (Join-Path $root "validation/music-cinema-v2/$test-core-errors.log")
   Start-Sleep -Milliseconds 800
  }
  & (Join-Path $root "$UiBuild/$test.exe") @(@($endpoint) | Where-Object {$test -in @('EntropyStage05Ui','EntropyStage06Ui')}) *> (Join-Path $root "validation/music-cinema-v2/$test.log")
  $results+=[pscustomobject]@{test=$test;exit=$LASTEXITCODE}
 }finally{if($process -and !$process.HasExited){Stop-Process -Id $process.Id}}
}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'validation/music-cinema-v2/ui-tests.json') -Encoding utf8
$results | Format-Table
if(@($results|Where-Object exit -ne 0).Count){throw 'Qt regression failed; see individual logs'}
