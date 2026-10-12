# Cinema mute / DISARM investigation — 13 October 2026

Status: v0.2.21 with Stage 3 r3 is accepted as the local DTS reference for the
measured intervals. The user confirms correct playback. The three production
source waits were caused by the user's monitor switch, confirmed below.

The investigation used the user's real Cinema profile, the UR23 Exclusive
capture, DTS playback and both DACs. Crossover, output matrix, headroom,
speaker protection, SRC algorithms, clock calculations and buffer capacities
were preserved. Stage 1 v0003-r9 and Stage 2 v0010 were not changed.

## Demonstrated boundaries

1. Stage 3 startup reference selection included a hardware presentation lead
   in the source frame calculation. This could choose PCM not yet published.
   r3 selects the reference from committed source PCM while keeping the hardware
   presentation lead. The known-data startup regression failed with the original
   calculation and passed with r3. Six focused Stage 3 checks passed. r3 does
   not make source absence a DISARM condition or manufacture source blocks.
2. A Windows DATA_DISCONTINUITY warning was treated as an encoded stream
   boundary even when the retained carrier was continuous. v0.2.18 DTS had
   17,356 IEC headers, all 2,048 bytes apart; strict offline FFmpeg decoding
   succeeded across the warning. Replaying the identical carrier continuously
   versus resetting at the warning lost 1,024 decoded frames in the reset case
   (21.333 ms at 48 kHz), before source reacquisition. v0.2.19 reports the warning
   without resetting solely because of it. Codec changes, lost burst lock,
   decoder stall/exit and pipe failures retain their recovery paths.
3. v0.2.19 still had two unexpected source waits. Its PCM pacer was late
   78.6059 ms while 6,912 decoded frames remained queued. Publication had a
   72.1163 ms maximum gap. v0.2.20 registers this owner in MMCSS Pro Audio,
   CRITICAL. A fresh 420-second run reduced its maximum interval to 4.9286 ms;
   the 400-second DAC monitor had no source wait, DISARM, fault, xrun, ClockSync
   reset, replan or callback deadline miss.
4. v0.2.20 downstream publication still had two late intervals: 26.5053 and
   22.6678 ms, exceeding the unchanged 21.3333 ms target. The pacer submitted
   1,280 and 1,024 frames during those gaps, with maximum pacer intervals
   3.0832 and 3.0558 ms. Publication sequence was consecutive. Thus the
   remaining deadline failure lies after the timely pacer. v0.2.21 registers
   the downstream SRC/publication owner in the same MMCSS task.

| Trial | PCM pacer maximum gap | Publication maximum gap | Late publication intervals | Unexpected source waits |
|---|---:|---:|---:|---:|
| v0.2.19 | 78.6059 ms | 72.1163 ms | 41 | 2 |
| v0.2.20 | 4.9286 ms | 26.5053 ms | 2 | 0 |
| v0.2.21 | 4.9232 ms | 8.8166 ms | 0 | 0 during source RUN |

The 400-second v0.2.21 observer includes the programmatically scheduled end of
the 420-second decoder experiment. Its single wait occurs at Unix
1791845534.099830, after the final captured packet at 1791845533.930529. The
timeline anchor and both full and active summaries are retained. No samples or
telemetry are removed: the active 394.867-second shared observation interval
has zero waits, XRUNs, faults, resyncs, replans and callback deadline misses.
At test termination logical ARM remains true; the absent source causes mute.

The complete v0.2.21 carrier is 80,570,880 bytes / 41,964 packets, with one
Windows warning at 163.9693 seconds, zero source-frame/byte-offset gaps and
39,342 DTS headers at exactly 2,048-byte spacing. Strict offline FFmpeg decoding
exited zero. No recovery, underflow, discard, gate loss or publication drop
occurred, and the active generation stayed 3. Carrier SHA256:
`28ae4148ce94ba01db694722e887dd9f64c1367d92af2664fbfa3048ebd1022c`.

Full MSVC 19.51.36257.0 build succeeded. The three targeted checks — capture
policy, output transport and producer retirement — passed. The publishing-owner
guard/linkage review found no Critical or Important issue. An early test command
ran before its executables existed and is retained as Not Run; the checks above
are the completed, freshly built test run.

