# Stage 3 Recovery R2 — isolated input queue and telemetry

Date: 14 October 2026. Base: `841e0ada1a1e2867a3fdb7af692ca0e7a8081719` (R1).
Branch: `chatgpt/stage3-audio-recovery-20261013`.

**Source candidate for local Windows compilation and manual listening. Not an accepted sound-quality reference.** R1 is retained unchanged in Git history and in the separate installed `live/R1-64k` package.

## User observation and the scope of this change

R1 restored much of the mid/low-mid sound and eliminated the reported severe fragmentation on individually auditioned ways. Fine high-mid/high roughness remains. The user reproduced it in MANUAL Headroom at -10 dB and with BITEXACT at 0 dB; they do NOT use AUTO. This invalidates treating AUTO gain jumps as the explanation of those particular observations. It also weakens the near-PCM-rail hypothesis, without supplying a measured diagnosis of every boundary. No new gain test or profile upload is required for R2.

R2 addresses the remaining *input delivery concurrency* defect only. It neither changes Headroom nor adds a limiter, gain ramp, peak reserve, normalization or clipping workaround. It does not claim the audible roughness has been proved to originate in this queue.

## Actual production changes

Only `Engine/Input/DomainIngress.cpp` and the new `Engine/Input/IngressConcurrency.h` change production behavior.

- The preallocated 16-block queue now has one producer cursor and one Graph-consumer cursor, published with release/acquire ordering. A normal active enqueue does not take a lock also needed by the consumer. Capacity is still 16 x 1024 frames; audio planes remain FP64 and are copied unchanged.
- Telemetry uses three preallocated ownership-swapped snapshots. The producer never waits on a telemetry reader and never takes the snapshot reader mutex. That mutex serializes only non-audio observers with each other; it is not an audio queue lock. `consumed`, current state and approximate occupancy are exposed separately as observations.
- A bounded consumer reservation protects the full block and session header during dequeue. Existing ProgramIngress still reserves/validates both requested domains before consuming either. MUSIC-only never requires CINEMA and vice versa; ProgramIngress.cpp is unchanged from R1.
- Only reconnect, source fault, stopped reset and inactive-domain keep-latest use the maintenance gate. A maintenance request excludes NEW readers; an already-pinned reader finishes before queue retirement or session replacement. Waiting is on the producer/lifecycle side, never in the Graph. Steady active delivery does not enter this gate.
- Inactive keep-latest remains explicit. Demand is rechecked after maintenance admission so a stale inactive observation does not discard a newly demanded queue. Active full-queue admission still faults; it does not overwrite unread audio, enlarge the queue, replay data or silently drop a block and continue.

These are not claims that the complete application or every lifecycle action is lock-free. The corrected steady-state queue is SPSC, telemetry is isolated, and reset/session replacement has explicit ownership. Snapshot header statistics may lag the current atomic state/counters; they are monitoring, not PCM identity authority. Actual PCM keeps its source headers and generation token.

## Deliberately unchanged

Crossover, PCA, filter coefficients and delays, OutputMatrix, Headroom in every mode, PCM24/dither, ClockSync sinc/servo, WASAPI, startup/presentation alignment, DAC FIFO capacity (65536) and target (4096), scheduling policy, Stage1/Stage2/CINEMA producers, transport validation, drivers, Qt, profiles and SDKs are unchanged from R1. No extra nominal audio latency is requested.

The coefficient-sharing optimization inside ClockSync is deferred: it would be a second independent intervention. No AUTO/peak instrumentation is added because the user's reproduction does not use AUTO. Existing PCM/timing counters remain available, unmodified.

The existing UI/core version text and sink telemetry's `recoveryRevision=R1` may still identify the unchanged r3/R1 components. R2 identity is the NEW package manifest, pinned Git SHA, Core hash and `Stage3-Recovery-R2` startup banner, not a relabelled old executable.

## Verification performed here

