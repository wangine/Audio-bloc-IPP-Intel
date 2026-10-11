param([string]$Endpoint='EntropyAudioCore.Control.v1',[string]$Profile,[switch]$AuthorizedLiveTest,[switch]$SmokeTest)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runtime=Join-Path $root 'runtime';$core=Join-Path $runtime 'EntropyAudioCore.exe';$ui=Join-Path $runtime 'EntropyUI.exe'
$process=$null;$uiProcess=$null;$started=$false
try {
 if($SmokeTest -and $AuthorizedLiveTest){throw 'Offline smoke cannot authorize physical output'}
 if($SmokeTest){$Endpoint='EntropyAudioCore.MusicCinema.Smoke.'+[Guid]::NewGuid().ToString('N')}
 if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){throw 'Invalid local control endpoint'}
 $manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
 Write-Host "Entropy Stage3 $($manifest.version) / $($manifest.variant); MUSIC3 + independent CINEMA6/8; starts DISARMED"
 $pattern='--endpoint\s+"?'+[regex]::Escape($Endpoint)+'"?(\s|$)'
 $existing=@(Get-CimInstance Win32_Process -Filter "name='EntropyAudioCore.exe'"|Where-Object {$_.CommandLine -match $pattern})
 if($existing.Count -gt 1){throw 'Multiple control owners; refusing to choose'}
 if($existing.Count){
  if($SmokeTest -or $existing[0].ExecutablePath -ne $core){throw 'Another Core owns this endpoint. Save and close its UI, then stop that Core explicitly. No process was stopped.'}
  if($existing[0].CommandLine -notmatch '--stage2-input(\s|$)'){throw 'Existing Core is not a production MUSIC/CINEMA session'}
  if($AuthorizedLiveTest -and $existing[0].CommandLine -notmatch '--authorized-live-test(\s|$)'){throw 'Existing Core needs an explicit restart for live-test authorization'}
  $process=Get-Process -Id $existing[0].ProcessId;Write-Host "Attach to existing package Core PID $($process.Id); state retained."
 }
 if($Profile){$Profile=(Resolve-Path -LiteralPath $Profile).Path;if($Profile.Contains('"')){throw 'Invalid profile path'}}
 $env:PATH=$runtime+';'+$env:PATH;$env:QT_PLUGIN_PATH=$runtime;$env:QT_QPA_PLATFORM_PLUGIN_PATH=Join-Path $runtime 'platforms';$env:QT_QPA_PLATFORM=if($SmokeTest){'offscreen'}else{'windows'}
 $logs=Join-Path $root ('logs/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $logs -Force|Out-Null
 if(!$process){
  $args=@('--control-server','--endpoint',$Endpoint)
  if($SmokeTest){$args+='--offline-test-signal'}else{$args+='--stage2-input'}
  if($AuthorizedLiveTest){$args+='--authorized-live-test'}
  if($Profile){$args+=@('--profile',('"'+$Profile+'"'))}
  $log=Join-Path $logs 'core.txt';$process=Start-Process -FilePath $core -ArgumentList $args -WindowStyle Hidden -PassThru -WorkingDirectory $root -RedirectStandardOutput $log -RedirectStandardError (Join-Path $logs 'core-errors.txt');$started=$true
  $until=[DateTime]::UtcNow.AddSeconds(15);$ready=$false
  do {if($process.HasExited){throw "Core exited before readiness; inspect $logs"};$text=Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue;$ready=$text -and $text.Contains("Stage05 native IPC READY: ${Endpoint};");if(!$ready){Start-Sleep -Milliseconds 100}}while(!$ready -and [DateTime]::UtcNow -lt $until)
  if(!$ready){throw "Readiness timeout; package Core PID $($process.Id) retained for explicit shutdown"}
 }
 $existingUi=@(Get-CimInstance Win32_Process -Filter "name='EntropyUI.exe'"|Where-Object {$_.ExecutablePath -eq $ui -and $_.CommandLine -match $pattern})
 if($existingUi.Count){Write-Host 'Package UI already attached.';exit 0}
 $uiArgs=@('--endpoint',$Endpoint);if($SmokeTest){$uiArgs+=@('--smoke-seconds','2','--capture-ui',('"'+(Join-Path $logs 'overview.png')+'"'))}
 $uiWindow=if($SmokeTest){'Hidden'}else{'Normal'}
 $uiProcess=Start-Process -FilePath $ui -ArgumentList $uiArgs -WindowStyle $uiWindow -PassThru -WorkingDirectory $root -RedirectStandardOutput (Join-Path $logs 'ui.txt') -RedirectStandardError (Join-Path $logs 'ui-errors.txt')
 $null=$uiProcess.Handle;$uiProcess.WaitForExit();if($uiProcess.ExitCode){throw "UI failed; inspect $logs"}
 if($SmokeTest){Write-Host 'PASS packaged Core/IPC/Qt smoke, DISARMED.'}else{Write-Host "UI closed; Core PID $($process.Id) retains its state. Explicit ARM only in UI."}
}catch{Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
finally{if($SmokeTest -and $started -and $process -and !$process.HasExited){Stop-Process -Id $process.Id}}
