# Loaded MUSIC driver and frozen Stage1 findings — 2026-10-11

The development-signed **Entropy Music Render V2 0.3.0.10** is installed on the
sole `ROOT\MEDIA\0003` instance. Kernel source is `d3ba51e`; the permanent audit
package is `0.3.0.10-signed-dev-r6`, source `1322849`. Its kernel is unchanged
from the tested signed SYS, SHA256
`23A28749893F13A03C0A5E06FAE76B779669547882EB83E2E465250EA9A09428`.

## Recorded tests

| Probe | Actual outcome |
|---|---|
| Six rates × PCM16 / PCM24-valid-in32, Exclusive Event,10s | All12 raw cases pass;12,894,000 stereo frames |
| Same12 formats, Exclusive Polling | Initialize refuses0x80070032; position-only ownership unsupported |
| PCM24/192k Exclusive Event,1800s |345,600,000 stereo frames; every container byte exact; no lost frames, audit overflow or failed capture writes |
| All12 Event formats, PAUSE200ms in2s sequence | Every byte preserved; both paused clocks freeze; generation preserved |
| All12 formats, requested period2208frames,2s | Raw PCM passes; actual rounded buffer/frame counts recorded |
| Requested period192frames,2s | Four cases pass at44.1/48k; eight higher-rate cases refuse a period below device minimum |
| Native192k PCM→FP64, PCM16/24, two10s runs and one2s run | All six raw captures pass; FP64 continuity fails because invalid zero blocks are inserted inside MUSIC |
| Native192k FP64, requested period2208,2s | Both formats refuse before prefill: incomplete1024-frame output tail; raw captures contain0frames |

Across nine evidence families, **47 raw comparisons cover372,472,800 stereo
frames** with zero mismatched bytes/lost frames/audit overflow. This qualifies the
recorded raw Event path for finite tests. Unsupported cases remain failures.

The1800s Core diagnostic has MUSIC frames_in==frames_out==345,600,000,
lost_frames==overruns==faults==0, and41 FIFO underrun ticks. That run did not capture
FP64 publication, so it cannot establish uninterrupted Stage1 output.

## Remaining Stage1 defect

Isolated tests disable AUX, use fresh control/publication endpoints, and run a
hash-identical frozen Stage1v0002 copy. The audit Provider observes exactly the
PCM bytes returned to that Core; no second raw reader is involved.

The passive subscriber captures all FP64 clock blocks. Official output tests
fail at the first inserted invalid zero block. Repetition without concurrent
compilation also fails. In the10s repeat, PCM16 has10,240 interior invalid frames,
including a gap at source frame1,332,224 (about6.94s into the source). PCM24 has
1,024 interior invalid frames. Startup and tail zeros are classified separately.

The offline diagnostic compares *every valid block* against verified raw PCM.
Across six captures,8,448,000 valid stereo frames /16,896,000 binary64 samples
compare exactly: zero changed values or extra valid frames. This identifies a
chronology defect in the captured publication. Concatenating valid blocks for
diagnosis neither repairs the timeline nor makes acceptance pass. These tests
do not establish correctness of the historical Transport/Virtual drivers.

Frozen `SourceDomain::pop1024()` reports underrun on an empty FIFO;
`EngineLifecycle::tick()` then publishes an invalid zero block. Its independent
scheduler and ingress worker need separate investigation. The worker's `Sleep(1)`
no-data/backpressure paths are a candidate scheduling cause, not an experimentally
isolated root cause. Core/SRC/FIFO remain unchanged here.

## Strict completion evidence

Frozen publication clears subscription state to0 on both normal shutdown and
premature/error retirement, erasing the distinction. Core's final diagnostic
snapshot precedes scheduler shutdown and cannot prove final publication count.
Audit r6 drains retained slots but records this terminal state as an error.
Frozen v0002 publication completion remains **unqualified**. A later version
needs durable, distinguishable terminal publication provenance.

Eight full-process publisher fixtures cover intact clean exit, delayed extra block,
abort, abnormal exit, session retirement with later output, erased error retirement,
ambiguous normal stop, and abort at final drain. Earlier r5 fails four checks;
the final tool passes all eight. All12 CTest suites pass from an isolated extraction
of the final source ZIP. Independent review closed the Important audit findings;
it did not judge live acceptance or kernel safety.

## Preserved evidence and scope

[Complete evidence](acceptance-B-and-FP64-0.3.0.10.json) contains per-case reports,
provenance, allocation/clock/generation evidence, full gap timelines and local
capture paths/byte counts/SHA256. The approximately2.8GB stress capture stays
local; Git retains its hash and complete comparison. The
[24-case matrix](acceptance-A-0.3.0.10.json) retains supported and refused cases.
Historical failing packages/captures remain local.

The versioned archive in `Integration/Drivers/EntropyMusicRenderV2/` preserves
signed SYS/CAT/INF, both Providers, audit executable/runner, source ZIP and SHA256
inventory. See [permanent tool instructions](../../Drivers/EntropyMusicRenderV2/audit/TEST-TOOL.md).
No signing private key is included. The approved existing development certificate
and test-signing/trust policy were used; Microsoft production signing is not claimed.
No boot policy, default endpoint, TIDAL selection or active runtime changed.
Original listening Core/DSP/Stage3 processes remain running.

Frozen Core SHA256:
`AB6B817CF3A0C65379B349FA8E01F3302E08F5119B9ACBF874916D54D24D3C4A`.
Provider ABI SHA256:
`A4F848B9A4E653FD58ED937E4488D1DB81D5BAA1222CDEFB93813D07B40F60CD`.
Protected baseline72/72 is verified separately before delivery.

Real TIDAL Shared/Exclusive comparison, native OS partial EOS, power/removal,
overload and long ULONG wrap remain unqualified. Other-rate post-SRC comparison
needs an independent latency/startup/flush reference. AUX faults remain the
separate Stage1 follow-up. Full-chain acceptance is incomplete.
