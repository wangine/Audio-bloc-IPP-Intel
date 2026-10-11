# Stage1 v0003-r6 — isolated qualification, 11 October 2026

Final Core SHA256: `FE3479AE0060A3D90336526C45417CADDBB0A95ECD48FDA87542CE3402334F87`.
Raw Provider SHA256: `3329D657956A42A6E83C28850EE457D1A3A9BF9AC327955825621732B6C4B0DE`.
Provider bundle: `0.3.0.11-signed-dev-r3`, user-mode revision only.
Installed signed kernel .11 SHA256:
`FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA`.

The driver remains installed as before. Stage2 v0009 and Stage3 are unchanged.
Frozen Stage1 v0002 and earlier packages remain preserved. r6 accepts the new
Provider lifecycle capability; v0002 must keep its original Provider.

## Known PCM and clock

The all-rate test covers PCM16 and PCM24-valid-in-32 at 44.1, 48, 88.2, 96,
176.4 and 192 kHz: 12 Exclusive Event-Driven cases, two seconds of supplied PCM
and a one-second Stop/Start without Reset in every case. All **2,578,800 stereo
frames** compare byte-for-byte before SRC, with no lost frames, duplicate frame
indices, audit overflow or failed audit writes. Clock and source frame positions
remain unchanged during pause, with the same generation on resume.

The traced Core has zero underruns from startup through the final supplied PCM
packet, including every pause and resumed RUN. Total fault/lost/overrun,
publisher-drop and clock-late counters are zero. One 176.4 kHz PCM24 case records
one later RUN underrun after the supplied tail; it is retained explicitly.
Finite client termination is not claimed as a universally zero-counter session.

Native 192 kHz output qualification compares **7,680,000 stereo frames**, two
20-second PCM16/PCM24 cases: every sample is exact binary64, no interior invalid
frames, durable completion, zero source/publication/clock-late counters. The
boundary-invalid totals are 31,744 and 25,600 frames respectively, outside the
valid sequence and retained separately. Detailed qualification is recorded in
`evidence/IDLE-R6-RESULT.json`, with the exact candidate hash, per-format frame
counts, boundary-invalid frames, comparison results and durable completion.
At other rates, SRC changes the sampling grid; byte identity concerns the raw
input, while the unchanged SRC is verified by differential regressions.

## Failure evidence and scope

The original r3 pause/play test counted 1,761 legitimate idle ticks as underruns.
r4 exposed false initial SRC-settling counters. Both failures and the r5
post-tail aggregate failure are preserved; only the final candidate's recorded
scope is qualified. Regression tests retain genuine RUN and UNKNOWN starvation,
queued PCM drain and actual Provider discovery.

The complete timing trace is bound to independently verified PCM frame count,
packet count, frame indices, generation and final publication count. Ingress
and scheduler truncation are rejected. `R6-CONTINUITY-COVERAGE.json` records the
strengthened check on all 14 final cases; validator regressions retain a
post-tail RUN counter and reject internal starvation and truncated evidence.
Diagnostic traces hold at most 4,096 ticks (approximately 21.8 s), and require
complete coverage to qualify a case; these tests fit that limit.

This is known-pattern qualification, not a capture of original TIDAL samples or
proof of equivalence between TIDAL Shared and Exclusive. It is a finite test,
not an all-load guarantee or long stress qualification. AUX rate/stereo
investigation remains unresolved and is not certified by these MUSIC probes.

Qt now displays `Raw PCM · WASAPI access unverified` for V2 mode0. Automatic
Shared/Exclusive detection is still unavailable in the kernel metadata.
