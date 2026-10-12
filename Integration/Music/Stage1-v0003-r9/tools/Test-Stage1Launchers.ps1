param([string]$RuntimeRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'release\stage1-native-music-v0003-r3'))
$ErrorActionPreference = 'Stop'
# Exercises the real CMD -> Windows PowerShell -> packaged native Core lifecycle.
# Catches duplicate starts, unsafe PID reuse, cold-start profile loss, and missing graceful stop.
$testRoot = Join-Path (Split-Path $PSScriptRoot -Parent) "evidence\launcher test $PID"
$state = Join-Path $testRoot 'state with spaces'
New-Item -ItemType Directory -Path $state -Force | Out-Null
$config = Join-Path $state 'Stage1Music.ini'
@('[music]', 'providerId=', 'endpointId=', 'mode=8', '[aux]', 'providerId=', 'endpointId=', 'mode=4', '; preserved by cold start') | Set-Content -LiteralPath $config -Encoding ascii
$before = (Get-FileHash -LiteralPath $config).Hash
$pipe = "\\.\pipe\Entropy.Stage1.LauncherTest.$PID"
$env:STAGE1_NO_PAUSE = '1'
function Invoke-Launcher([string]$name,[switch]$AllowFailure) {
    $file = Join-Path $RuntimeRoot $name
    if (!(Test-Path -LiteralPath $file)) { throw "Missing double-click launcher: $name" }
    # Wait for the CMD process itself. Native pipeline redirection can wait
    # indefinitely for inherited pipe handles retained by its background Core.
    $logId=[guid]::NewGuid().ToString('N')
    $arguments='/d /c ""'+$file+'" -StateDirectory "'+$state+'" -PublishEndpoint "'+$pipe+'" -NoUI"'
    $launcher=Start-Process -FilePath $env:ComSpec -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$testRoot/$logId-out.txt" -RedirectStandardError "$testRoot/$logId-error.txt"
    if(!$launcher.WaitForExit(15000)){throw "$name did not finish within 15 seconds"}
    if($AllowFailure){return $launcher.ExitCode}
    if($launcher.ExitCode){throw "$name failed: $($launcher.ExitCode)"}
}
try {
    Invoke-Launcher 'Start-Stage1-Music.cmd'
    $first = Get-Content -LiteralPath "$state\core-owner.json" -Raw | ConvertFrom-Json
    Invoke-Launcher 'Start-Stage1-Music.cmd'
    $again = Get-Content -LiteralPath "$state\core-owner.json" -Raw | ConvertFrom-Json
    if ($first.pid -ne $again.pid -or $first.startTicks -ne $again.startTicks) { throw 'Normal start duplicated the Core' }
    # A reused PID with different creation time must never be treated as our Core.
    $invalid = $again | Select-Object *
    $invalid.startTicks = '0'
    $invalid | ConvertTo-Json | Set-Content -LiteralPath "$state\core-owner.json" -Encoding utf8
    $invalidExit=Invoke-Launcher 'Stop-Stage1-Music.cmd' -AllowFailure
    if (!$invalidExit -or !(Get-Process -Id $first.pid -ErrorAction SilentlyContinue) -or (Test-Path -LiteralPath "$state\core.stop")) { throw 'Invalid identity was allowed to stop a process' }
    $again | ConvertTo-Json | Set-Content -LiteralPath "$state\core-owner.json" -Encoding utf8
    Invoke-Launcher 'ColdStart-Stage1-Music.cmd'
    $cold = Get-Content -LiteralPath "$state\core-owner.json" -Raw | ConvertFrom-Json
    if ($cold.pid -eq $first.pid -and $cold.startTicks -eq $first.startTicks) { throw 'Cold start reused the old Core' }
    $firstDiagnostic = Get-Content -LiteralPath "$state\diagnostic.json" -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $config).Hash -ne $before) { throw 'Cold start changed the profile' }
    Invoke-Launcher 'Stop-Stage1-Music.cmd'
    if (Get-Process -Id $cold.pid -ErrorAction SilentlyContinue) { throw 'Stop returned with the owned Core still running' }
    $lastDiagnostic = Get-Content -LiteralPath "$state\diagnostic.json" -Raw | ConvertFrom-Json
    [ordered]@{ result = 'PASS'; firstPid = $first.pid; repeatedPid = $again.pid; coldPid = $cold.pid; profilePreserved = $true; invalidIdentityRefused = $true; firstDiagnostic = $firstDiagnostic; lastDiagnostic = $lastDiagnostic } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$testRoot\result.json" -Encoding utf8
    Write-Output "PASS packaged CMD lifecycle; duplicate start reused Core, cold restart preserved profile, invalid identity refused, graceful stop. Evidence: $testRoot\result.json"
} finally {
    if (Test-Path -LiteralPath "$state\core-owner.json") {
        & "$RuntimeRoot\Stage1-Launch.ps1" -Action Stop -StateDirectory $state -NoUI
    }
    Remove-Item Env:STAGE1_NO_PAUSE -ErrorAction SilentlyContinue
}
