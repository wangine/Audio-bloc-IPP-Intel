param([string]$LauncherScript=(Join-Path $PSScriptRoot 'Stage1-Launch.ps1'))
$ErrorActionPreference='Stop'
$stage1Root=Split-Path $PSScriptRoot -Parent
$stage1Test=Join-Path $stage1Root ('evidence/aux-cold-config-'+[Guid]::NewGuid().ToString('N'))
$stage1Runtime=Join-Path $stage1Test 'runtime'
$stage1Providers=Join-Path $stage1Runtime 'core/providers'
New-Item -ItemType Directory -Path (Join-Path $stage1Providers 'reference') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $stage1Root 'build/EntropyMusicSourceCore.exe') -Destination (Join-Path $stage1Runtime 'core')
foreach($stage1Library in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')){
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot ('System32/'+$stage1Library)) -Destination (Join-Path $stage1Runtime 'core')
}
Copy-Item -LiteralPath (Join-Path $stage1Root 'build/aux-fixture/AuxFixtureProvider.dll') -Destination $stage1Providers
Copy-Item -LiteralPath (Join-Path $stage1Root 'build/aux-fixture/reference/EntropyAuxEndpoint.dll') -Destination (Join-Path $stage1Providers 'reference')
Copy-Item -LiteralPath $LauncherScript -Destination (Join-Path $stage1Runtime 'Stage1-Launch.ps1')
$stage1Launcher=Join-Path $stage1Runtime 'Stage1-Launch.ps1'
$stage1State=Join-Path $stage1Test 'fresh-state'
$stage1Pipe='\\.\pipe\Entropy.Stage1.AuxColdConfig.'+[Guid]::NewGuid().ToString('N')
try{
    & $stage1Launcher -Action Start -StateDirectory $stage1State -PublishEndpoint $stage1Pipe -NoUI
    $stage1Config=Get-Content -LiteralPath (Join-Path $stage1State 'Stage1Music.ini') -Raw
    if($stage1Config -notmatch '(?s)\[aux\]\s+providerId=legacy\.aux\s+endpointId=entropy-aux:system-default\s+mode=4'){
        throw 'Cold config did not bind the fixed original AUX provider'
    }
    $stage1Snapshot=& (Join-Path $PSScriptRoot 'Get-Stage1Snapshot.ps1') -ControlEndpoint ($stage1Pipe+'.Control.V1') | ConvertFrom-Json
    $stage1Aux=$stage1Snapshot.sources | Where-Object {$_.role -eq 'AUX'}
    if($stage1Aux.providerId -ne 'legacy.aux' -or $stage1Aux.endpointId -ne 'entropy-aux:system-default' -or !$stage1Aux.packets){
        throw 'Cold launch failed to open the V1 AUX fixture automatically'
    }
    Write-Output 'PASS fresh state automatically binds AUX V1 without UI selection; synthetic module, no audio device accessed.'
}finally{
    & $stage1Launcher -Action Stop -StateDirectory $stage1State -PublishEndpoint $stage1Pipe -NoUI
}
