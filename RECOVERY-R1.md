# Stage 3 Recovery R1 — Core input delivery and sink service

Status: source candidate for Windows compilation and a manual listening test. NOT an accepted audio reference. Windows/MSVC build, Qt deployment and physical playback have not been run by ChatGPT for R1.

This document supersedes the initial documentation-only handoff in STAGE3-RECOVERY.md. The owner has now authorized the listed source corrections and local packaging. Codex remains build/package-only; it must not invent further changes.

## Scope and reference

Base: `1b7a29d6d9f5aedb860c31aaa3374ad15b64bca7`, on `chatgpt/stage3-audio-recovery-20261013`.

The user reproduced chopped/rough mid and mid-low audio on a single full-range driver, including HP/LP bypass. This investigation is not predicated on band summation or a profile redesign. Version 0.1.0 is a comparatively better listening observation, not a perfect golden reference. R1 neither restores all of 0.1.0 nor increases the latency to recreate the older approximately two-second path.

## Actual source changes

1. `Engine/Input/ProgramIngress.cpp`: MUSIC-only reads no longer acquire the CINEMA guard; CINEMA-only reads no longer acquire MUSIC. A transition requesting both still acquires/validates both before consuming either. No partial paired read, new spin loop or fabricated input block.
2. `Engine/Input/DomainIngress.cpp`: peak analysis is combined with finite validation on producer-owned scratch BEFORE the shared queue guard. The guarded copy transfers the session's actual planes, not all eight scratch planes. Queue size, source headers, generation rules, demand/discard policy and all fault behavior are unchanged.
3. `Engine/Egress/LiveSinkWorkers.cpp` and `.h`: optional audit PCM/rendered-buffer copies are performed AFTER `sink->submit`, while the owner-local buffers are still unchanged. The existing callback wall metric starts at return from wait, rather than after validation/calibration/startup work. Additional passive telemetry exposes wait-return intervals, time through submit and estimated submission slack against the existing hardware-clock presentation mapping.

New telemetry: `recoveryRevision=R1`, `waitReturnIntervalUs`, `maximumWaitReturnIntervalUs`, `serviceWallToSubmitUs`, `maximumServiceWallToSubmitUs`, `submitTimingSamples`, `estimatedSubmitSlackUs`, `minimumEstimatedSubmitSlackUs`, `estimatedLateSubmissions`.

A negative estimated slack is diagnostic evidence, NOT a confirmed hardware xrun or a new playback gate. No disarm, resync, mute, drop/duplicate, phase correction or FIFO adjustment is triggered by these fields. Existing `deadlineMisses` remains a work-duration threshold, not a hardware continuity certificate. `callbackThreadCycles` retains its previous narrower scope; do not equate it with the expanded wall metric.

Transport/StagePublishV2 already checks stream epoch, block sequence, first_frame and publisher drops. That validation is preserved. The graph's local frame counter is not reclassified as a proof of source continuity. R1 does not claim to have demonstrated an actual lost source packet or the complete cause of the user's audible regression.

No changes to DSP/Crossover, PCA, filters, delays, Headroom, OutputMatrix, PCM24, ClockSync coefficients/controller/interpolator, WASAPI format/timeout, FIFO capacities/targets, source-selection semantics, Stage1, Stage2, drivers or UI. Startup calibration and presentation/re-prime algorithms remain unchanged.

## Verification actually performed

The original four source blobs were copied through the GitHub connector and matched their exact Git blob SHA before editing. A local diff contains 86 insertions and 14 deletions across those four production files. Original line endings were preserved.

A bounded portable C++ fixture compiles the exact ProgramIngress::tryRead method body with deterministic domain/lock substitutes. The original source failed the selected-domain contention checks; the corrected body passed 10/10 checks, including signed-zero/payload preservation and atomic dual-domain admission. A separate exact queue-copy fragment passed 6/6 byte-comparison cases for 3/6/8 channels and ring-end positions. Source/timestamp checks confirm removal of the guarded per-sample peak scan, full pre-render wall accounting, and post-submit audit-copy placement. These are narrow checks, NOT a full Windows build, concurrency stress test, hardware deadline measurement or audio-quality result.

Reproduction: `python3 Tests/RecoveryR1Portable.py` with g++ or clang++ on Linux/macOS. Recorded compiler: GCC 14.2.0. Do not run this or a general suite as part of the owner's local build-only task. No original CTest suite, Windows driver, physical DAC, Qt runtime or MSVC/IPP integration was executed here.

IAudioClock QPC units and buffer timing semantics were checked against Microsoft documentation:
- https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition
- https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiorenderclient-getbuffer

## Local build and package

Use the existing checkout, NOT a new clone:

`D:\Projects\Entropy-Stage3-Recovery-20261013\source`

First fast-forward this branch to the exact final R1 commit supplied in the handoff. Stop on tracked modifications or a different local branch; do not reset/stash/delete the user's work. Then:

```powershell
& .\tools\Build-Recovery-R1.ps1 -ExpectedCommit '<FULL_R1_COMMIT>'
```

The script builds only Core 64k through the established MSVC/IPP wrapper, including its existing offline --probe. It reuses the unchanged Qt executable already built in this checkout, verifying the SHA256 from the user's build report. No general CTest, -Verify, benchmark, 32k variant or live test is launched.

The SRC tree is already tracked in this checkout (198 files verified against the local reference in the user's build report); no dependency recopy or replacement is needed. This corrects the initial handoff's assumption that the tree was absent.

The script reads the existing saved `Documents\entropy.entropy`, copying it to the new runtime's `profile\listening.entropy`. For a different existing saved profile, pass its exact local path with `-Profile`. This is runtime setup, not a request to upload or change the profile for diagnosis. No profile values or DAC mappings are rewritten. An absent file is an error, never permission to invent settings.

Default result: `D:\Projects\Entropy-Stage3-Recovery-20261013\live\R1-64k`.

It contains the new Core, unchanged deployed Qt UI, manifest, SHA256SUMS and the existing versioned start/stop scripts. An existing R1 folder is never overwritten. An incomplete package retains INCOMPLETE.txt and cannot launch through Cold-Start.cmd. Build/package scripts are supplied for local execution; they have not been executed on Windows in this session.

Codex reports the source SHA, build/probe exit codes, Core/UI hashes, package path and final tracked Git status, then STOPS. It does not start/stop running processes, ARM, touch Stage1/2 or edit source code. On an error, report the first actionable error and stop.

## Manual listening and rollback

The owner closes the current Stage3 UI and stops that Core using its matching Stop-AudioCore.cmd. Closing the UI alone does not stop Core. Stage1/Stage2 stay independent and unchanged.

Run the new runtime's `Live-Test.cmd`. It starts DISARMED with the copied profile. Confirm mapping/level, then ARM manually. Reproduce the SAME one-way/full-range/bypass listening setup and passage used to hear the fault. R1 introduces no requested acoustic adjustment or extra gain.

If it faults or sounds worse, DISARM, close its UI and run THIS R1 package's Stop-AudioCore.cmd; then restart the original installed package. Do not replace its executable in place. Record whether the chopping changed; a clean build or zero estimated late submissions alone is not a sound-quality verdict.
