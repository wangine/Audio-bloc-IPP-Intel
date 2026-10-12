param([Parameter(Mandatory=$true)][string]$EvidenceCase)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Assert-Source-Continuity.ps1')
$result=Assert-SourceContinuity $EvidenceCase
if($result.underrunsTotal -ne 1){throw 'This recorded fixture must contain the known post-tail RUN underrun'}
$fixture=Join-Path (Split-Path $PSScriptRoot -Parent) ('evidence/continuity-validator-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'timing') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $EvidenceCase 'core-final.json') -Destination $fixture
foreach($file in @('render.json','comparison.json')){Copy-Item -LiteralPath (Join-Path $EvidenceCase $file) -Destination $fixture}
Copy-Item -LiteralPath (Join-Path $EvidenceCase 'timing/music-read.csv') -Destination (Join-Path $fixture 'timing')
$ticks=@(Import-Csv -LiteralPath (Join-Path $EvidenceCase 'timing/ticks.csv'))
$ticks[0].music_underruns='1'
$ticks | Export-Csv -LiteralPath (Join-Path $fixture 'timing/ticks.csv') -NoTypeInformation
$rejected=$false
try {$null=Assert-SourceContinuity $fixture} catch {$rejected=$_.Exception.Message -like 'Source underrun occurred*'}
if(!$rejected){throw 'Startup/interior underrun was concealed'}
$ticks=@(Import-Csv -LiteralPath (Join-Path $EvidenceCase 'timing/ticks.csv'))
$ticks[0] | Export-Csv -LiteralPath (Join-Path $fixture 'timing/ticks.csv') -NoTypeInformation
$rejected=$false
try {$null=Assert-SourceContinuity $fixture} catch {$rejected=$_.Exception.Message -eq 'Trace truncated before last PCM packet'}
if(!$rejected){throw 'Incomplete trace was falsely accepted'}
$reads=@(Import-Csv -LiteralPath (Join-Path $EvidenceCase 'timing/music-read.csv'))
$firstPcm=$reads | Where-Object rc -eq '1' | Select-Object -First 1
$reads | Where-Object {[int64]$_.end -le [int64]$firstPcm.end} | Export-Csv -LiteralPath (Join-Path $fixture 'timing/music-read.csv') -NoTypeInformation
Copy-Item -LiteralPath (Join-Path $EvidenceCase 'timing/ticks.csv') -Destination (Join-Path $fixture 'timing/ticks.csv') -Force
$rejected=$false
try {$null=Assert-SourceContinuity $fixture} catch {$rejected=$_.Exception.Message -eq 'Ingress trace does not cover the verified complete PCM sequence'}
if(!$rejected){throw 'Truncated ingress trace moved the boundary and concealed an underrun'}
$ticks | Where-Object {[int64]$_.qpc -le [int64]$firstPcm.end+100000} | Export-Csv -LiteralPath (Join-Path $fixture 'timing/ticks.csv') -NoTypeInformation
$rejected=$false
try {$null=Assert-SourceContinuity $fixture} catch {$rejected=$_.Exception.Message -eq 'Ingress trace does not cover the verified complete PCM sequence'}
if(!$rejected){throw 'Both truncated traces were falsely accepted'}
Write-Output 'PASS recorded post-tail RUN counter retained; startup/interior starvation, truncated ticks, truncated ingress and both-truncated traces rejected'
