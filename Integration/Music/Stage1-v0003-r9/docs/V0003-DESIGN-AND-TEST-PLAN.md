# Stage1 v0003: ingress timing, capture queues and final publication

Frozen reference: Stage1 v0002, unchanged. Candidate: native C++ Core with separate Qt client. Stage2 v0009 and Stage3 remain independent consumers.

## Reproduced causes

- MUSIC Provider waited approximately15ms for a requested2ms timeout; Core added another coarse1ms sleep. Captured intervals reached32ms while publication consumes1024frames every5.333ms.
- Consuming the first available output block cannot bridge a10ms input packet period. Worker scheduling also needs MMCSS; scheduler registration alone does not protect ingress.
- AUX capture waited for a fresh event before every packet instead of draining queued packets. A real Shared run rejected an unmarked353-frame position gap at67.5317seconds.
- Normal publication shutdown erased its state to0, indistinguishable from retirement. An exact sample comparison could not certify completion.
- Control server disconnected immediately after writing a buffered response; a reader delayed25ms lost the response. This affects status delivery, separately from PCM.

## Implementation

Use cancellable high-resolution provider waits. Core does not add a second delay after an already-blocking provider read; nonblocking providers receive a local1ms high-resolution wait. Ingress joins MMCSS Pro Audio.

Prime the existing16-block FIFO with ceil(packet output frames/1024)+1 complete blocks, capped at its existing capacity. For a1920-frame/192k packet this is3072frames,16ms of stored audio; legacy2048-frame/44.1k packets require10240frames,53.333ms. This is explicit startup buffering, with no added steady processing stage. Actual delay includes scheduling and SRC startup. A bounded startup deadline releases complete short streams. Every fill/pop decision binds the same generation token. Startup is not counted as a steady underrun; a real underrun after priming is still counted and published invalid.

Capture reads GetBuffer until empty and only then awaits another event. This also removes the Shared-only GetNextPacketSize call from Exclusive capture. No retry hides capture errors or frame-position gaps.

After publication owner and source workers join, normal stop release-transitions connected1→3. Retirement0 and error2 survive cleanup. Audit drains all queued blocks and requires a fixed terminal write cursor, exit0, no drops and exact expected source coverage. Terminal publication does not substitute for source-frame completeness. Final diagnostic snapshot is taken after shutdown.

Control server waits for a client receipt or close before disconnecting. Request/response binary sizes remain V1; old clients can close after reading. New clients send an acknowledgement. Audio scheduler does not perform this control work.

No gain, mute, clipping, normalization, dither, r8brain modification, FIFO enlargement, Stage2/3 change, kernel replacement or default Windows endpoint change.

## Required checks

1. Preserve failing old immediate-consumption, split capture queue, ambiguous stop and delayed control response evidence.
2. Native PCM16 and PCM24-in-32 at192k: every raw input byte and every output binary64 sample, first to last; reject interior invalid blocks and incomplete/aborted drains.
3. Raw ingress matrix: both PCM formats at44.1/48/88.2/96/176.4/192k. This matrix certifies ingress, not native sample identity after nonnative SRC.
4. Exact20s synthetic clock regression, generation fencing, complete short streams, bounded memory and existing SRC differential tests.
5. Publication error/retirement/abort/extra-frame tests, including two final unread blocks when terminal3 is published.
6. Invalid trace destination must not crash Core shutdown; saturated traces must remain internally consistent.
7. Real AUX Shared and Exclusive capture; then TIDAL MUSIC plus default-output Spotify AUX with live counters. Do not turn absence of a fault into a claim of bit identity for arbitrary Spotify/TIDAL source material.
8. Packaged Qt IPC and original-format display, source-only build, immutable artifact hashes, code review and private architecture-branch delivery.

Primary capture reference: https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer . Shared-only packet-size restriction: https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getnextpacketsize .
