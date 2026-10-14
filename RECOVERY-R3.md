# Stage 3 Recovery R3 — low-bandwidth clock observation

14 October 2026. Base: `2cd916e30cd0f3a6253e429c68e9208f76cd1942` (R2).
Branch: `chatgpt/stage3-audio-recovery-20261013`.

**Source candidate for local MSVC compilation and manual listening. The physical cause of the reported audible roughness is not proven, and R3 is not an accepted sound-quality reference.** R2 and R1 remain intact for rollback.

## Evidence from the supplied R2 interval

61 successful snapshots over approximately 12.015 seconds, Qt closed, one unchanged Core PID 16072. Observed executable SHA256: `3E4DAFD030E01EFA2BDF4C2B5C81E866D747519A6ECA6908BA5E3B61D1188A86`. Raw JSONL SHA256: `1614195A1BEC21050618EF13F463553544E5863AF32EE70041702E9023323B6E`. The user's raw file is NOT uploaded to the repository: it contains private endpoint/profile data.

Both DACs remained RUNNING, 8 channels at 192 kHz, actual WASAPI buffer 1024, FIFO capacity 65536 and target 4096. MMCSS was registered. No interval increments in pipeline xruns, resyncs, work-duration deadline misses, estimated late submissions, PCM overload/clipping/nonfinite counters, or source-wait/resume counters. This interval does not support blaming measured CPU overload or PCM clipping for the persistent roughness. It does not prove hardware/analog continuity.

Applied ppm varied approximately -120..+0.6 (A) and -116..+3.6 (B); published sink-rate estimates reached approximately 192024 Hz. Internal post-render fill stayed about 3072..3073 frames, while the preceding FIFO stayed about 4244..5327. The internal fill is sampled AFTER a 1024-frame render, so 3072 does not establish an erroneous 1024-frame deficit against the 4096 PRE-render controller target. No target correction is made from that subtraction.

The approximately 12 ppm largest sampled steps are BETWEEN 5 Hz cached snapshots, not per-callback delta. Actual callback delta maxima and separate WASAPI timeout counters were not exposed in R2. The R2 JSONL contains configuration and level observations, not raw PCM or every hardware-clock observation. Earlier isolated bypass listening and this telemetry configuration must not be treated as the same measurement.

## Finding, hypothesis, and change

R2 fits hardware position against QPC over a rolling four-second window at callback cadence, accepting a fit by span/monotonicity and a broad rate bound. A timestamp phase offset can therefore be interpreted as a short-lived oscillator rate change. The observed ppm/rate excursions justify investigating this sensitivity, but do NOT establish whether the physical DAC rate, driver timestamping, or some other boundary actually changed.

A deterministic sensitivity probe uses a constant-rate synthetic clock, block-quantized positions, a coarse 1 ms timestamp, and a 1/3 ms timestamp phase step after 48 seconds. This is an explicit model, NOT a replay of missing real callback data. The actual R2 estimator reports about 125 ppm peak rate error. R3 reports about 14.7 ppm in the same probe. These bounds are engineering acceptance criteria for this candidate, not perceptual thresholds or a proof of the user's cause.

R3 changes the oscillator estimator's observation bandwidth:

- Preallocated approximately 100 ms buckets average valid position/QPC observation pairs. These are small clock records, never PCM/audio buffers.
- Acquisition starts with a multi-second span (3.9 s minimum fit span); measured synthetic acquisition is about 4.05-4.11 s. The regression window then grows to 32 s, using at most 512 preallocated bucket records.
- The long-window estimate is smoothed with a 4 s time constant. Initialisation uses the first valid measured estimate, not a forced nominal 192000 Hz. Valid estimates are retained between bucket completions. Short-window and long-window raw estimates remain observable.
- Integer anchors are subtracted before conversion to floating point to avoid losing low bits at long uptimes, including when MSVC long double is FP64.
- Invalid/nonmonotonic observations do not become fabricated valid samples. Explicit discontinuity or a gap over 500 ms clears the observation window and reacquires. No new PCM drop/duplicate/reset/mute policy is introduced.

The trade-off is slower response to a genuine abrupt rate change. In a separate synthetic -180 to +180 ppm step, integrated estimator error reaches about 1384 frames before convergence, and final rate error approaches zero. This is an observer-only calculation, NOT proof that every real FIFO/driver scenario is safe. Existing overflow/starvation/recovery gates remain unchanged.

**32 seconds is the growing clock observation horizon, NOT added audio latency.** No PCM queue, target, latency compensation or delay parameter is enlarged. The existing ARM calibration still depends on live clock readiness; real startup timing is not measured here.

## Unchanged signal path

The original RatioController numeric implementation is retained as updateImpl/resetImpl with passive wrappers. Its requested correction law, 50 ppm/s slew, backlog term, and configured correction limit are unchanged. 50000 decision sets match R2 bit-for-bit when BOTH controllers receive identical observations and estimates. This does not imply R2/R3 live PCM is bit-identical: the observer now intentionally supplies a different filtered rate, hence a different ASRC read trajectory.

The 96-tap sinc table/interpolator, sample-by-sample read-position integration, nominal SRC, PCM24/dither, Headroom in all modes, Crossover/PCA/FIR/delays, OutputMatrix, Graph/input queue, scheduling/MMCSS, WASAPI negotiation/timeout, startup presentation alignment, DAC FIFO capacities/targets, Stage1/Stage2, driver settings, profiles and Qt sources are unchanged.

## Passive telemetry additions

Existing fields are retained. New fields include:

