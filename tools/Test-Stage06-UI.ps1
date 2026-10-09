param([string]$NativeBuild='build/msvc-stage06',[string]$UiBuild='build/qt-stage06')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$endpoint='EntropyAudioCore.Stage06.FirstRun.'+[Guid]::NewGuid().ToString('N')
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
$missing=Join-Path $root 'local/stage06/does-not-exist.entropy'
$core=Start-Process -FilePath (Join-Path $root "$NativeBuild/EntropyAudioCore.exe") -ArgumentList @('--control-server','--endpoint',$endpoint,'--stage2-input','--profile',$missing,'--run-seconds','15') -PassThru -WindowStyle Hidden -WorkingDirectory $root -RedirectStandardOutput (Join-Path $root 'local/stage06/first-run-core.txt') -RedirectStandardError (Join-Path $root 'local/stage06/first-run-core-errors.txt')
try {
    & (Join-Path $root "$UiBuild/EntropyStage06Ui.exe") $endpoint *> (Join-Path $root 'local/stage06/first-run-qt.txt')
    Get-Content (Join-Path $root 'local/stage06/first-run-qt.txt')
    if($LASTEXITCODE){throw 'Qt first-run profile dialog FAIL'}
}finally{if(!$core.HasExited){Stop-Process -Id $core.Id}}
