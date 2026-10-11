# Stage3 source-wait implementation plan

User direction: Stage3 must remain logically ARM while Stage1/Stage2 are stopped, or while selected Cinema is absent. Wait muted indefinitely, including ten hours. Do not fabricate source PCM blocks or feed zeros through the Graph. Resume only validated source PCM. Keep ordinary DAC/module faults distinct. No Git upload until a local candidate passes live testing.

This changes local lifecycle ownership, not DSP algorithms. Original accepted releases and pinned upstream source folders remain immutable. New runtime candidates go in fresh directories. Stage1 r7 is active and its five-minute real test is saved independently.

## Confirmed causes and current work

- Stage3 DomainIngress latched every required-input fault and permanently stopped reconnecting. ARM required Receiving, making recovery impossible without Core restart.
- A real isolated stop/restart regression failed on the old behavior. Local DomainIngress now retries after retiring subscriptions, retaining strict session/finite-PCM checks and fault counters. This fix passed the new regression and current MUSIC/CINEMA tests. Full 64k CTest: 52/53; inherited `EntropyStage04Offline` still fails `unsafe PCM rejected before converter`, consistent with historical package reports. Never report 53/53.
- Normal source absence also drains DAC FIFOs. Existing egress treats starvation/input loss as output fault and closes ARM. This is the remaining requested change.

## Selected local design

Separate user ARM intent from current physical playback. A lifecycle-owner SourceWaitPolicy tracks intent, muted waiting, wait/resume counters, and failed resume. It uses existing egress stop/prepare/start on the correct owners. While waiting, native DAC owners are stopped/closed; the user ARM intent remains set, Core/ingress/control stay alive, and no PCM is manufactured. On valid input and settled safe DSP policy, resume with existing per-DAC prepare/preroll and fresh timing. Do not conceal the stopped physical state in telemetry.

Distinguish source-induced hold from fatal egress failure: a scheduler input loss explicitly requests source hold; DAC FIFO starvation is an input-supply hold. Clock/device/PCM faults and Graph/module-policy failures retain fatal DISARM. A fatal bit always overrides source hold. Input faults/gaps remain visible in input history. No blanket clearing, limiter, gain, mute ramp, synthetic Graph PCM, or mutation of original clock-controller law.

Core telemetry: `armed` means user ARM intent during source wait; `physicalArmed` reports actual egress; `inputMuted` and lifecycle `WAITING_INPUT` make the distinction explicit. UI shows ARM retained + MUTE/WAITING; it must not imply physical audio remains active. Explicit DISARM cancels intent, pending resume, and output publication promptly. No automatic initial ARM: resumption is only within previously requested ARM. Initial ARM with no selected PCM may wait after profile/device/FIR validation; headroom/settling gates must pass before opening physical playback.

## Tasks

- [x] Reproduce and fix required-input permanent latch; preserve RED/GREEN logs.
- [x] Add actual policy regression with injected sink owners and real Source/ProgramIngress/AudioScheduler: ARM → source stop → indefinite muted waiting (no Graph blocks or submitted false PCM) → source restart/new UUID → resume same Core/user ARM → explicit DISARM while waiting cancels resume. Retain real DAC fault regression.
- [x] Implement SourceWaitPolicy and explicit source-hold classification in LiveSinkWorkers; integrate CoreRuntime and lifecycle state; truthful Qt presentation.
- [x] Run relevant MUSIC/CINEMA, lifecycle, egress and UI tests; retain inherited failures separately. Verify protected DSP hashes.
- [x] Prepare fresh local Stage3 candidate(s), no replacement of accepted 0.1.9-v2.
- [x] Live test with Stage1 r7 stop/restart, Stage2 stop/restart, unavailable Cinema, explicit user DISARM, real input recovery. Capture PID continuity and source/session/timing/counters; never claim original TIDAL PCM byte identity from telemetry.
- [x] Package source/runtime/evidence locally after validation. No Git commit/push until user-authorized live acceptance condition is met; current user instruction is no upload yet.

## Concurrency and cancellation

Policy service and output reconfiguration run only on LifecycleExecutor owner. User intent is atomic because DISARM admission can cancel from the IPC reader. Resume uses existing lifecycle generation/activate guard; cancellation during preparation must not reopen playback. Fault probe must not auto-DISARM an already-classified source hold before the policy owner finishes it. Existing DAC fault bits and counter history must remain observable; do not classify all Input failures as source loss.
