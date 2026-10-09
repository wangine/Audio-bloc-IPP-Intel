param(
    [string]$Endpoint='EntropyAudioCore.Control.v1',
    [string]$Profile,
    [switch]$AuthorizedLiveTest,
    [switch]$SmokeTest
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runtime=Join-Path $root 'runtime'
$core=Join-Path $runtime 'EntropyAudioCore.exe'
$ui=Join-Path $runtime 'EntropyUI.exe'
$startedCore=$false;$ready=$false;$coreProcess=$null;$uiProcess=$null
try {
    if($SmokeTest -and $AuthorizedLiveTest){throw 'Offline smoke cannot authorize physical output'}
    if($SmokeTest){$Endpoint='EntropyAudioCore.BetaSmoke.'+[Guid]::NewGuid().ToString('N')}
    if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){throw 'Invalid IPC endpoint'}
    foreach($file in @($core,$ui,(Join-Path $runtime 'Qt6Core.dll'),(Join-Path $runtime 'platforms/qwindows.dll'))){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing runtime file: $file"}}
    $manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw | ConvertFrom-Json
    Write-Host "Entropy AudioCore $($manifest.version) / $($manifest.channel); source $($manifest.sourceCommit)"
    Write-Host "Core executable: $core"
    Write-Host "Manual LIVE test session: $AuthorizedLiveTest; new Core starts DISARMED."
    if($Profile){$Profile=(Resolve-Path -LiteralPath $Profile).Path;if($Profile.Contains('"')){throw 'Invalid profile path'}}
    $env:PATH=$runtime+';'+$env:PATH
    $env:QT_PLUGIN_PATH=$runtime
    $env:QT_QPA_PLATFORM_PLUGIN_PATH=Join-Path $runtime 'platforms'
    $env:QT_QPA_PLATFORM=if($SmokeTest){'offscreen'}else{'windows'}
    $logs=Join-Path $root ('logs/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $logs -Force | Out-Null
    $arguments=@('--control-server','--endpoint',$Endpoint)
    if($SmokeTest){$arguments+='--offline-test-signal'}else{$arguments+='--stage2-input'}
    if($AuthorizedLiveTest){$arguments+='--authorized-live-test'}
    if($Profile){$arguments+=@('--profile',('"'+$Profile+'"'))}
    $pattern='--endpoint\s+"?'+[regex]::Escape($Endpoint)+'"?(\s|$)'
    $existing=@(Get-CimInstance Win32_Process -Filter "name='EntropyAudioCore.exe'" | Where-Object {$_.CommandLine -match $pattern})
    if($existing.Count -gt 1){throw 'Multiple Core owners; refusing to choose'}
    if($existing.Count){
        if($SmokeTest -or $existing[0].ExecutablePath -ne $core -or $existing[0].CommandLine -notmatch '--stage2-input(\s|$)'){throw 'Another Core owns this endpoint. Save its profile and close Qt, then stop that Core before launching this beta.'}
        if($AuthorizedLiveTest -and $existing[0].CommandLine -notmatch '--authorized-live-test(\s|$)'){throw 'Existing Core does not have manual test authorization; restart it explicitly after saving the profile.'}
        $coreProcess=Get-Process -Id $existing[0].ProcessId;$ready=$true
        Write-Host "Reconnecting to Core PID $($coreProcess.Id); current engine state retained."
    }else{
        $coreLog=Join-Path $logs 'core.txt'
        $coreProcess=Start-Process -FilePath $core -ArgumentList $arguments -WindowStyle Hidden -PassThru -WorkingDirectory $root -RedirectStandardOutput $coreLog -RedirectStandardError (Join-Path $logs 'core-errors.txt')
        $startedCore=$true;$deadline=[DateTime]::UtcNow.AddSeconds(15)
        do {
            if($coreProcess.HasExited){throw "Core exited before readiness; logs: $logs"}
            $coreOutput=Get-Content -LiteralPath $coreLog -Raw -ErrorAction SilentlyContinue
            $ready=$null -ne $coreOutput -and $coreOutput.Contains("Stage05 native IPC READY: ${Endpoint};")
            if(!$ready){Start-Sleep -Milliseconds 100}
        }while(!$ready -and [DateTime]::UtcNow -lt $deadline)
        if(!$ready){throw "Core readiness timeout; logs: $logs"}
        Write-Host "Core PID $($coreProcess.Id); new engine DISARMED. Offline Stage04 qualification remains FAIL."
    }
    $existingUi=@(Get-CimInstance Win32_Process -Filter "name='EntropyUI.exe'" | Where-Object {$_.ExecutablePath -eq $ui -and $_.CommandLine -match $pattern})
    if($existingUi.Count){Write-Host 'UI already connected.';exit 0}
    $uiArguments=@('--endpoint',$Endpoint)
    if($SmokeTest){$uiArguments+=@('--smoke-seconds','2')}
    $uiLog=Join-Path $logs 'ui.txt'
    $uiProcess=Start-Process -FilePath $ui -ArgumentList $uiArguments -PassThru -WorkingDirectory $root -RedirectStandardOutput $uiLog -RedirectStandardError (Join-Path $logs 'ui-errors.txt')
    $null=$uiProcess.Handle;$uiProcess.WaitForExit()
    if($uiProcess.ExitCode -ne 0){throw "UI failed; logs: $logs"}
    if($SmokeTest){
        if(!([string](Get-Content -LiteralPath $uiLog -Raw)).Contains('EntropyUI executable IPC smoke PASS')){throw 'Packaged UI did not confirm smoke PASS'}
        Write-Host 'PASS packaged Core/UI IPC smoke, isolated offline endpoint; no source or DAC opened.'
    }else{Write-Host "UI closed; Core PID $($coreProcess.Id) retains its engine state. Logs: $logs"}
}catch{Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
finally{
    if($uiProcess -and !$uiProcess.HasExited){Stop-Process -Id $uiProcess.Id}
    # A readiness timeout must not kill a live transport owner. The Core may
    # already hold a lease; preserve it for reconnect or explicit IPC shutdown.
    if($startedCore -and $coreProcess -and !$coreProcess.HasExited){
        if($SmokeTest){Stop-Process -Id $coreProcess.Id}
        elseif(!$ready){Write-Host "Core PID $($coreProcess.Id) retained after startup timeout. Use Stop-AudioCore.cmd for safe IPC shutdown."}
    }
}
