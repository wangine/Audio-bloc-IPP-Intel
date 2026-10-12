# Recovery at a reported Windows capture discontinuity

Status: candidate built; physical DTS validation in progress. No stable Git push.

The capture owner restarted FFmpeg on DATA_DISCONTINUITY alone. Valid encoded
packets therefore acquired a new Cinema generation and Stage 3 reacquired its
source, causing about four seconds of mute. This is separate from genuine DAC
or clock faults and from AC-3 startup priming.

The retained DTS Type I carrier contains 35,544,960 bytes, 18,513 packets and
17,356 IEC61937 headers, all spaced exactly 2,048 bytes. One capture warning was
at device frame 8,009,760, raw byte offset 32,039,040. No captured bytes, zeros
or headers were removed for analysis. Strict offline FFmpeg decoding using
-err_detect explode -xerror succeeded across the entire signal.

Carrier SHA256:
70c6e3e93a6ef89202bb83a03382035f526f52ad6f5a09a03d89505c69777e81

An offline causal control feeds these exact bytes through the same FFmpeg
low-delay/probe arguments either continuously or with a forced child reset at
the marked packet. Continuous decoding yields 8,885,248 audio frames (6 channels);
the two reset pieces yield 8,008,704 + 875,520 frames. Reset loses 1,024 frames
(21.333 ms at 48 kHz), before queued PCM retirement and source reacquisition.
All three offline decodes exit zero. This proves artificial reset loss for this
retained carrier; it does not prove identity to its master.

v0.2.19 reports captureWarnings but does not treat that flag alone as an encoded
codec/stream boundary. The same FFmpeg process consumes the carrier. Lost burst
lock, codec change, stalled decoded progress, child exit and pipe-write failure
retain existing recovery behavior. PCM discontinuity handling, source gating,
nonfinite handling, calibration, clamp, crossover and speaker protection remain.
No gain, filter, SRC, FIFO capacity, pacing deadline, driver or clock change is
made by this recovery correction. Stage 1 r9 and Stage 2 v0010 are untouched.
The original GUI, topology, installer and autostart lifecycle remain.

Validation limit: brief carrier corruption which leaves burst lock and decoder
progress active does not force a reset solely due to a Windows warning. The
clean captured DTS stream demonstrates the observed case, not universal recovery
or integrity for injected missing/corrupt packets. Keep warnings visible and do
not describe a liveness check as bit-integrity certification.

Evidence: D:/Projects/3Stage-3/Diagnostics/Cinema-Disarm-20261012
- v18-dts-carrier-48000-s16le-stereo.raw
- v18-dts-boundary-analysis.txt / v18-dts-carrier-offline-decode.log
- dts-recovery-offline-control.json / CompareDecoderBoundaryReplay.py
- continuous-dts-recovery-red.log: one warning caused one v0.2.18 restart
- CheckNoUnnecessaryRecovery.py: exercised warning must cause no reset/loss
- v19-dts-decoder.log / v19-dts-live-telemetry.jsonl: current live trial

Require exercised warnings, unchanged generation, no unexpected source wait,
decoder restart, pacer underflow, publication drop/discard or live DAC fault
over seven minutes. Preserve the raw carrier and complete installed production
and listening verification before claiming stable.
