function Assert-SourceContinuity([string]$Folder) {
    $final=Get-Content -LiteralPath (Join-Path $Folder 'core-final.json') -Raw | ConvertFrom-Json
    foreach($counter in @('lost_frames','overruns','faults')) {
        if($final.sources[0].$counter -ne 0){throw "Source counter is not zero: $counter"}
    }
    if($final.publisher_drops -ne 0 -or $final.clock_late_blocks -ne 0){throw 'Publication counters are not zero'}
    $reads=@(Import-Csv -LiteralPath (Join-Path $Folder 'timing/music-read.csv') | Where-Object rc -eq '1')
    $ticks=@(Import-Csv -LiteralPath (Join-Path $Folder 'timing/ticks.csv'))
    if(!$reads.Count -or !$ticks.Count -or !$ticks[0].PSObject.Properties['music_underruns']){throw 'Continuity trace missing'}
    $render=Get-Content -LiteralPath (Join-Path $Folder 'render.json') -Raw | ConvertFrom-Json
    $raw=Get-Content -LiteralPath (Join-Path $Folder 'comparison.json') -Raw | ConvertFrom-Json
    if(!$raw.pass -or $raw.mismatchedBytes -ne 0 -or $raw.lostFrames -ne 0 -or !$render.drained -or $raw.frames -ne $render.emittedFrames -or $final.sources[0].generation -ne $render.generation){throw 'Verified PCM identity does not match final source'}
    [uint64]$expectedFrame=0
    foreach($read in $reads){
        if([uint64]$read.first -ne $expectedFrame -or [uint32]$read.frames -eq 0){throw 'Ingress trace is not complete and contiguous'}
        $expectedFrame+=[uint32]$read.frames
    }
    if($expectedFrame -ne $raw.frames -or $expectedFrame -ne $final.sources[0].frames_in -or [uint64]$reads.Count -ne $final.sources[0].packets){throw 'Ingress trace does not cover the verified complete PCM sequence'}
    $lastPacket=[int64]$reads[-1].end
    if([int64]$ticks[0].qpc -gt [int64]$reads[0].end -or [int64]$ticks[-1].qpc -lt $lastPacket){throw 'Trace truncated before last PCM packet'}
    if([uint64]$ticks.Count -ne $final.blocks_published -or [uint64]$ticks[-1].music_underruns -ne $final.sources[0].underruns){throw 'Scheduler trace does not cover the final publication and counter state'}
    $interior=@($ticks | Where-Object {[int64]$_.qpc -le $lastPacket -and [uint64]$_.music_underruns -ne 0})
    if($interior.Count){throw 'Source underrun occurred before the last PCM packet, including startup/pause/RUN'}
    return [ordered]@{underrunsBeforeLastPacket=0;underrunsTotal=$final.sources[0].underruns;verifiedFrames=$expectedFrame;verifiedPackets=$reads.Count;generation=$render.generation;lastPacketQpc=$lastPacket;scope='Includes startup, pause and resumed RUN through the complete independently verified PCM sequence; later RUN-without-PCM counters remain visible'}
}
