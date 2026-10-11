param(
    [ValidateSet('Start', 'Cold', 'Stop', 'UI')][string]$Action = 'Start',
    [string]$StateDirectory = (Join-Path $PSScriptRoot 'state'),
    [string]$PublishEndpoint,
    [switch]$NoUI
)
$ErrorActionPreference = 'Stop'
$runtime = $PSScriptRoot
$state = [IO.Path]::GetFullPath($StateDirectory)
$coreExe = Join-Path $runtime 'core\EntropyMusicSourceCore.exe'
$uiExe = Join-Path $runtime 'ui\EntropyMusicSourceQt.exe'
$stopFile = Join-Path $state 'core.stop'
$coreRecord = Join-Path $state 'core-owner.json'
if (!$PublishEndpoint) {
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $PublishEndpoint = '\\.\pipe\Entropy.StagePublish.Music.Source.V2.' + $sid
}
if (!$PublishEndpoint.StartsWith('\\.\pipe\') -or $PublishEndpoint.Contains('"')) { throw 'Invalid local publish pipe' }
$controlEndpoint = $PublishEndpoint + '.Control.V1'
New-Item -ItemType Directory -Path $state -Force | Out-Null

function Get-OwnedProcess([string]$recordPath, [string]$exe) {
    if (!(Test-Path -LiteralPath $recordPath)) { return $null }
    $record = Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
    $process = Get-Process -Id $record.pid -ErrorAction SilentlyContinue
    if (!$process) { return $null }
    # A PID alone is insufficient: Windows can assign it to a different process.
    if ($process.Path -ne $exe -or [string]$process.StartTime.ToUniversalTime().Ticks -ne [string]$record.startTicks) {
        throw "Process identity changed; refusing to control PID $($record.pid). Record: $recordPath"
    }
    return $process
}
function Save-Owner($process, [string]$recordPath) {
    [ordered]@{ pid = $process.Id; startTicks = [string]$process.StartTime.ToUniversalTime().Ticks; executable = $process.Path; controlEndpoint = $controlEndpoint } |
        ConvertTo-Json | Set-Content -LiteralPath $recordPath -Encoding utf8
}
function Stop-OwnedCore {
    $process = Get-OwnedProcess $coreRecord $coreExe
    if (!$process) { Write-Output 'No running Core owned by this launcher.'; return }
    Set-Content -LiteralPath $stopFile -Value '1' -Encoding ascii
    if (!$process.WaitForExit(10000)) { throw 'Core did not finish graceful shutdown within 10 seconds; no forced termination was performed.' }
    Remove-Item -LiteralPath $coreRecord
    Write-Output "Stage1 MUSIC Core PID $($process.Id) stopped gracefully."
}
function Start-OwnedCore {
    $process = Get-OwnedProcess $coreRecord $coreExe
    if ($process) {
        $owner = Get-Content -LiteralPath $coreRecord -Raw | ConvertFrom-Json
        if ($owner.controlEndpoint -ne $controlEndpoint) { throw 'This state directory already belongs to a different control endpoint.' }
        Write-Output "Stage1 MUSIC Core already running: PID $($process.Id)."
        return
    }
    if (!(Test-Path -LiteralPath $coreExe)) { throw "Missing packaged Core: $coreExe" }
    $config = Join-Path $state 'Stage1Music.ini'
    if (!(Test-Path -LiteralPath $config)) {
        @('[music]', 'providerId=', 'endpointId=', 'mode=8', '[aux]', 'providerId=', 'endpointId=', 'mode=4') | Set-Content -LiteralPath $config -Encoding ascii
    }
    if (Test-Path -LiteralPath $stopFile) { Remove-Item -LiteralPath $stopFile }
    $arguments = @('--config', "`"$config`"", '--provider-directory', "`"$runtime\core\providers`"", '--publish-endpoint', "`"$PublishEndpoint`"", '--control-endpoint', "`"$controlEndpoint`"", '--stop-file', "`"$stopFile`"", '--diagnostic-file', "`"$state\diagnostic.json`"")
    $process = Start-Process -FilePath $coreExe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$state\core-stdout.txt" -RedirectStandardError "$state\core-stderr.txt"
    Save-Owner $process $coreRecord
    $ready = $false
    for ($attempt = 0; $attempt -lt 16; $attempt++) {
        $process.Refresh()
        if ($process.HasExited) {
            Get-Content -LiteralPath "$state\core-stderr.txt" -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $coreRecord
            throw "Core failed to start: exit $($process.ExitCode)"
        }
        $pipe = New-Object IO.Pipes.NamedPipeClientStream('.', $controlEndpoint.Substring(9), [IO.Pipes.PipeDirection]::InOut)
        try { $pipe.Connect(500); $ready = $true } catch [TimeoutException] {} finally { $pipe.Dispose() }
        if ($ready) { break }
    }
    if (!$ready) { throw "Core PID $($process.Id) is running but its control pipe is not ready. See state/core-stderr.txt; Stop-Stage1-Music.cmd requests graceful stop." }
    Write-Output "Stage1 MUSIC Core ready: PID $($process.Id)."
}
function Start-OwnedUI {
    if ($NoUI) { return }
    $record = Join-Path $state 'ui-owner.json'
    $process = Get-OwnedProcess $record $uiExe
    if ($process) { Write-Output "Stage1 MUSIC UI already open: PID $($process.Id)."; return }
    $process = Start-Process -FilePath $uiExe -ArgumentList @('--control-endpoint', "`"$controlEndpoint`"") -PassThru
    Save-Owner $process $record
    Write-Output "Stage1 MUSIC UI opened: PID $($process.Id). Closing the UI leaves Core running."
}
$lock = $null
try {
    # Serialize double clicks on this state directory; native Core also enforces pipe ownership.
    $lock = [IO.File]::Open((Join-Path $state 'launcher.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    switch ($Action) {
        'Start' { Start-OwnedCore; Start-OwnedUI }
        'Cold' { Stop-OwnedCore; Start-OwnedCore; Start-OwnedUI }
        'Stop' { Stop-OwnedCore }
        'UI' { Start-OwnedUI }
    }
} finally { if ($lock) { $lock.Dispose() } }