`applied_ppm`, `requested_ppm`, `delta_applied`, `max_abs_delta_applied`, `max_abs_applied_ppm_per_second`, `max_reset_jump_ppm`, `ratioResetCount`, `ratioObservationCalls`.

The delta is calculated on the actual ratio-owner call. Its maximum excludes reset jumps; reset jump maximum and count are separate. Counters/maxima belong to the lifetime of the controller object, including its initial prepare/reset. Observation uses bounded atomic snapshots; check `ratioTelemetryCoherent`. The old `ppm` and new `applied_ppm` can be captured at slightly different instants; do not infer a defect from their last-bit difference.

`clockShortWindowHz`, `clockLongWindowHz`, `clockFilteredHz`, `clockObserverWindowSeconds`, `clockBucketResidualRmsFrames`, `clockFitCount`, `clockAcceptedObservations`, `clockRejectedObservations`, `clockObservationFrame`, `clockObservationQpc100ns`.

Check `clockObserverTelemetryCoherent`, `clockObserverValid`, `clockShortWindowValid`, and `clockLongWindowValid`. The published frame/QPC pair identifies the observation that completed the last bucket; it is not an every-callback recording. Fit validity describes the last published fit, not approval of every subsequent observation. Accepted/rejected totals may be newer than that fit snapshot. A residual RMS is an observation statistic, not audible jitter or an xrun counter.

`wasapiXruns`, `wasapiTimeouts`, `wasapiDeviceLosses` forward the separate sink's existing counters. `wasapiXruns` currently counts detected wait-timeout xruns; it is NOT an exhaustive detector of all driver/hardware discontinuities. It is not substituted for the existing pipeline `xruns`. These observations do not steer playback.

`recoveryRevision=R3` identifies the new egress observation code. Some older Core/UI version text remains r3. The package manifest, Git SHA, binary hash and startup banner identify the complete R3 candidate.

## Verification actually executed

Exact baseline Git blobs were checked before editing. In an isolated Linux partial source copy, GCC 14.2 and Clang 17 compiled the real estimator and ratio implementations with C++20, -O2, disabled FP contraction, and warnings enabled. The numeric config declaration used for the ratio fixture matches the repository's EntropyEgressClockConfig fields, but the fixture is not a full Shared/Windows ABI integration build.

- R2 failed precisely the three new phase-noise criteria (256/1024/2048 frames, peak errors 124.956/124.997/125.493 ppm). Its existing macro-disabled unused gapRebased warning was retained as a warning for that baseline-only compile, without modifying baseline source.
- R3 passed those criteria (14.705/14.671/14.701 ppm), constant real drift -180/-70/0/+100/+180 ppm, unchanged producer-counter influence, large integer anchors, invalid observations, discontinuity and >500 ms reacquisition.
- A real 360 ppm synthetic rate step converged; the observer lag budget, no C++ allocation during observe/quality, and concurrent passive telemetry checks passed.
- 50000 real ratio decisions were identical to R2 under identical inputs, including invalid-estimate and reset cases. Per-owner-call delta maxima, the 50 ppm/s slew observation, and concurrent ratio snapshots passed.
- The focused tests also completed under Clang AddressSanitizer + UndefinedBehaviorSanitizer with exit 0 and no reported sanitizer diagnostics. No ThreadSanitizer result is claimed for R3.

Reproduction in a complete clone: `python3 Tests/RecoveryR3Portable.py --check-baseline`. It reads R2 from existing Git objects, downloads/installs nothing, and does not run the rest of the suite. The --baseline-directory / --config-only-fixture options reproduce the explicitly limited partial-copy environment used here.

The complete Windows/MSVC/IPP link, real named-pipe telemetry, physical DAC output, original CTest suite, all diagnostic-macro build combinations and PowerShell packaging were NOT executed here. The PowerShell script and forwarding changes were reviewed; local compilation remains necessary. These checks are not an audio-quality verdict.

Primary API reference (units and semantics, not a diagnosis of this driver): Microsoft IAudioClock::GetPosition documents the paired device position and QPC in 100 ns units, interpreted with GetFrequency:
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition

## Local build, listening and rollback

Use the existing `D:\Projects\Entropy-Stage3-Recovery-20261013\source` checkout. Fetch/fast-forward to the exact SHA supplied in the handoff, then run:

```powershell
& .\tools\Build-Recovery-R3.ps1 -ExpectedCommit '<FULL_R3_SHA>' -Profile 'C:\Users\bobi\Documents\entropy.entropy'
```

The script builds only Core 64k and its existing offline --probe, checks R2 ancestry and an exact changed-path whitelist, verifies the untouched R2 Core/UI hashes, and creates a NEW `D:\Projects\Entropy-Stage3-Recovery-20261013\live\R3-64k`. An existing destination is refused. Packaging failure leaves INCOMPLETE.txt. It copies the saved profile used by the observed R2 session, without rewriting parameters or assuming unsaved UI changes. No CTest, benchmark, new capture, DAC access or process control is part of Codex's task.

After a successful package, the owner DISARMs R2, closes its Qt UI, then stops that Core using R2's own Stop-AudioCore.cmd. Stage1/Stage2 are unchanged. Start R3's Live-Test.cmd, verify the same saved mapping/gain setup and ARM manually. Keep driver buffer, passages and upstream processing fixed for the comparison. Evaluate both initial playback and after at least 40-45 s of continuous playback, when the longer observation window has filled. Do not interpret the observation horizon as an audio delay.

If R3 is worse or faults, DISARM it, close its UI, stop it with its own Stop-AudioCore.cmd, and start the preserved R2 package. Do not replace an installed executable in place. Report whether the audible roughness changed; lower ppm variation alone is not proof that the sound is repaired.
