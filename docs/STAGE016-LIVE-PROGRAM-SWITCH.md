# Live MUSIC/CINEMA control regression — stable beta 0.1.5

Baseline: published 0.1.4, commit 4538aab0731a89aaf7430471294c501b5da79bb0.
Branch: codex/live-program-switch. GitHub publication authorized by the user
on 2026-10-10. No main merge or changes to earlier release folders/tags.

The native Core rejected `program`, `parameter(cinemaSource)` and bulk scene
requests unconditionally while LiveSinkWorkers was engaged. The Qt selector
then reverted to MUSIC despite valid CINEMA source meters. This host gate,
present before PCA 0.1.4, was the confirmed cause.

Core now previews the exact command on an independent native AudioGraph with
the current serialized saved scenes, Matrix and Headroom settings. The source
clone first processes at least 940 blocks and completes the native mode ramp;
then the target is restored by the original control transaction. FIR8193 x16
is rejected before processing the target. Twenty measured transition blocks
cover the original 9600-sample old/new crossfade. Original deadline and finite
output gates apply. Target warmup and strict 1000-block preflight follow.
Only PASS permits the original live command, adopted at a block boundary.
No reset/restart of live DAC owners, clocks, FIFO or source transport occurs.

Scene preflight remains bounded by the original 20-second control-thread
budget. Qt allows its existing 30-second response budget for scene commands
and telemetry queued behind them. The Core's five-second idle lease now starts
after the complete response. Idle revocation and slow-write deadlines remain
unchanged. A real server regression caught the former disconnect after a
5600 ms handler; the repaired server accepts the following request normally.

## Verification

Focused real offline scene test PASS: actual target scene, all command aliases,
cache reuse, unchanged original scene/Headroom, forbidden saved FIR8193 x16
and out-of-range rejection. Original Core failed this new regression before
the fix. A second RED exposed absent source warmup/transition evidence before
the transition validation was added. Native transport lease regression RED
then GREEN. Qt long-response regression RED then GREEN for scene selection.

MSVC 19.51.36257 x64 / Intel IPP 2026.0 and Qt 6.11.2 builds PASS. Existing
EntropyStage07Live, EntropyStage05Boundary and EntropyStage05Control: 3/3 PASS.
Native transport test PASS; four Qt timeout variants PASS; Qt device selection
PASS; packaged Core/Qt isolated offline startup PASS. All 542 initial packaged
manifest hashes matched. A nonfatal existing Qt font-size warning appeared
when opening the Crossover dialog; no visual code was changed.

Offline copy of the user's preserved configuration at 1024 frames /192 kHz:

| Phase | Median | p99 | Maximum | Deadline misses |
| --- | ---: | ---: | ---: | ---: |
| MUSIC→CINEMA transition, 20 blocks | 799.8 us | Not estimated | 1713.8 us | 0 |
| CINEMA steady, 1000 blocks | 651.1 us | 878.7 us | 2395.6 us | 0 |

Deadline: 5333.333 us. These are independent offline wall-time measurements,
not a full CPU benchmark or a global Stage04 realtime certification.

Physical manual test, previously authorized by the user: ARM once, then
MUSIC→CINEMA→MUSIC→CINEMA by IPC. All commands accepted/applied. Both configured
eight-channel DAC workers stayed RUNNING/ARMED, with zero observed XRUN,
deadline-miss, fault, PCM nonfinite/overrange/clipping counters in the recorded
snapshots. Native graph output was nonzero in CINEMA. The same selector then
worked in both directions through Qt while ARMED. Final UI: CINEMA/ARMED,
active module meters, transport faults 0 and underruns 0. Stage1/2 PIDs did not
change during Core handover or switching. The preserved private profile and
full live snapshots stay under local/stage016 and are excluded from releases.

No changes in DSP/, Engine/AudioGraph.cpp, Engine/Egress/, Engine/Input/ or
UI/Legacy/. PCA, Headroom gain/timing, PCM24, ClockSync, Guardian, WASAPI,
source independence and visual layouts retain their 0.1.4 implementations.
Historical Stage04 FAIL REALTIME, formal V07 BLOCKED and 0.1.4 residual PCM
overrange remain visible. No full DSP suite or benchmark rerun, or automated
acoustic certification. Evidence: validation/stage016.

User live acceptance, 2026-10-10: stable operation and working MUSIC/CINEMA
switching; publication approved. Version 0.1.5 becomes the accepted stable
beta reference for subsequent integration work. The runtime is unchanged
since the live test; release publication updates documentation/metadata only.
Technical and historical gate limitations above remain explicit.

Core SHA256: 0EC4B47A9A7F3377D12EE1EC4186949C34F8A0565482426E019610DBF16983EB.
UI SHA256: 5F4D9F90A860F84B6CCA19E8D91DF60B6E0A6EDB5FCBD782CFE3AA2C94643515.
