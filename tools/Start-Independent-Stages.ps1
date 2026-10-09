param(
    [string]$Stage1Root='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 1 Sources/releases/stage1-passive-publish-v0002-20261009',
    [string]$Stage2Root='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 2 DSP/versions/v0008-passive-source',
    [string]$CoreRoot='D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release/0.1.0/EntropyAudioCore-0.1.0-beta-win-x64',
    [string]$Profile
)
$ErrorActionPreference='Stop'
try {
    $source=[IO.Path]::GetFullPath((Join-Path $Stage1Root 'Stage1/runtime/stage1/EntropySourceEngine.exe'))
    $dsp=[IO.Path]::GetFullPath((Join-Path $Stage2Root 'EntropyDspEngine.exe'))
    $coreLauncher=Join-Path $CoreRoot 'tools/Start-Beta.ps1'
    foreach($file in @($source,$dsp,$coreLauncher)){
        if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing release file: $file"}
    }
    # This is a startup convenience only. No process owns another process's lifetime.
    # Never terminate/reconfigure an existing source or infer readiness from silence.
    foreach($exe in @($source,$dsp)){
        $existing=@(Get-CimInstance Win32_Process -Filter "name='$([IO.Path]::GetFileName($exe))'")
        if($existing.Count -gt 1 -or ($existing.Count -and $existing[0].ExecutablePath -ne $exe)){
            throw "Another version is running: $([IO.Path]::GetFileName($exe)). Close it explicitly first."
        }
        if($existing.Count){Write-Host "Already running independently: $exe";continue}
        $check=Start-Process -FilePath $exe -ArgumentList '--verify-only' -WindowStyle Hidden -PassThru -Wait
        if($check.ExitCode -ne 0){throw "Package verification failed: $exe"}
        $runtime=Split-Path $exe
        Start-Process -FilePath $exe -ArgumentList @('--runtime-root',('"'+$runtime+'"')) -WorkingDirectory $runtime | Out-Null
        Write-Host "Started independently: $exe"
    }
    Write-Host 'Stage1 -> passive FP64 -> Stage2 -> passive FP64 -> Core. New Core starts DISARMED.'
    $coreArguments=@{AuthorizedLiveTest=$true}
    if($Profile){$coreArguments.Profile=$Profile}
    & $coreLauncher @coreArguments
    exit $LASTEXITCODE
}catch{Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
