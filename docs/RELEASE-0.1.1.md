# AudioCore 0.1.1 — beta test: Headroom render mask

Separate candidate for rectification 3. The accepted listening reference remains
AudioCore 0.1.0 + Stage1 passive v0002 + Stage2 v0008. It was reported by the user
to sound good, with clean highs and no perceived artifacts. That observation is
not carried forward as a listening PASS for this candidate.

The Headroom detector now observes only configured DAC lanes with valid Matrix
contributors. Its mask does not change merely because output is DISARMED. One
linked Headroom group remains; incompatible group commands return explicit
errors. AUTO safety gain-step, one-second hold, attack/release and manual gain
mathematics are unchanged. No visual Qt changes or hidden attenuation.

Launch `Live-Test.cmd` to connect the unchanged Qt UI to this Core. A newly
started Core is DISARMED; load your profile and explicitly ARM from the UI.
Stage1 and Stage2 remain independent processes. This candidate does not restart
or configure them. `Cold-Start.cmd` starts the normal session without the live
test authorization flag. Closing Qt leaves Core and its current state running.

Validation: 36/37 configured tests PASS. The retained `EntropyStage04Offline`
FAIL expects the old finite-overrange rejection policy, superseded by PCM24 V2
clip-and-report. It is not hidden or disabled. Historical Stage04 realtime
qualification remains FAIL; formal V07 acceptance remains BLOCKED. Explicit live
test authorization is distinct from those gates.

MANUAL -12 dB produced an exactly constant FP64 scalar on two real six-second
fragments. Original AUTO capacity -3 dB was insufficient on the louder fragment;
the FAIL and overrange counters are retained. An explicit AUTO -12 dB comparison
copy passed with zero PCM overrange and exhibited the original gain dynamics.
Analog level matching and the new candidate's listening assessment remain
pending. See `docs/STAGE012-HEADROOM-MASK.md` and `validation/stage012`.

No true-peak detector was added. Sample analysis occurs before DAC ClockSync
interpolation and cannot guarantee post-interpolation peak safety. Check the
existing PCM overrange/clipping telemetry during live playback.
