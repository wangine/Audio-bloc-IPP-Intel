# 0.1.7 beta test — FFT / response visualization
Baseline: v0.1.6; no change to FP64 audio mathematics, Stage1/2, stable reference or profile.
Explicit IPC view v2 responseFrequencyHz; UI projects/crops actual Hz on 5k/40k/60k axes.
Responses use APPLIED sections, real FIR kernel, gain, linear/Thiran delay and LFE bypass; complex phase included.
Crossfade is TRANSITION; static lane transfer does not claim a transfer for the nonlinear PCA source extractor.
One visualization worker owns FFT/cache; audio supplies atomic samples/metadata, control reads complete frames.
N=16384 normalized Hann; interior2/N, DC/Nyquist1/N; spectrum unit dBFS peak, not RMS/noise density.
60dB/s from new-frame captureQpc; idempotent reads preserve smoothing; new channel/scene needs N fresh samples.
Targeted regression PASS:119.5Hz preserved; sine .125@1500Hz=-18.0618dBFS;10/20Hz reads=-36.0618 after.3s.
MUSIC→CINEMA→MUSIC and >500ms stale checks PASS;48 original XO methods/57 protected files unchanged.
Build MSVC19.51.36257 x64/IPP2026.0,Qt6.11.2; visual LR/Center/Bass PASS offline; no realtime qualification.
Run Live-Test.cmd after saving/stopping previous Core; explicit ARM by user. Live acceptance PASS USER CONFIRMED on 2026-10-10: user confirmed the FFT/axis/time revision works. Stage04 FAIL REALTIME and formal V07 BLOCKED remain historical limits; no hardware skew measurement.
