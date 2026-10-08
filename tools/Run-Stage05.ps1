param(
    [string]$Endpoint='EntropyAudioCore.Control.v1',
    [switch]$OfflineTestSignal,
    [switch]$SmokeTest
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$core=Join-Path $root 'build/msvc-stage05/EntropyAudioCore.exe'
$ui=Join-Path $root 'build/qt-stage05/EntropyUI.exe'
$qt='C:/Qt/6.11.2/mingw_64'
$coreProcess=$null
$uiProcess=$null
try {
    foreach($required in @($core,$ui,"$qt/bin/Qt6Network.dll","$qt/plugins/platforms/qwindows.dll")) {
        if(!(Test-Path -LiteralPath $required -PathType Leaf)){throw "Missing dependency: $required"}
    }
    if($SmokeTest -and !$PSBoundParameters.ContainsKey('Endpoint')) {
        $Endpoint='EntropyAudioCore.ColdStart.'+[Guid]::NewGuid().ToString('N')
    }
    if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){throw 'Invalid IPC endpoint'}
    $env:PATH="$qt/bin;C:/Qt/Tools/mingw1310_64/bin;"+$env:PATH
    $env:QT_PLUGIN_PATH="$qt/plugins"
    $env:QT_QPA_PLATFORM_PLUGIN_PATH="$qt/plugins/platforms"
    $env:QT_QPA_PLATFORM='windows'
    if($SmokeTest){$env:QT_QPA_PLATFORM='offscreen'}
    $logs=Join-Path $root ('local/stage05/cold-start/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $logs -Force | Out-Null
    Write-Host "Cold start: AudioCore -> IPC -> Qt; DISARMED; hardware BLOCKED."
    Write-Host "Logs: $logs"
    $arguments=@('--control-server','--endpoint',$Endpoint)
    if($OfflineTestSignal){$arguments+='--offline-test-signal'}
    $coreLog=Join-Path $logs 'core.txt'
    $coreProcess=Start-Process -FilePath $core -ArgumentList $arguments -PassThru -WindowStyle Hidden -WorkingDirectory $root -RedirectStandardOutput $coreLog -RedirectStandardError (Join-Path $logs 'core-errors.txt')
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    do {
        if($coreProcess.HasExited){throw "AudioCore exited before IPC readiness. See $logs"}
        $coreOutput=Get-Content -LiteralPath $coreLog -Raw -ErrorAction SilentlyContinue
        $ready=$null -ne $coreOutput -and $coreOutput.Contains("Stage05 native IPC READY: ${Endpoint};")
        if(!$ready){Start-Sleep -Milliseconds 100}
    } while(!$ready -and [DateTime]::UtcNow -lt $deadline)
    if(!$ready){throw "AudioCore IPC readiness timed out. See $logs"}
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
    }
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
} finally {
    if($uiProcess -and !$uiProcess.HasExited){Stop-Process -Id $uiProcess.Id -ErrorAction SilentlyContinue}
    if($coreProcess -and !$coreProcess.HasExited){Stop-Process -Id $coreProcess.Id -ErrorAction SilentlyContinue}
}
