# PCM pacer and publication scheduling: v0.2.21 DTS reference

v0.2.19 fixes the demonstrated false encoded-stream restart caused solely by a
Windows DATA_DISCONTINUITY warning. It is not a stable end-to-end release:
its 420-second DTS trial still had two unexpected Stage 3 source waits.

The retained trace places the remaining interruption after FFmpeg decoding.
The PCM pacer had a 78.6059 ms interval while 6,912 decoded frames remained
queued. Publication had a 72.1163 ms maximum gap and 41 intervals exceeding
the unchanged 4,096-frame / 192 kHz sink reserve (21.3333 ms). No decoder
restart, decoded discard, pacer underflow or gate loss explains that gap.

v0.2.20 registers the PCM pacer thread with the existing Pro Audio MMCSS
RAII guard and AVRT_PRIORITY_CRITICAL. Registration and cleanup occur on
the owning thread. Registration failure invalidates the Cinema source and
stops the engine rather than escaping from the worker thread.

The scheduling change does not alter queue capacities, prebuffer targets,
pacing intervals, clock calculations, SRC, DSP, gains, filters, speaker
protection, GUI topology, Stage 1 r9, or Stage 2 v0010.

The v0.2.20 420-second carrier capture is complete: 42,011 packets, one Windows
warning, zero frame-index gaps, one unchanged generation, no decoder recovery,
underflow, gate loss, drop or decoded discard. All 39,386 DTS headers have exact
2,048-byte spacing. Strict offline FFmpeg decoding exited zero. Carrier SHA256:
d57d2e21a4ebdee990c756e5f5d2ba34d34c066018235480b401ddd26dbf0a2f.

The 400-second DAC monitor had zero unexpected source waits, fault, xrun,
ClockSync reset, group replan or deadline miss. Pacer maximum interval fell to
4.9286 ms. Publication nevertheless had two gaps over the 21.3333 ms target,
maximum 26.5053 ms. During those two gaps the pacer submitted 1,280 and 1,024
frames, with maximum intervals 3.0832 and 3.0558 ms respectively. Publication
sequence remained consecutive. Thus this remaining deadline failure is after
the timely pacer, in the downstream processing/publication owner.

v0.2.21 registers that owner using the same MMCSS guard; failure retires the
source, sets schedulingFailed, stops publication and returns. avrt linkage is
propagated to all consumers of the static CinemaOutput library. No queue, SRC,
deadline, clock, DSP or GUI policy is changed.

The acceptance trials are complete for the documented DTS intervals. The
420-second instrumented trial exercised a Windows capture warning without
recovery; maximum publication gap was 8.8166 ms. The installed production
trial measured 8.6162 ms and preserved three real source waits during the
user's confirmed monitor switch, without losing logical ARM. The original
GUI plus complete RAW trace measured 8.5955 ms, with no source waits during
its 400-second DAC observation. All three publication maxima remain below
21.3333 ms, with zero DAC faults or DISARM. No build ran during these captures.
See CINEMA-DISARM-VERDICT.md for counts, hashes, clipping observations and
the separate AC-3 startup and acoustic-latency limits.

The existing PCM reader also owns pcmMutex_ at ordinary priority; MMCSS
registration alone is not a proof against every possible priority inversion.
The measured publication trace and actual DAC state determine acceptance.

Evidence directory: D:/Projects/3Stage-3/Diagnostics/Cinema-Disarm-20261012

- v19-dts-decoder-trace.csv / v19-dts-timing.csv
- v19-dts-live-telemetry.jsonl
- AnalyzeOwnerTrace.py / CheckPublicationDeadline.py
- v20-dts-decoder-trace.csv / v20-dts-timing.csv
- v20-dts-live-telemetry.jsonl / v20-dts-carrier-48000-s16le-stereo.raw
- v21-dts-*, v21-installed-*, v21-gui-* / v21-gui-trace-*