The original DomainIngress and ProgramIngress source copies were verified against their exact Git blob SHA. An executable fixture compiled the original acquire() method and demonstrated that a held telemetry guard rejects audio admission (expected RED).

The new production concurrency header passed focused native C++20 checks: FIFO/full/wrap handling, signed-zero and subnormal-bit preservation, maintenance exclusion while PCM is pinned, coherent multi-reader telemetry, 10000 queue transactions while a telemetry copy is deliberately stalled, and 100000 ordered concurrent producer/consumer blocks. GCC 14.2.0 and Clang 17.0.0 passed. GCC AddressSanitizer + UndefinedBehaviorSanitizer and ThreadSanitizer runs of this header fixture completed with exit 0 and no reported diagnostics.

A separate adapter compiled the COMPLETE DomainIngress.cpp and unchanged ProgramIngress.cpp method bodies with deterministic Windows/StagePublish substitutes. It passed 3/6/8-channel byte comparisons with concurrent telemetry, inactive keep-latest, active overflow fault, reconnect while an old session/block is pinned, paired-domain atomic admission and unselected-domain independence. This checks the implemented methods, NOT the Windows ABI, real shared-memory transport or OS/DAC scheduling.

Reproduce those bounded checks with `python3 Tests/RecoveryR2Portable.py` and a C++20 g++/clang++ compiler. The script installs nothing and uses a temporary directory. It is NOT part of Codex's local build-only instructions.

No complete Windows/MSVC/IPP build, original CTest suite, PowerShell deployment, actual StagePublish IPC, hardware throughput measurement or DAC playback was executed here. Sanitizer/fixture success is not a mathematical proof of all concurrency schedules or an audio-quality verdict. The build/package script was reviewed but not executed on Windows in this session.

Concurrency reference (no new Boost dependency): the SPSC ownership and non-thread-safe reset requirements are also explicit in Boost's original documentation:
https://www.boost.org/doc/libs/1_73_0/doc/html/boost/lockfree/spsc_queue.html

## Local build and packaging

Use the existing checkout:
`D:\Projects\Entropy-Stage3-Recovery-20261013\source`

Fetch and fast-forward to the exact full R2 commit in the handoff, then:

```powershell
& .\tools\Build-Recovery-R2.ps1 -ExpectedCommit '<FULL_R2_COMMIT>'
```

The script checks R1 ancestry and protected source equality, preserves the known R1 Core hash, compiles ONLY the new Core 64k with the established offline --probe, and reuses the unchanged UI hash from the successful local report. No new dependency copy, CTest, benchmark or 32k build. Errors stop the task; Codex does not repair sources.

Default new package:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live\R2-64k`

The SAVED R1 listening profile is copied read-only with its hash, not edited. An unsaved adjustment in the running UI is not present in that disk file. The owner should use the same saved test setup for the comparison; an exact alternative saved file may be passed via -Profile. No AUTO setting or extra attenuation is introduced. The script refuses an existing R2 directory and leaves INCOMPLETE.txt if packaging fails.

Codex reports revision, build/probe/deploy result, executable/profile hashes, final tracked status and the Live-Test.cmd path, then stops without starting/stopping audio processes.

## Manual listening and rollback

After successful local packaging, the owner DISARMs the current R1 instance, closes its UI and stops its Core using R1's own Stop-AudioCore.cmd. UI closure alone does not stop Core. Leave Stage1/Stage2 independent and unchanged.

Start `live\R2-64k\Live-Test.cmd`. It starts DISARMED with the copied saved profile. Verify the same mapping and MANUAL/BITEXACT setup used for R1, ARM manually, and compare the same isolated way/full-range speaker and passage. Do not vary FIFO size, gain policy or upstream processing simultaneously.

If R2 is worse or faults, DISARM it, close its UI, stop it using R2's own Stop-AudioCore.cmd, then restart the untouched R1 package. Nothing is overwritten in place. Whether fine roughness improves remains an empirical listening result to report, not a promise of this patch.