The exact original installer wrapper installed v0.2.21. All six installed
payload hashes match the verified package, registry DisplayVersion is 0.2.21,
and the Windows autostart value is unchanged. The original GUI started the
installed decoder, which started the original FFmpeg. The 400-second production
trial kept publication under 8.6162 ms (zero deadline excursions) and had no
DAC fault, ClockSync reset, replan or DISARM. However, three waits occurred at
01:59:34–01:59:41 local time, with changed source generations and FFmpeg PID.
The raw carrier for that specific interval was not recorded. The user then
confirmed: "Am facut switch la monitor si e normal sa se opreasca sunetul .
Dar se aude corect , da eu am facut switch". Thus the observed source transition
has a confirmed external intervention, rather than an unexplained continuous
playback failure. All three waits and their timestamps remain in the report;
they are not erased or presented as a zero-transition run. Stage 3 remained
logically armed and resumed automatically, with no DAC fault or DISARM.

An additional 451.260-second carrier trace using the identical original GUI
lifecycle has 45,126 packets, one Windows warning at 160.3793 seconds, zero
frame-index/byte-offset gaps, one active generation and no recovery after first
stream adoption. All 42,306 DTS headers are 2,048 bytes apart. Strict offline
FFmpeg decoding exits zero. Its 400-second DAC observation has zero source
waits, fault or DISARM; publication maximum is 8.5955 ms, with zero intervals
over 21.3333 ms. PCM pacer maximum is 3.7170 ms. Final GUI/decoder restoration
uses the installed production files, and both DACs are physically armed.

Latest GUI carrier SHA256:
`053d2cc27eaa892c6e7b250b90924c70e46ea7a1da4316cac3dd4e26e4cd708d`.

## Capture integrity and recovery evidence

The complete v0.2.20 carrier contains 80,661,120 bytes / 42,011 packets, one
Windows warning and zero source-frame index or byte-offset gaps. All 39,386
DTS headers have exact 2,048-byte spacing. GetBuffer and ReleaseBuffer have no
failed owned-buffer operations. Strict offline FFmpeg decoding exited zero.
There was no decoder restart, pacer underflow, gate loss, decoded discard or
publication drop. Generation stayed at 3 throughout the active stream.

Carrier SHA256:
`d57d2e21a4ebdee990c756e5f5d2ba34d34c066018235480b401ddd26dbf0a2f`

These observations qualify the retained carrier and measured intervals. They
do not certify identity to the film master or universal behavior for injected
corruption/loss. Warnings remain visible. Brief corrupt carrier which preserves
burst lock and decoded progress does not trigger reset solely on the Windows
warning; liveness is not an integrity certificate.

## Preserved limits

The current Cinema profile produces PCM overrange on DAC A and invokes its
existing clipping/reporting policy. No gain or filter compensation was applied
to hide it. This is separate from transport starvation and must not be described
as a clean amplitude qualification or proof that MUSIC clarity is corrected.

An earlier AC-3 trace had one startup priming underflow; continuous carrier was
valid afterwards. This DTS experiment does not qualify all AC-3 startup cases,
end-to-end acoustic latency, analog DAC alignment, or arbitrary system load.
Existing pinned DSP limitations remain.

## Local evidence

- `v18-dts-boundary-analysis.txt`, `dts-recovery-offline-control.json`
- `v19-owner-analysis.txt` (if generated), `v19-dts-decoder-trace.csv`, `v19-dts-timing.csv`
- `v20-owner-analysis.txt`, `v20-publication-check.txt`, `v20-publication-frontier.txt`
- `v20-carrier-analysis.txt`, `v20-carrier-offline-decode.log`, `v20-live-summary.json`
- `v21-dts-*`, `v21-active-live-summary.json`, `v21-trial-timeline.json`
- `installed-v21-*`, `autostart-before-v21.json`, `autostart-after-v21.json`
- `v21-installed-*`: production run, including the confirmed monitor switch
- `v21-gui-*`, `v21-gui-trace-*`: original GUI lifecycle with complete RAW trace
- `CheckNoUnnecessaryRecovery.py`, `CheckPublicationDeadline.py`,
  `AnalyzeOwnerTrace.py`, `AnalyzeCarrierBoundary.py`, `ExplainPublicationGaps.py`,
  `SummarizeLive.py`, trace generation/build tools

Original captures and telemetry remain in this diagnostic directory. Complete
build, targeted tests, installed-file verification, production live testing and
user listening acceptance passed for the stated DTS trials. Local Stage 1/2
paths and source references are preserved; the delivery index accompanies the
Git publication. This does not extend the stated limits to every codec/source.
