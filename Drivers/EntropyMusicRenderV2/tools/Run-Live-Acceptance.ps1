param([switch]$RunLive,[switch]$Stress,[ValidateRange(1,1800)][int]$Seconds=10,[string]$Output)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $root -Parent) -Parent
$audit=Join-Path $root 'build/model/EntropyMusicPcmAudit.exe'
$provider=Join-Path $root 'build/model/EntropyMusicRawProviderV2Audit.dll'
$frozen=Join-Path $repo 'Integration/Music/Stage1/release/stage1-native-music-v0002/core/EntropyMusicSourceCore.exe'
if(!$RunLive){Write-Output 'No live action. After approved installation: -RunLive for all24 Exclusive cases; -RunLive -Stress for1800s PCM24/192 event. Never uses existing endpoints or DAC.';return}
if(!$Output){$Output=Join-Path $root ('build/live/'+[guid]::NewGuid().ToString('N'))}
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory; previous evidence is not overwritten'}
foreach($file in @($audit,$provider,$frozen)){if(!(Test-Path -LiteralPath $file)){throw "Required artifact unavailable: $file"}}
$identityJson=& $audit --action identify
if($LASTEXITCODE){throw 'Exact new driver identity not present; no live action'}
$identity=$identityJson | ConvertFrom-Json
New-Item -ItemType Directory -Path $Output | Out-Null
$core=Join-Path $Output 'EntropyMusicSourceCore.exe'
Copy-Item -LiteralPath $frozen -Destination $core
$providers=Join-Path $Output 'providers';New-Item -ItemType Directory -Path $providers | Out-Null
Copy-Item -LiteralPath $provider -Destination $providers
$hashes=@{coreSha256=(Get-FileHash -LiteralPath $core).Hash;frozenCoreSha256=(Get-FileHash -LiteralPath $frozen).Hash;providerSha256=(Get-FileHash -LiteralPath $provider).Hash;auditSha256=(Get-FileHash -LiteralPath $audit).Hash;identity=$identity;acceptance='pending; no model result promoted to kernel acceptance'}
if($hashes.coreSha256 -ne $hashes.frozenCoreSha256){throw 'Frozen Core copy differs'}
$hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Output 'provenance.json')
$cases=if($Stress){@(@{rate=192000;bits=32;servicing='event';seconds=1800})}else{@(foreach($rate in @(44100,48000,88200,96000,176400,192000)){foreach($bits in @(16,32)){foreach($mode in @('event','polling')){@{rate=$rate;bits=$bits;servicing=$mode;seconds=$Seconds}}}})}
$results=@()
foreach($case in $cases){
    $session=[guid]::NewGuid().ToString('N');$folder=Join-Path $Output "$($case.rate)-$($case.bits)-$($case.servicing)";New-Item -ItemType Directory -Path $folder | Out-Null
    $capture=Join-Path $folder 'core-raw.bin';$config=Join-Path $folder 'Stage1Audit.ini';$stop=Join-Path $folder 'stop.flag';$render=Join-Path $folder 'render.json';$report=Join-Path $folder 'comparison.json'
    @"
[music]
providerId=$($identity.providerId)
endpointId=$($identity.endpointId)
mode=8
[aux]
providerId=
endpointId=
mode=8
"@ | Set-Content -LiteralPath $config -Encoding utf8
    $collector=$null;$coreProcess=$null;$previousPipe=$env:ENTROPY_MUSIC_AUDIT_PIPE
    try{
        $collector=Start-Process -FilePath $audit -ArgumentList @('--action','collect','--session',$session,'--capture',('"'+$capture+'"')) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $folder 'collector.log') -RedirectStandardError (Join-Path $folder 'collector-error.log')
        $deadline=[datetime]::UtcNow.AddSeconds(10);$listening=$false
        while([datetime]::UtcNow -lt $deadline){if((Test-Path -LiteralPath (Join-Path $folder 'collector.log')) -and ((Get-Content -LiteralPath (Join-Path $folder 'collector.log') -Raw) -match 'Audit collector ready')){$listening=$true;break};if($collector.HasExited){break};Start-Sleep -Milliseconds 100}
        if(!$listening){throw 'Collector did not create its pipe before Core startup'}
        $env:ENTROPY_MUSIC_AUDIT_PIPE="\\.\pipe\EntropyMusicAudit-$session"
        $coreProcess=Start-Process -FilePath $core -ArgumentList @('--config',('"'+$config+'"'),'--provider-directory',('"'+$providers+'"'),'--publish-endpoint',"Entropy.Stage1.Audit.$session",'--control-endpoint',"Entropy.Stage1.Audit.$session.Control",'--stop-file',('"'+$stop+'"'),'--diagnostic-file',('"'+(Join-Path $folder 'core-final.json')+'"')) -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $folder 'core.log') -RedirectStandardError (Join-Path $folder 'core-error.log')
        $env:ENTROPY_MUSIC_AUDIT_PIPE=$previousPipe
        $deadline=[datetime]::UtcNow.AddSeconds(15);$ready=$false
        while([datetime]::UtcNow -lt $deadline){& $audit --action ready --session $session; if(!$LASTEXITCODE){$ready=$true;break};if($coreProcess.HasExited -or $collector.HasExited){break};Start-Sleep -Milliseconds 100}
        if(!$ready){throw 'Isolated frozen Core audit connection not ready'}
        & $audit --action render --session $session --rate $case.rate --bits $case.bits --servicing $case.servicing --seconds $case.seconds > $render 2> (Join-Path $folder 'render-error.log')
        $renderExit=$LASTEXITCODE
        Set-Content -LiteralPath $stop -Value 'qualification case finished'
        if(!$coreProcess.WaitForExit(10000)){throw 'Isolated Core did not stop after its own stop-file'}
        if(!$collector.WaitForExit(10000)){throw 'Audit footer/drain timed out'}
        $client=Get-Content -LiteralPath ($capture+'.client.json') -Raw | ConvertFrom-Json
        if($client.providerClientPid -ne $coreProcess.Id){throw 'Capture did not originate in the exact frozen Core process'}
        if($renderExit){throw 'Live render/drain failed; preserve all evidence'}
        $rendered=Get-Content -LiteralPath $render -Raw | ConvertFrom-Json
        & $audit --action verify --capture $capture --frames $rendered.emittedFrames --drained ($rendered.drained.ToString().ToLowerInvariant()) > $report
        if($LASTEXITCODE){throw 'Raw PCM comparison failed'}
        $results+=@{case=$case;pass=$true;report=$report;frames=$rendered.emittedFrames;observedAllocation=$rendered.observedAllocation}
    }catch{$results+=@{case=$case;pass=$false;reason=$_.Exception.Message};throw}
    finally{
        $env:ENTROPY_MUSIC_AUDIT_PIPE=$previousPipe
        if($coreProcess -and !$coreProcess.HasExited){Set-Content -LiteralPath $stop -Value 'cancel isolated qualification';if(!$coreProcess.WaitForExit(10000)){Stop-Process -Id $coreProcess.Id}}
        if($collector -and !$collector.HasExited){Stop-Process -Id $collector.Id}
        $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Output 'results.json')
    }
}
Write-Output "Evidence: $Output; this covers only the recorded formats, servicing, duration and transitions. TIDAL/PAUSE/EOS qualification remains separate."
