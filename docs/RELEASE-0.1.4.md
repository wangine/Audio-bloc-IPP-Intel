# AudioCore 0.1.4 — beta test: PCA Bass/Scoop correction

First version removing the artificial roughly +12 dB PCA reconstruction boost.
The former coherent-input measurements at 32/50/78/122/190 Hz were +8.53 to
+11.04 dB with variable phase. The summed envelope/carrier reconstruction has
been removed completely from PCA audio synthesis. On the same coherent fixtures,
the new Sub and linked Scoop are approximately 0 dB / 0 degrees, with only FP64
rounding error. This is not a claim that every possible programme peak is gone.

Complex PCA now estimates signed spatial weights only. Audio uses complementary
non-decimated FP64 subbands plus the fixed channel-average residual. Weights are
L1 bounded and smoothed over a 30 ms time constant. No hidden gain reduction,
limiter, AGC, RMS normalization or Headroom change.

PASS: targeted coherent unity/phase, impulse, antipolarity after coherent signal
and silence, selected feeds, independent MUSIC Scoop, CINEMA L/R/C Scoop and
direct L/R/C isolation. Build MSVC x64 / IPP and packaged Core/Qt offline startup
PASS. Test UI remains IPC-only and unchanged. Stage1/2 and other DSP modules are
unchanged. See docs/STAGE015-PCA.md and validation/stage015/numeric.json.

One original six-second fragment, with unchanged BITPERFECT Headroom:

| Measurement | Before | After |
| --- | --- | --- |
| SUB FP64 peak | 2.754746 | 0.813002 |
| SCOOP FP64 peak | 0.837429 | 0.263314 |
| DAC A pre-Headroom FP64 peak | 2.754746 | 1.479562 |
| DAC A PCM24 overrange samples | 379,313 | 1,310 |
| DAC A clipped blocks | 714 | 16 |

PASS PCA correction; FAIL remaining fragment PCM overrange. Historical Stage04
realtime FAIL and formal V07 BLOCKED remain visible. No broad suite or benchmark
was rerun. Listening/acoustic acceptance is not inferred from numeric results.

Implementation commit: 30577d0580f65efd04b590242ca37adbf2036cb3, based on v0.1.3.
Core SHA256: C1871A96B8CF3C45A11EBEABB235888436A3E4EB410F7A948B31735086605CD9.
The package contains Core/Qt runtime and offline evidence; complete sources and
reproduction code are in the Git source archive. No private profile/raw music,
Intel SDK/libraries or build directories. Earlier version folders stay intact.

Save your profile, DISARM, close Qt and stop only the previous Core with
Stop-AudioCore.cmd. Leave Stage1/2 running. Start this folder's Live-Test.cmd,
load the same profile, select COMPLEX MULTIBAND PCA (and COMMON EXTRACTOR for
linked MUSIC Scoop), inspect signal/DAC mappings, then ARM explicitly.
Existing output HP/LP, gains and Headroom remain user configured; no auto-ARM.
Closing Qt leaves Core running. This remains a beta test release.
