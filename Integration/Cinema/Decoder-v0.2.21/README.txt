Entropy Surround Decoder v0.2.21 - local validated DTS reference

Preserves original FULL AUTO, stereo 5.1/7.1 upmix, GUI/tray/autostart,
UR23 capture, FFmpeg, DTS LFE calibration and legacy float32/48 kHz ring.
Adds independent StagePublish V2 CINEMA planar FP64/192 kHz output:
FL FR FC LFE SL SR [BL BR]. Native DD/DTS is 5.1; stereo 7.1 remains
the existing manual upmix mode. One WDL sinc 128-tap SRC owns all phases.
No gain compensation. Synthetic recovery padding is not CINEMA PCM.

The installer uses the exact original v0.2.13 wrapper/lifecycle with updated
version and resources. Upgrade preserves Start with Windows. Uninstall is
only needed to remove the application; do not uninstall before upgrading.

Stage 3 must select its existing CINEMA input manually. Crossover and
speaker protections must stay active. DTS live listening and installed
production acceptance passed for the documented intervals. Stage 1 and Stage 2 are unchanged. Stage 3 r3 is
the separate startup-reference correction used in this investigation.
v0.2.15 replaced coarse pacer/publication waits with high-resolution timers.
v0.2.16 registers the capture owner with MMCSS Pro Audio / CRITICAL.
v0.2.17 prepares the FFmpeg child before starting WASAPI, then adopts the first
codec without recreating an unused child while holding a capture buffer.
Later recovery conditions are preserved. DSP, SRC, capacities, gain, capture
period and the existing pacing deadlines are unchanged.
v0.2.18 removes Shared-only GetNextPacketSize from Exclusive capture, using
GetBuffer/ReleaseBuffer directly. This contract correction DID NOT eliminate
the repeatable ~163-second Windows capture discontinuity.
v0.2.19 reports that warning but does not restart a healthy encoded stream solely
because of it. Real codec/lock/liveness and PCM recovery policies remain active.
See docs/VALID-CARRIER-RECOVERY.md and docs/EXCLUSIVE-CAPTURE-CONTRACT.md.
v0.2.20 registers the decoded-PCM pacer in MMCSS Pro Audio / CRITICAL.
v0.2.21 also registers the downstream SRC/publication owner in the same task.
Neither change alters SRC, queue sizes, deadlines, filters or clock calculations.
See docs/PACER-SCHEDULING.md for the red/green evidence and acceptance limits.
Full build, three targeted tests, installed-file hashes and real dual-DAC DTS
trials passed. Maximum publication gap: 8.8166 ms instrumented / 8.6162 ms
installed / 8.5955 ms with original GUI and RAW trace, below 21.3333 ms target.
The user confirms correct sound and a monitor switch during the three expected
production source waits. No DISARM or DAC fault occurred. See the verdict for
capture hashes and limits; AC-3 startup and acoustic latency remain unqualified.

Build: tools\Build.cmd (VS 2026, CMake, Ninja, local D:\Projects\JUCE).
Package: tools\Package.py on Windows (bundled Python 3.12).
Focused tests: ctest --test-dir build -R "CinemaCapturePolicy|CinemaOutputTransport|CinemaProducerRetirement" --output-on-failure.
Original installer and installed-file backup are retained for rollback.


