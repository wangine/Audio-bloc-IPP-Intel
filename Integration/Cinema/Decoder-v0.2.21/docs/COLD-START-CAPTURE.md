# Cold-start capture ownership

The v0.2.16 diagnostic acquired its first WASAPI packet, then created FFmpeg
and the pipe owners before releasing that packet. The trace measured a
466.5738 ms hold at a 10 ms endpoint period, followed by five discontinuity
flags and one recovery. CheckCaptureStartup.py fails on that preserved trace.
This is a separate startup mechanism from the v0.2.15 interruptions during
continuous playback. The short v0.2.16 trial had no additional discontinuity
or recovery in its subsequent 64 seconds; that duration alone is inconclusive.

v0.2.17 creates the initial child before IAudioClient::Start. The first codec
selection sets the existing atomic codec metadata and adopts the unused child.
No carrier has been sent to that child before this selection. If the prepared
child has exited, the existing restart path applies. Subsequent codec changes,
discontinuities, stalls, child exits and failed pipe writes retain recovery.
The original GUI, carrier bytes, SRC, calibration, gain, capacities and endpoint
period are preserved. This does not suppress any WASAPI discontinuity flag.

Acceptance: measure the first acquired buffer against its 10 ms deadline;
require no new capture flags, recoveries, pacer underflows or publication drops
in the full continuous live trial with the existing Stage 3 profile and DACs.
Verify production installation separately. Do not mark stable or push without
the live result. Recovery after an actual transport discontinuity still performs
process lifecycle work synchronously; this candidate does not claim otherwise.

Microsoft documents that GetBuffer and ReleaseBuffer should occur within the
same processing period, otherwise samples can be lost:
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer
