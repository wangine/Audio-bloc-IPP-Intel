# ClockFollow C1 — MUSIC driver rate actuator, not a live clock bridge

Approved direction: retain separate driver, Stage1, Stage2, Stage3; make the controllable MUSIC virtual clock follow a selected physical master later, without resampling source PCM in the driver. The C1 scope is the driver-side actuator and a versioned private control ABI, inactive until a controller explicitly acquires a live stream session. It does not remove WDL or modify any stage.

Baseline is `fa9e126d4bcbde2aaaf033de78509c6f601f8dbb`, the source pinned in the installed 0.3.0.11 package report. All work stays on `chatgpt/music-clockfollow-c1-20261015`; no live-renderer branches, installed SYS, INF registration or signing policy are changed.

## Contract

One monotonic QPC-to-frame timeline drives position, packet count and notification decisions. A fine-rate update rebases the continuous accumulated phase, keeping its rational remainder; it never edits earlier frame counts, resets rings, modifies PCM, or synthesizes/drops frames. Integer arithmetic only, bounded work, no allocations or waiting in the realtime path.

The new control extension is additive to Read/Info/Claim/Trace. A separate handle-scoped control lease does not take the RAW reader claim. Acquire binds the current driver stream generation to a nonzero master epoch. Updates carry the exact lease, stream generation, master epoch and monotonically increasing sequence. The actuator takes correction in signed parts per billion, bounded to +/-1,000,000 ppb (+/-1000 ppm); slew is limited to 20 ppm per elapsed QPC second. The user-mode bridge will estimate the desired rate later; the kernel never reads a DAC or accepts arbitrary position writes.

A two-second renewable lease holds the last rate between updates. Expiry returns the clock's slope to nominal QPC without a phase discontinuity and exposes EXPIRED. This is not certified continued synchronized playback: the later DIRECT_PRIMARY renderer must stop/wait when the lease is invalid. DISARM/source reset/master change need explicit reacquisition; no peer-master auto-selection. Rate transitions on explicit release/expiry may change slope, never phase; the later bridge controls handover while output is muted. No implementation here claims an uninterrupted transition between different masters.

PCM OS commits must use a coherent clock snapshot and QPC sampled under the same metadata lock. The new command must not introduce a rebased clock snapshot paired with an older independently sampled query timestamp. Commands and kernel queries retain the existing PcmCommit operation/rundown discipline; control returns busy rather than waiting inside the audio path.

## C1 acceptance and installation boundary

The local task is compilation of an isolated unsigned candidate plus the bounded clock/model contracts. There is no installation, certificate import, signing, reboot, registry change, tone, capture, DAC open or modification of Stage1/2/3. The legacy provider ABI and PCM conversion-free payload path remain compatible. The driver-side capability is not usable as a complete synchronized system until ClockBridge and Stage1 pacing are integrated.

The first C1 compile is not a listening test and cannot change the currently installed system. Preserve the installed .11 package for rollback. Kernel compilation and physical driver validation are distinct from passing portable model contracts.

## AUX and CINEMA boundaries

AUX must later receive its own compatible controllable-clock adapter, before a simultaneous MUSIC+AUX mix is declared in the master clock domain. Do not assume the MUSIC change also affects the separate AUX endpoint.

The user confirms CINEMA is received optically through Hifime UR23, then DD/DTS is decoded before the existing FP64/192 publication. This is an external physical clock domain. A stereo 16/48 transport containing encoded bursts is not ordinary stereo program PCM and must not be resampled/gain-modified before depacketizing/decoding. Preserve all existing CINEMA code in C1. At a later boundary, adapt the decoded PCM to the output master, preferably within its single existing SRC stage if that implementation supports variable ratio. This plan does not certify 7.1 discrete content from a two-channel transport label.

## Execution

1. Compile a failing actuator contract against the original clock (expected missing-actuator failure, not a compiler failure).
2. Implement the bounded clock primitive and handle/session protocol plus kernel integration.
3. Re-run the same contracts and unchanged model/packet tests, inspect every changed file.
4. Publish an exact commit and compile-only Codex handoff. Local build uses the existing WDK and locally extracted SDK; no toolchain substitution. No SYS installation at C1.
