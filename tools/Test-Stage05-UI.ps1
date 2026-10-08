param([string]$NativeBuild='build/msvc-stage05',[string]$UiBuild='build/qt-stage05')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
$endpoint='EntropyAudioCore.Stage05.UI.'+[Guid]::NewGuid().ToString('N')
New-Item -ItemType Directory -Force local/stage05 | Out-Null
$core=Start-Process -FilePath (Join-Path $root "$NativeBuild/EntropyAudioCore.exe") -ArgumentList @('--control-server','--endpoint',$endpoint,'--offline-test-signal','--run-seconds','30') -PassThru -WindowStyle Hidden -WorkingDirectory $root -RedirectStandardOutput (Join-Path $root 'local/stage05/ui-core.txt') -RedirectStandardError (Join-Path $root 'local/stage05/ui-core-errors.txt')
try {
 & "$UiBuild/EntropyStage05Ui.exe" $endpoint *> local/stage05/ui-test-final.txt
 if($LASTEXITCODE){throw 'Qt controls integration FAIL'}
 & "$UiBuild/EntropyUI.exe" --endpoint $endpoint --smoke-seconds 2 --capture-ui local/stage05/ui-captures/executable.png *> local/stage05/ui-executable.txt
 if($LASTEXITCODE){throw 'EntropyUI executable smoke FAIL'}
 Get-Content local/stage05/ui-test-final.txt
 Get-Content local/stage05/ui-executable.txt
} finally {if(!$core.HasExited){Stop-Process -Id $core.Id}}
