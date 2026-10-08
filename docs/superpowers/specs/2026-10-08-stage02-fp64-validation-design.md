# Etapa 02: FP64 AudioGraph validation and performance

Date: 2026-10-08
Branch: codex/stage02-fp64-validation
Baseline: 019e3aa04cbe8e5f4c405568ee09f971a4a1e3cd
Status: approved and implemented; see [executed results](../../VALIDATION-STAGE02.md).
Subsequent explicit authorization permits publication on the Stage02 branch only,
without binaries or a merge into main; the original execution boundaries below remain historical.

## Purpose and boundaries

Validate the offline Crossover IPP -> Output Matrix -> Headroom graph at 192000 Hz, native C++20, planar FP64, 13 inputs and 16 outputs. Produce reproducible numerical measurements, CPU benchmarks and a realtime safety assessment. An unexpected all-zero result must fail. Existing Stage 01 tests remain enabled.

Do not modify any imported DSP or Egress source, filter mathematics, default production policies or algorithms. Do not modify stable Stage3, launch DACs, connect Stage2 or Qt, push, merge, or implement an alternative FIR. Test fixtures, Engine integration, CMake and offline tooling are the allowed change surface. Report defects without masking them. Stop after delivering results.

## Reference strategy and validity

Use pristine source references at CrossoverIPP commit 99b84d13a6808486690e24feba35128d7675be95 and other native modules commit 6cbac37233c8094e39828de8309a9fe13d724ad4 from wangine/RenderEngine-Stage-3. Acquire these in an isolated local reference checkout, never by changing Stage3. Compare source hashes and build the reference modules without edits. Record commit, files, hashes, compiler and build flags.

Compare AudioGraph to separately instantiated reference Crossover, Matrix and Headroom with the same settings and sample-accurate input/parameter schedule. Imported-source identity plus this comparison validates integration; it does not independently prove the correctness of the shared DSP mathematics. Use independent identity, gain, delay, impulse-response and filter transfer checks to address this limitation.

Crossover V07 commit 2d5b35165de2ac6be5afa9420851fc24895c6416 is an additional external golden reference. Verify the provenance and availability of its executable adapter before using it. Never load an unverified stable runtime DLL as a golden oracle. If a verified runnable V07 reference is unavailable, explicitly report this comparison as BLOCKED and withhold overall golden-equivalence PASS. Do not substitute the IPP implementation for V07 or import JUCE into AudioCore.

## Offline configuration access

AudioGraph currently has private module instances and a fixed prepare fixture. Add the smallest offline validation interface needed to apply existing Crossover parameters and service coefficient updates on the control side between callbacks. Expose no hardware, IPC or scheduling behavior. Use the same production AudioGraph processing path, not a duplicate graph implementation hidden inside the test. Matrix/Headroom state may be supplied through an offline configuration fixture; preserve current prepare behavior for existing callers.

All creation, configuration, coefficient compilation, buffer sizing and file writing occur outside callback measurement scopes. Parameter publication and subsequent callback adoption have distinct timing scopes. Test-owned reference instances remain separate from graph instances.

## Numerical coverage

Exercise sine, impulse, logarithmic sweep, multitone and seeded deterministic noise. Store the seed and generator definition. Use low-level inputs avoiding protection trips unless deliberately testing mute/protection. Include channels with distinct deterministic signatures to detect routing and channel swaps.

Use frequencies 10, 20, 40, 80, 160, 315, 630, 1000, 2000, 4000, 8000, 16000, 24000, 40000 and 60000 Hz. Sweep and multitone cover 10-60000 Hz. Publish filter passbands and expected active outputs for each fixture; attenuation in a configured stopband is not confused with unexplained silence.

Run every stimulus under MUSIC and CINEMA, IIR, FIR and All-Pass fixtures, with blocks 1, 7, 31, 64, 127, 256, 511 and 1024 samples. FIR coverage includes 2049, 4097 and 8193 taps. All 16 outputs must be exercised with explicitly enabled topology and appropriate input roles; report each output separately. Test All-Pass with IIR and FIR where supported.

Include gain changes (0, -6 and +6 dB), zero and nonzero integer/fractional delay, channel mute/unmute, emergency mute, topology changes, MUSIC/CINEMA changes, IIR/FIR transitions, coefficient changes and FIR length changes. Apply events at fixed absolute sample boundaries shared by both paths; split calls at event boundaries when necessary. Include updates during an existing transition. Document any unsupported configuration and mark it BLOCKED rather than silently omitting it.

Use identical pre-roll and histories. For steady sinusoidal measurements, exclude settling and observe at least 10 periods at the lowest tone, with pre-roll covering configured delay, FIR history and parameter ramps. Impulse capture includes complete filter support and delay, with IIR tails recorded until the selected analysis window ends. Sweep/noise transients are compared sample by sample without arbitrary realignment.

## Metrics and numerical verdicts

