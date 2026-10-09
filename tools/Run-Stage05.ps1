param(
    [string]$Endpoint='EntropyAudioCore.Control.v1',
    [switch]$OfflineTestSignal,
    [switch]$SmokeTest,
    [switch]$Stage2Input,
    [switch]$AuthorizedLiveTest,
    [string]$Profile,
    [string]$LiveGate='local/stage07/live-offline-pass.gate',
    [string]$NativeBuild='build/msvc-stage07',
    [string]$UiBuild='build/qt-stage07'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$core=Join-Path $root ($NativeBuild+'/EntropyAudioCore.exe')
$ui=Join-Path $root ($UiBuild+'/EntropyUI.exe')
$qt='C:/Qt/6.11.2/mingw_64'
$coreProcess=$null
$uiProcess=$null
$startedCore=$false
$ready=$false
try {
    if($SmokeTest -or $OfflineTestSignal){$Stage2Input=$false}
    if($AuthorizedLiveTest -and !$Stage2Input){throw 'Authorized live test requires the actual FP64 input mode'}
    foreach($required in @($core,$ui,"$qt/bin/Qt6Network.dll","$qt/plugins/platforms/qwindows.dll")) {
        if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Missing dependency: $required"}
    }
    $core=(Resolve-Path -LiteralPath $core).Path
    $ui=(Resolve-Path -LiteralPath $ui).Path
    if($SmokeTest -and !$PSBoundParameters.ContainsKey('Endpoint')) {
        $Endpoint='EntropyAudioCore.ColdStart.'+[Guid]::NewGuid().ToString('N')
    }
    if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){throw 'Invalid IPC endpoint'}
    if($Profile){$Profile=(Resolve-Path -LiteralPath $Profile).Path;if($Profile.Contains('"')){throw 'Invalid profile path'}}
    $env:PATH="$qt/bin;C:/Qt/Tools/mingw1310_64/bin;"+$env:PATH
    $env:QT_PLUGIN_PATH="$qt/plugins"
    $env:QT_QPA_PLATFORM_PLUGIN_PATH="$qt/plugins/platforms"
    $env:QT_QPA_PLATFORM='windows'
    if($SmokeTest){$env:QT_QPA_PLATFORM='offscreen'}
    $logs=Join-Path $root ('local/stage07/cold-start/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $logs -Force | Out-Null
    Write-Host "Cold start: AudioCore -> IPC -> Qt; new Core starts DISARMED; explicit ARM requires session eligibility and a valid profile."
    Write-Host "Logs: $logs"
    $arguments=@('--control-server','--endpoint',$Endpoint)
    if($OfflineTestSignal){$arguments+='--offline-test-signal'}
    if($Stage2Input){$arguments+='--stage2-input';$gate=Join-Path $root $LiveGate;if(Test-Path -LiteralPath $gate -PathType Leaf){$arguments+=@('--live-gate',('"'+$gate+'"'))}}
    if($AuthorizedLiveTest){$arguments+='--authorized-live-test';Write-Host 'Explicit user-authorized live test session; offline FAIL results remain unchanged. Startup remains DISARMED.'}
    if($Profile){$arguments+=@('--profile',('"'+$Profile+'"'))}
    $coreLog=Join-Path $logs 'core.txt'
    $endpointPattern='--endpoint\s+"?'+[regex]::Escape($Endpoint)+'"?(\s|$)'
    $existing=@(Get-CimInstance Win32_Process -Filter "name='EntropyAudioCore.exe'" | Where-Object {$_.CommandLine -match $endpointPattern})
    if($existing.Count -gt 1){throw 'Multiple Core instances found; refusing to choose an owner'}
    if($existing.Count -eq 1){
        $existingReal=$existing[0].CommandLine -match '--stage2-input(\s|$)'
        $existingSignal=$existing[0].CommandLine -match '--offline-test-signal(\s|$)'
        if($AuthorizedLiveTest -and $existing[0].CommandLine -notmatch '--authorized-live-test(\s|$)'){throw 'Existing Core was not started for authorized live testing; save its state and restart explicitly in test mode.'}
        if($existing[0].ExecutablePath -ne $core -or [bool]$Stage2Input -ne $existingReal -or [bool]$OfflineTestSignal -ne $existingSignal){throw 'A different Core or input mode owns this endpoint. Use a separate endpoint; save the project before replacing a previous Core.'}
        $coreProcess=Get-Process -Id $existing[0].ProcessId
        $ready=$true
        Write-Host "Reconnecting to Core PID $($coreProcess.Id); last engine state retained."
        if($Profile){Write-Host 'Existing Core retains its state; startup profile applies only to a newly started Core.'}
    }else{
        $coreProcess=Start-Process -FilePath $core -ArgumentList $arguments -PassThru -WindowStyle Hidden -WorkingDirectory $root -RedirectStandardOutput $coreLog -RedirectStandardError (Join-Path $logs 'core-errors.txt')
        $startedCore=$true
        $deadline=[DateTime]::UtcNow.AddSeconds(15)
        do {
            if($coreProcess.HasExited){throw "AudioCore exited before IPC readiness. See $logs"}
            $coreOutput=Get-Content -LiteralPath $coreLog -Raw -ErrorAction SilentlyContinue
            $ready=$null -ne $coreOutput -and $coreOutput.Contains("Stage05 native IPC READY: ${Endpoint};")
            if(!$ready){Start-Sleep -Milliseconds 100}
        } while(!$ready -and [DateTime]::UtcNow -lt $deadline)
        if(!$ready){throw "AudioCore IPC readiness timed out. See $logs"}
        Write-Host "Core PID $($coreProcess.Id) owns the engine independently of Qt."
    }
    $existingUi=@(Get-CimInstance Win32_Process -Filter "name='EntropyUI.exe'" | Where-Object {$_.ExecutablePath -eq $ui -and $_.CommandLine -match $endpointPattern})
    if($existingUi.Count){if($SmokeTest){throw 'Smoke test cannot reuse an already open UI'};Write-Host 'Qt is already open for this Core.';exit 0}
    $uiArguments=@('--endpoint',$Endpoint)
    if($SmokeTest){$uiArguments+=@('--smoke-seconds','2')}
    $uiProcess=Start-Process -FilePath $ui -ArgumentList $uiArguments -PassThru -WorkingDirectory $root -RedirectStandardOutput (Join-Path $logs 'ui.txt') -RedirectStandardError (Join-Path $logs 'ui-errors.txt')
    # Retain the process handle so Windows PowerShell can read ExitCode after exit.
    $null=$uiProcess.Handle
    $uiProcess.WaitForExit()
    if($uiProcess.ExitCode -ne 0){throw "EntropyUI failed (exit $($uiProcess.ExitCode)). See $logs"}
    if($SmokeTest){
        $output=[string](Get-Content -LiteralPath (Join-Path $logs 'ui.txt') -Raw)
        if(!$output.Contains('EntropyUI executable IPC smoke PASS')){throw "IPC smoke did not confirm PASS. See $logs"}
        Write-Host $output.Trim()
    }else{
        Write-Host "Qt closed; AudioCore PID $($coreProcess.Id) remains running with its last applied state."
    }
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
} finally {
    if($uiProcess -and !$uiProcess.HasExited){Stop-Process -Id $uiProcess.Id -ErrorAction SilentlyContinue}
    if($startedCore -and $coreProcess -and !$coreProcess.HasExited -and ($SmokeTest -or !$ready)){Stop-Process -Id $coreProcess.Id -ErrorAction SilentlyContinue}
}
