param([string]$RuntimeRoot)
$ErrorActionPreference='Stop'
$stage1ProjectRoot=Split-Path $PSScriptRoot -Parent
$stage1TestRoot=Join-Path $stage1ProjectRoot "evidence\qt-smoke-$PID"
New-Item -ItemType Directory -Path $stage1TestRoot -Force | Out-Null
$stage1Stop=Join-Path $stage1TestRoot 'stop'
$stage1Config=Join-Path $stage1TestRoot 'candidate.ini'
$stage1Image=Join-Path $stage1ProjectRoot 'evidence\qt-smoke.png'
$stage1Pipe="\\.\pipe\Entropy.Stage1.QtSmoke.$PID"
@('[music]','providerId=synthetic.music','endpointId=fixture','mode=1','[aux]') | Set-Content -LiteralPath $stage1Config -Encoding ascii
$stage1Arguments=@('--config',"`"$stage1Config`"",'--provider-directory',"`"$stage1ProjectRoot\build\test-providers`"",'--publish-endpoint',"`"$stage1Pipe`"",'--control-endpoint',"`"$stage1Pipe.Control`"",'--stop-file',"`"$stage1Stop`"")
$stage1CoreExe="$stage1ProjectRoot\build\EntropyMusicSourceCore.exe"
$stage1QtExe="$stage1ProjectRoot\build-qt\EntropyMusicSourceQt.exe"
if($RuntimeRoot){$stage1CoreExe=Join-Path $RuntimeRoot 'core\EntropyMusicSourceCore.exe';$stage1QtExe=Join-Path $RuntimeRoot 'ui\EntropyMusicSourceQt.exe'}
$stage1Core=Start-Process -FilePath $stage1CoreExe -ArgumentList $stage1Arguments -PassThru -WindowStyle Hidden
try {
 if($RuntimeRoot){$env:PATH="$env:SystemRoot\System32;$env:SystemRoot";Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue;Remove-Item Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue}else{$env:PATH='C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.2\mingw_64\bin;'+$env:PATH}
 $env:QT_QPA_PLATFORM='offscreen'
 for($stage1Attempt=0;$stage1Attempt -lt 2;$stage1Attempt++){
  $stage1Qt=Start-Process -FilePath $stage1QtExe -ArgumentList @('--control-endpoint',"`"$stage1Pipe.Control`"",'--smoke-file',"`"$stage1Image`"") -PassThru -Wait -WindowStyle Hidden -RedirectStandardError "$stage1TestRoot\qt-stderr.txt"
  if($stage1Qt.ExitCode -or !(Test-Path -LiteralPath $stage1Image)){Get-Content "$stage1TestRoot\qt-stderr.txt";throw "Qt IPC smoke failed: $($stage1Qt.ExitCode)"}
  $stage1Status=Get-Content -LiteralPath "$stage1Image.json" -Raw | ConvertFrom-Json
  if($stage1Status.musicFormat -ne '24/192'){throw 'Original source format missing from Qt'}
  if($stage1Status.musicAccess -ne 'WASAPI access unverified'){throw 'Unconfirmed access must not be presented as Shared or Exclusive'}
  if($stage1Attempt -and ($stage1Status.sessionId -ne $stage1FirstStatus.sessionId -or $stage1Status.blocksPublished -le $stage1FirstStatus.blocksPublished)){throw 'Qt restart changed source identity or interrupted publication'}
  $stage1FirstStatus=$stage1Status
 }
} finally {
 Set-Content -LiteralPath $stage1Stop -Value '1' -Encoding ascii
 if(!$stage1Core.WaitForExit(5000)){Stop-Process -Id $stage1Core.Id;throw 'Own smoke Core failed graceful stop'}
}
Write-Output "PASS Qt MinGW IPC with MSVC Core; original 24/192 input and native clock; screenshot: $stage1Image"
