param([ValidateRange(1,3600)][int]$Seconds=120,
      [Parameter(Mandatory=$true)][string]$Output,
      [string]$ControlEndpoint)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Preserve evidence: choose a fresh output file'}
$stage1Watch=[Diagnostics.Stopwatch]::StartNew()
$stage1Rows=@()
do {
    $stage1Snapshot=& (Join-Path $PSScriptRoot 'Get-Stage1Snapshot.ps1') -ControlEndpoint $ControlEndpoint | ConvertFrom-Json
    $stage1Row=[ordered]@{elapsedSeconds=$stage1Watch.Elapsed.TotalSeconds;snapshot=$stage1Snapshot}
    $stage1Row | ConvertTo-Json -Depth 8 -Compress | Add-Content -LiteralPath $Output -Encoding utf8
    $stage1Rows += [pscustomobject]$stage1Row
    Start-Sleep -Milliseconds 1000
}while($stage1Watch.Elapsed.TotalSeconds -lt $Seconds)
$stage1Summary=[ordered]@{elapsedSeconds=$stage1Watch.Elapsed.TotalSeconds;samples=$stage1Rows.Count;sessionUnchanged=(@($stage1Rows.snapshot.sessionId | Select-Object -Unique).Count -eq 1);sources=@()}
foreach($stage1Role in @('MUSIC','AUX')){
    $stage1Sources=@($stage1Rows | ForEach-Object {$_.snapshot.sources | Where-Object {$_.role -eq $stage1Role}})
    $stage1First=$stage1Sources[0];$stage1Last=$stage1Sources[-1]
    $stage1Summary.sources += [ordered]@{role=$stage1Role;providerId=$stage1Last.providerId;rate=$stage1Last.rate;validBits=$stage1Last.validBits;mode=$stage1Last.mode;generations=@($stage1Sources.generation | Select-Object -Unique);framesInDelta=($stage1Last.framesIn-$stage1First.framesIn);framesOutDelta=($stage1Last.framesOut-$stage1First.framesOut);lostFramesDelta=($stage1Last.lostFrames-$stage1First.lostFrames);overrunsDelta=($stage1Last.overruns-$stage1First.overruns);underrunsDelta=($stage1Last.underruns-$stage1First.underruns);faultsDelta=($stage1Last.faults-$stage1First.faults);nonzeroInputSideSamples=@($stage1Sources | Where-Object {$_.inputSideRms -gt 0}).Count;nonzeroOutputSideSamples=@($stage1Sources | Where-Object {$_.outputSideRms -gt 0}).Count}
}
$stage1Summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath ($Output+'.summary.json') -Encoding utf8
$stage1Summary | ConvertTo-Json -Depth 8
