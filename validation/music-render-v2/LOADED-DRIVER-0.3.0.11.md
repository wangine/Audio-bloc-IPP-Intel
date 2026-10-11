# Loaded Music Render V2 0.3.0.11 + Stage1 v0003-r3

The installed device remains `ROOT\MEDIA\0003`, hardware `ROOT\EntropyMusicRenderV2`,
service EntropyMusicRenderV2. Existing DEV signing certificate/trust/boot policy
was used. PnP version is **0.3.0.11**, published INF **oem28.inf**, service Running.
Signed SYS SHA256: **FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA**.
No other device/driver or default output was changed.

## Reproduced kernel latency and correction

0.3.0.10 intermittently spent **16.1713 ms** in a successful raw read's completion,
while the generator's maximum commit interval was **11.34 ms**. The Core sequence
then contained an interior invalid 1024-frame block. All 3,840,000 input raw
frames still compared exactly, so raw identity alone did not prove output timing.
The failed `r3-native-full-20s` and `r3-render-timing-normal` reports are retained.

`PcmCommit::EnterPassive()` retried a busy publication gate with a periodic
1 ms KeDelayExecutionThread; this could round to the system tick. Version0.3.0.11
waits on an embedded synchronization event, signaled after gate release. RT paths
still only attempt nonblocking acquisition. Rundown/read leases are preserved.
Blocking wait is confined to passive paths, matching Microsoft's
[wait](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kewaitforsingleobject)
and [event](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesetevent)
requirements. The source regression fails on the old retry and passes on the event.

## Measured qualification

- Stage1 v0003-r3 Core SHA256 **C36CE4148FC50BBF4DFA002AD7592BA50D4AF79A1ECF8960063BF30FB091A147**.
- Native192k PCM16 and PCM24-valid-in-32, 60 seconds each: **23,040,000 stereo frames**,
  exact normalized binary64, zero mismatched samples, zero interior invalid frames,
  complete final drain, durable terminal3 and zero subscriber errors.
- Raw Event-Driven, both integer formats at 44.1/48/88.2/96/176.4/192kHz,
  10 seconds/case: **12,894,000 stereo frames**, byte exact, zero lost frames.
- The captured positive read maximum became **0.19/0.18 ms** in the bounded traces
  for PCM16/PCM24. Generator commit maxima were11.38/11.55ms. These are trace-window
  observations, not worst-case bounds for every future load condition.
- Driver/model12/12 CTest; source archive rebuilt independently12/12 CTest.
  Stage1 Core6/6 CTest; terminal publisher9/9 including unread final blocks.
  Isolated packaged CMD lifecycle passes start/repeat/cold/stop and identity refusal.

Detailed results are in
[Stage1 acceptance](../../Integration/Music/Stage1-v0003/evidence/V0003-ACCEPTANCE.json)
and its per-case evidence. Known raw/FP64 capture binaries remain local; compact
reports, runner hashes, exact source archives and runtime bundles are preserved.

## AUX and real listening

TIDAL legacy MUSIC + Spotify on Windows default AUX Shared ran for300s on r3,
with25ms delayed control reads and concurrent Qt polling: both sources RUNNING,
zero Core faults/underruns, zero blank metadata and zero regressed frame snapshots.
Those listening sources do not exercise Music Render V2's raw PCM contract.
AUX Exclusive ingress with MMCSS Pro Audio HIGH passed120s, **5,292,000 frames**,
zero position gaps. The non-MMCSS Exclusive failure remains separate evidence.
Stage2v0009 and Stage3 binaries, source processing, profile and DAC assignments
were not modified. Stage1v0002 remains a frozen reference.

## Limits

At other rates SRC changes samples and frame counts; input/output sample identity
is meaningful here at native192k only. r8brain differential and level tests cover
the six input rates without hidden gain/clipping. Invalid clock blocks during
startup/stop are recorded separately from gaps inside a valid source sequence.
Polling/position-only raw commit is unsupported. Real TIDAL Shared/Exclusive PCM
equivalence and application-native EOS are not certified. Historical1800s raw stress
was on0.3.0.10 and is not silently promoted to a0.3.0.11 long-stress qualification.
These are finite tests of the recorded binaries and conditions, not a production
or unconditional zero-loss claim. Existing0.3.0.10/r6 packages and failures remain unchanged.
