# Capture scheduling hypothesis and local acceptance

v0.2.15 corrected coarse waits. Its paired diagnostic trace still recorded 12
WASAPI discontinuity flags and two FFmpeg recovery cycles, with no pacer
underflow, publication drop or decoded backlog discard. The first flagged
packets followed 27.5379 ms and 20.8215 ms owner arrival gaps at a 10 ms endpoint
period. Synchronous recovery then held capture processing for 102.1807 ms and
103.9774 ms. Device frame positions remained consecutive; that observation
does not certify the actual carrier bytes or justify suppressing flags.

The capture owner had no MMCSS registration. v0.2.16 registers that owner with
Pro Audio / CRITICAL before WASAPI starts and reverts registration when the
owner returns. Admission failure is an explicit startup error. This changes
only thread scheduling; existing capture mode, period, byte handling, recovery
conditions, timers, queues, codec calibration, SRC and GUI topology remain.

The hypothesis is that an ordinary-priority capture owner misses endpoint
deadlines while other audio owners run. MMCSS is the bounded correction to
test, not proof that the USB driver or source has no defect. Microsoft documents
MMCSS for threads accessing endpoint buffers:
https://learn.microsoft.com/en-us/windows/win32/coreaudio/user-mode-audio-components
and demonstrates registration in its capture sample:
https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/multimedia/audio/CaptureSharedEventDriven/WASAPICapture.cpp

The live acceptance script CheckCaptureContinuity.py fails on the v0.2.15
reference (12 discontinuities, two recoveries). Repeat the same live Cinema
passage with one reader, preallocated diagnostic trace, unchanged DAC profile,
and Stage 3 r3. Require no new discontinuities/recoveries/underflows/drops, then
verify the installed production binary. Do not upload as stable before that.
