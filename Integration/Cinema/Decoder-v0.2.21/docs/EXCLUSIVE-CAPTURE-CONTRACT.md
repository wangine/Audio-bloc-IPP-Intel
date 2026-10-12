# Exclusive capture contract investigation

Preserved v0.2.17 live trial: 32,965 acquired packets, two DATA_DISCONTINUITY
flags, one recovery. The first flag at 161.9793 seconds followed a 10.0038 ms
owner interval. All ten preceding buffers were acquired and released normally.
No pacer underflow, publication drop or decoded discard occurred. The initial
buffer hold improved separately from 466.5738 ms to 0.7873 ms.

A second v0.2.17 trial correlated with Windows Audio/CPU ETW captured one flag
at 163.3393 seconds (device frame 7,840,320; owner interval 10.0309 ms).
Microsoft.Windows.Audio.KernelStreamingEndpoint reported at trace time
257.722423 seconds, decoder PID 21232 / capture TID 6168:
KseGlitchEvent::IOMGR_NO_OUTSTANDING_PACKETS (enum 1, count 1).
The KsePinGuid ce5b86a8-6ba0-460a-bfb0-766a1f1bf785 was created for the UR23
endpoint e2456064-fa8b-45ae-922f-6615b3ee9919, Exclusive, 48k/16 stereo.
This identifies the first observed failing boundary in Windows KS capture;
it does not establish physical carrier corruption or a particular USB fault.

The source queried GetNextPacketSize before and after every acquired buffer.
Microsoft specifies that API only for Shared, not Exclusive:
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getnextpacketsize

v0.2.18 removes these unsupported queries from the actual Exclusive capture
loop. It drains directly with GetBuffer / ReleaseBuffer. BUFFER_EMPTY and
Exclusive BUFFER_ERROR wait for the next processing pass; all other failures
remain explicit errors. Zero frames acquire no owned packet, so are not released.
Actual discontinuity flags and later FFmpeg recovery are preserved. No byte,
SRC, clock, gain, filter, capacity, period, GUI or protection change is made.
Handling of the empty results follows Microsoft's GetBuffer contract:
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer

The preserved v0.2.17 live continuity regression is red. Require a new trial
longer than the repeated ~162-second failure interval with no flags/recovery,
and with the same Stage 3 profile and DACs active. Unsupported API removal is
correct independently; attributing the observed glitch to it requires a green
live trial. Do not suppress flags, claim bit identity, or upload as stable.

## Result: hypothesis did not pass live validation (2026-10-13)

The v0.2.18 DTS run was stopped after reproducing the fault. It acquired
22,576 packets, one DATA_DISCONTINUITY and one recovery at 163.1392 seconds.
No pacer underflow, publication drop or decoded discard occurred. Removing
GetNextPacketSize did not remove the first capture glitch. The Stage 3 observer
kept logical ARM true and recorded no live fault; recovery caused approximately
four seconds of source reacquisition. v0.2.18 is not installed or stable.

A subsequent passive, preallocated-memory carrier capture received AC-3,
not DTS. All 35,564,160 bytes are retained with SHA256:
190282bd00c66cb9d4f4c57e59494898db15e866d4c89145f31d5a202f41a885.
There were 18,523 packets and one flagged packet at 162.2593 seconds,
device frame 7,788,960, byte offset 31,155,840. Its 1,920 bytes are zero.
5,788 IEC61937 AC-3 headers have exactly 6,144-byte spacing throughout,
including across this packet. No carrier bytes or silence were removed.
FFmpeg offline decoding with -err_detect explode -xerror exited zero;
this is evidence of a decodable AC-3 capture, not master bit identity.
No decoder recovery followed that flag because the packet had no IEC header.

The same AC-3 run had one startup pacer underflow at 0.1040886 seconds.
Its first decoded frame contained 1,536 samples; the following frame arrived
38.0109 ms later, after the initial queue was drained. There were no later
pacer underflows, publication drops, decoded discards or live DAC faults.
This startup issue is distinct from the ~163-second capture flag.

Next required observation: retain the actual DTS carrier across the flagged
packet and inspect burst spacing, payloads and strict offline decoding.
Do not infer that the DTS payload is corrupt or intact from the AC-3 result.
Do not change recovery, gain, buffers, SRC, clocks or protection speculatively.
The standard installed v0.2.15 GUI/autostart was restored for listening.

Evidence directory: D:/Projects/3Stage-3/Diagnostics/Cinema-Disarm-20261012
- v18-decoder-live.log / v18-decoder-trace.csv
- v18-stage3-state-summary.txt / v18-live-telemetry.jsonl
- v18-raw-decoder.log / v18-raw-decoder-trace.csv
- v18-carrier-48000-s16le-stereo.raw
- v18-raw-boundary-analysis.txt / v18-raw-owner-analysis.txt
- v18-carrier-offline-decode.log / v18-raw-live-final.json
- MakeV18RawTrace.py / AnalyzeCarrierBoundary.py
