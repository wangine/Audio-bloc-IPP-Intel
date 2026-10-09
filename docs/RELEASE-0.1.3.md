# AudioCore 0.1.3 — beta test: PCM24 evidence

Rectification 05 adds bounded offline diagnostics, reproduction scripts and
measured evidence. Production Core and Qt binaries are identical to the
published 0.1.2 package. No DSP, PCM24, ClockSync, Headroom or UI algorithm
changes; this is not a clipping fix or a new listening acceptance.

PCM24 TPDF_FLAT / PRODUCTION_INDEPENDENT / CLIP_AND_REPORT configuration:
PASS. Sub-rail conversion: PASS, maximum error 1.488792 LSB, RMS 0.499623 LSB.
Actual live measurement used the unchanged 0.1.1 engine: DAC A saturation FAIL,
488,853 overrange samples and 1,068 clipped blocks in approximately 16 seconds.
DAC B had zero new clipping, with its historical counters retained.

Offline overload already exists before Headroom: DAC A peak 2.754746 FP64.
BITPERFECT preserves it. An explicit private MANUAL SAFE -12 dB comparison
passes that same capture, peak 0.691961 and zero clipping. It was not loaded
live, is not included as a default, and is not a universal headroom policy.
The user's live profile was unchanged. Analog attribution remains BLOCKED
without calibrated measurement. See docs/STAGE014-PCM24-ULTRASONIC.md and
validation/stage014 for full measurements, limitations and source hashes.

Historical Stage04 realtime FAIL, prior finite-overrange-policy assertion FAIL,
PCA transparency FAIL and formal V07 acceptance BLOCKED remain visible.
No broad test suite or benchmark was rerun for this diagnosis.

Runtime Core SHA256 (identical to 0.1.2):
7842EC34F359B7F78AAB1CF84B147E303FAE82CD4E571D2E96B76A96DAC2BBA3

Each version has a separate release folder. Existing references, Stage1 and
Stage2 are unchanged. The package does not contain private profiles, raw music,
Intel SDK/libraries or build directories. Complete source and reproduction
scripts are available from this release's Git source archive/tag.

For an intentional version switch, save your profile, close Qt and use the
previous version's Stop-AudioCore.cmd. Leave Stage1/Stage2 running. Start this
folder's Live-Test.cmd, load your profile and explicitly ARM. Closing Qt leaves
Core running. No automatic ARM, source restart, device assignment or attenuation.