For each case/output report maximum absolute difference, RMS difference, reference/candidate peak and RMS amplitude, absolute/relative gain error, finite sample count, expected activity, first significant impulse sample and measured lag. Use compensated or extended-precision metric accumulation where available.

For same-backend reference comparisons, use maximum absolute error <= 1e-12 + 1e-11 * reference peak and RMS error <= 1e-13 + 1e-12 * reference RMS. Report bitwise match separately. Never relax tolerances after observing a failure without documenting and obtaining approval for the change.

Matrix identity and Headroom bit-perfect/unity fixtures require exact FP64 sample identity, including finite edge values, signed zero and amplitudes above unity that must not be clipped. They must not introduce delay. Test each module independently as well as both after Crossover.

Estimate sine amplitude and phase by fitting sine/cosine/DC terms at the actual test frequency, avoiding FFT-bin assumptions. Report reference phase, candidate phase and wrapped phase difference. Mark phase undefined when reference amplitude is below 1e-10 full scale. FIR impulse peak/group delay and first significant response are distinct from added graph latency. Graph must add zero samples relative to the reference chain.

Require expected passband activity above an explicitly recorded noise floor. Check expected routing, polarity, gain and delay with simple analytic fixtures. For muted fixtures require silence after the specified ramp, and recovery after unmute. A zero reference and zero candidate cannot pass an expected-active fixture. Report intentionally inactive outputs as such.

V07 comparisons use original published validation thresholds if present. If these thresholds cannot be established, publish raw metrics and mark golden numerical acceptance BLOCKED rather than inventing a PASS threshold.

## CPU benchmark

Measure Crossover, Matrix, Headroom and full AudioGraph separately for 128, 256, 512 and 1024 frames at 192000 Hz. Include identity, representative IIR/All-Pass, FIR 8193 on all enabled outputs, and filter/program/length transitions. Include emergency mute and topology reduction callbacks to quantify bulk-reset spikes.

Use Release x64, the same compiler/FP settings as Stage 01, QueryPerformanceCounter, preallocated input/output/timing storage and no reporting inside the timed interval. Report observed CPU model, processor features, IPP version/dispatch information, timer resolution, warmup and sample count. Use at least 2000 warmup callbacks for steady cases and 10000 measured callbacks per case; separately collect at least 200 transition/reset events after restoring a defined state.

Report median, nearest-rank p99, maximum observed, callback budget (frames / 192000 seconds) and budget utilization. Measure and report timer overhead without subtracting it from the raw results. Keep control-side coefficient generation timing separate. Maximum is an observation on this PC, not a hard realtime bound. Deadline verdict is FAIL if any observed callback exceeds its budget; report scheduling/environment limitations.

## Realtime safety

Count allocations and deallocations only during process scopes, covering scalar/array/aligned/nothrow C++ operators. Cover startup processing, steady-state, transitions, mute/unmute and topology reduction. Audit malloc/free, aligned CRT/Windows heap and IPP allocation paths as well; C++ counters alone cannot establish zero total heap activity. Any uninstrumented allocator path is explicitly unverified.

Audit the complete callback call graph for mutexes, waits, filesystem, logging, lazy IPP initialization and dynamic sizing. Combine dynamic measurements and source evidence; state the limits of each. Verify IPP plans/dispatch warmup occur off callback. Require no NaN/Inf, no hidden gain/clipping and no unexplained latency.

Known source evidence: Crossover processBlock clears full delayBuffer/delayEpochs and resets FIR history on first emergency mute/topology disable; mode adoption can also reset FIR history. Record affected sizes, call sites and measured callback spikes. Preserve these routines and report the bulk-reset requirement as FAIL while this behavior remains. Do not transform a numerical PASS into realtime PASS.

## CPU compatibility

Do not add mandatory /arch:AVX2 or FMA3. Record compiler flags and actual PC capabilities; do not claim execution on an older CPU when measured on this PC only. Inspect actual IPP version/dispatch and primary Intel documentation for minimum supported ISA and any version-specific limitations. Separate compiled baseline ISA, backend dispatch support and hardware actually tested.

Provide a recommendation for a future native SSE2 FP64 fallback preserving FIR taps, normalization, phase, latency and transition behavior, with differential acceptance gates. Do not implement it in this stage or assume the installed IPP version supports SSE2 without evidence.

## Deliverables and stop gate

Retain offline harness source and CMake integration on the validation branch. Preserve existing tests. Store machine-readable numerical/benchmark results and full build/test logs outside callback scopes. Deliver a concise Markdown report with coverage totals, per-category PASS/FAIL/BLOCKED, numerical differences, benchmark tables, realtime findings, compiler/IPP/CPU information, source provenance and executable SHA256.

Report each unmet requirement explicitly. An incomplete reference comparison or unverified realtime scope prevents overall PASS. Re-run Stage 01 CTest and --probe along with new validation tests. Verify DSP/Egress source hashes remain unchanged and no stable Stage3 files were written by this task. No push or merge. STOP after the report.
