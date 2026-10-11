# Stage1 r7 / Stage3 r2 publication — 2026-10-12

Published on the existing `codex/music-cinema-architecture` branch after local
live verification and the user's explicit approval to upload sources,
documentation, tools and tested deliveries. Stage1 and Stage3 changes have
separate commits on that branch. Stage2 MUSIC v0009, signed Music Render V2
kernel 0.3.0.11 and the paired raw Provider r3 remain unchanged.

## What changed

Stage1 r7 applies a provider's RUN state only to the ingress worker's prepared
generation. A new stream without its first PCM packet no longer starts the old
SourceDomain's resume deadline. Actual same-generation starvation remains counted.
No PCM decode, SRC, gain, FIFO or startup-prime algorithm changes are introduced.
The source mirror is [Integration/Music/Stage1-v0003](../Integration/Music/Stage1-v0003).
See the [full r7 report](../Integration/Music/Stage1-v0003/docs/V0003-R7-GENERATION-RESULT.md).

Stage3 r2 separates logical user ARM from physical output. If Stage1/Stage2 stop
or their input becomes invalid, it closes native DAC owners and retains
MUTE/WAITING_INPUT indefinitely, without fabricated source/Graph/PCM blocks.
Fresh validated input reconnects and resumes the same Core. Explicit DISARM
cancels resumption. Graph, PCM, clock and device failures keep fatal provenance
and automatically disarm; older queued ARM commands are invalidated.
The [r2 report](STAGE3-SOURCE-WAIT-r2.md) records the exact behavior and limits.

## Tested deliveries and provenance

- [Stage1 r7 runtime/source ZIPs and manifests](../Integration/Music/Stage1-v0003/releases/stage1-native-music-v0003-r7).
- [Stage3 r2 32k/64k runtime ZIPs and source ZIP](../release/music-cinema/Stage3/0.1.9-v2-r2).
- [Five-archive path, size and SHA256 index](architecture/publication-r7-r2-artifacts.json).
- [Original archive closure verification](../validation/music-cinema-v2/source-wait-final-archive-verification.json).
- [Unchanged Stage2 v0009 delivery](../release/music-cinema/Stage2).

All five original archives are copied byte-for-byte and rechecked against their
per-file manifests. Earlier releases remain available. Archive manifests record
the modified local working tree based on `34556e782e47a67417aadbf7b06e12c330f7da10`;
they do not pretend that the tested binaries were built from a later publication
commit. The original October 11 reports' `gitUploaded:false` and no-upload prose
are historical state at validation time, superseded by this publication.

Stage3's original source ZIP includes Stage1 r6, as it did during its build.
Stage1 r7 is supplied separately and imported into the current Git source tree.
The only post-live-test Stage3 code change is in `Tests/MusicCinemaSourceWait.cpp`:
the late-fatal test waits until the lifecycle owner closes its publication gate
before injecting Clock fault. Previously the fake sink could stop before policy
service, exercising a different event ordering and producing intermittent test
failure. Both original and corrected results are retained. Production source
and runtime binary hashes remain identical to the locally tested candidates.

## Verification and remaining limits

Stage1 Core tests pass 6/6. Its real r7 transition audit records 12,722,176 stereo
source frames over 299.781 seconds, zero MUSIC underrun/fault/lost/overrun,
publisher drops and clock-late counters. Native-192k byte identity qualification
from r6 remains historical evidence for its original binary, not an r7 claim.

Stage3's fresh publication suites record 56/57 on each 32k/64k variant. The sole
native failure remains inherited `EntropyStage04Offline` (`unsafe PCM rejected
before converter`). The corrected late-fatal fixture passes 100 consecutive
runs per variant. Relevant Qt presentation/lifecycle/device tests and original
packaged startup smokes pass. Broader `EntropyStage05Ui` still fails; it was also
reproduced with the unchanged accepted Core. No tests are disabled or reclassified
as successes. No full Stage04/V07 qualification is claimed.

The original real 32k/64k audits exercise upstream stop/restart, retained ARM,
no advancing Graph/sink PCM counters while waiting, UUID reconnection, explicit
DISARM, ARM without input and unavailable Cinema rejection. The same Core survives
each audit. Final continuous intervals are 30/60 seconds, followed by a separate
20-second 64k listening snapshot. There is no ten-hour wall-clock audit.

Actual Cinema decoder playback, original TIDAL PCM identity, automatic WASAPI
Shared/Exclusive detection, all machine loads and analog DAC skew remain outside
these results. Producer frames may be lost in the finite driver ring while
Stage1 is deliberately stopped; those losses stay recorded and are not called
zero-loss transport. The AUX stereo investigation remains separate future work.
