# Rectification 05 — PCM24 and ultrasonic-peak evidence

Date: 2026-10-10, Europe/Bucharest. Source baseline:
`14708605e5be2bca13964d44f460c8f37cc955ea`, published as AudioCore 0.1.2.
Work branch: `codex/stage014-pcm24-ultrasonic-evidence`.

## Result

PCM24 configuration and bounded sub-rail conversion: **PASS**.
Live DAC A saturation on the measured interval: **FAIL**.
Live DAC B finite-overrange delta: zero on this interval; earlier clipping
counters remain visible. Analog attribution/calibration: **BLOCKED**.

No PCM24, ClockSync, DSP, Core or Qt production source changed. No noise shaping,
error feedback, limiter, normalization, double dither or new DISARM was added.
Finite overrange remains CLIP_AND_REPORT; NaN/Inf protection is unchanged.
Only offline diagnostic sources, CMake test registration, reproduction tools,
and evidence/documentation were added.

The user's approved prior candidate was pushed on its source branch and uploaded
with the full report and retained failures:
https://github.com/wangine/Audio-bloc-IPP-Intel/releases/tag/v0.1.2
ZIP SHA256:
`F0C0871DDEDF75BC187285B78342C6E9820498A9F18C59C1357DBD020A044A70`.
All 521 package-manifest files were checked; GitHub asset digest matches local.
No merge into main, private profile or raw music upload.

## Executed live runtime and enum decoding

The actual running engine was the unchanged **0.1.1 Core**, PID 6848, rather than
restarting it just for diagnosis. Its executable SHA256 is recorded in the
integrity manifest. The PCM24 ABI, converter, engine, NativeEgress and ClockSync
sources are unchanged between its commit `cfce5849...` and the 0.1.2 baseline.
The offline evidence executable uses those same sources from the 0.1.2 baseline.
This identifies the tested runtime; it does not claim a 0.1.2 live audition.

Enums from `Shared/include/entropy/pcm24_abi.h` of that source version:

| Field | Executed value | Meaning |
| --- | --- | --- |
| pcmDitherMode | 1 | ENTROPY_PCM24_DITHER_TPDF_FLAT |
| pcmSeedMode | 1 | ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT |
| DAC output format | 1 | ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED |
| pcmFaults | 0 | ENTROPY_PCM24_FAULT_NONE |
| NativeEgress policy | 1 | ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT |

Overrange policy is confirmed from the unchanged NativeEgress configuration;
the live domain snapshot does not expose a policy field. Overrange increasing
with zero faults and continuing output is consistent with that policy.
Production master seeds are generated with BCryptGenRandom during quiescent
prepare; per-channel two-stream PCG state is seeded before processing. The
source implements TPDF U1-U2 without feedback. Dither is applied once in final
FP64-to-PCM24 conversion after ClockSync and optional Guardian exact bypass.

## Live before/after interval

Two telemetry snapshots, approximately 16 seconds apart, with the same Core PID
and both DACs RUNNING/ARMED throughout. Each DAC processed/submitted 3,083,264
new frames (about 16.059 seconds at nominal 192 kHz). Snapshot/API wall interval
is approximate and counters are independently loaded, not an atomic per-sample
capture. The local script's 16.015-second timer excludes profile-export latency;
do not use it as a sample-clock timing calibration.

| Counter | DAC A before | DAC A after | Delta A | DAC B before | DAC B after | Delta B |
| --- | --- | --- | --- | --- | --- | --- |
| Overrange samples | 14,098,590 | 14,587,443 | **488,853** | 4,832 | 4,832 | 0 |
| Clipped blocks | 26,085 | 27,153 | **1,068** | 25 | 25 | 0 |
| Rail clamp samples | 0 | 0 | 0 | 0 | 0 | 0 |
| Nonfinite samples | 0 | 0 | 0 | 0 | 0 | 0 |
| PCM faults | 0 | 0 | 0 | 0 | 0 | 0 |
| XRUN | 0 | 0 | 0 | 0 | 0 | 0 |

Raw finite overrange and clipped-block counts describe saturation, not
transparency. Rail-clamp telemetry counts in-range programme samples whose
dither/rounding result exceeds the representable rails; it deliberately excludes
raw overrange. Thus rail-clamp zero does not contradict the observed clipping.
Near full scale, rail clamps may occur without raw overrange; none were observed
here. The zero finite-overrange delta on DAC B does not erase its historical 25
clipped blocks or establish universal transparency.

Headroom actual mode is BITPERFECT (enum 1), effective gain 1. Stored manual
control -3 dB is inactive in that mode. Active MUSIC Bass/Scoop uses PCA extractor
3 with COMMON EXTRACTOR link 2; PHASE SAFE is inactive. The profile and audio
policy were not changed. Qt alone was temporarily closed to access the existing
single-client IPC, then reopened. Core retained output; Stage1/Stage2 were not
restarted or modified.

The first diagnostic client waited 16 seconds on a connection with a 5-second
idle lease, causing Windows pipe error 233. That invalid counter pair was
discarded. The corrected collection uses two short IPC sessions; no server or
audio code was changed. This was a diagnostic-client issue, not a PCM fault.

## One bounded converter probe

One second at 192 kHz, two channels, FP64
`0.001*sin(2*pi*1000*t+phase) + 0.00001*sin(2*pi*50000*t+phase)`.
The 50 kHz component is intentionally present before the converter. The same
fragment and deterministic test seed check both packed-24 and left-aligned-32
formats. Production seeds remain independent. No sink, source connection or
live callback recorder is instantiated.

384,000 channel-samples per format:

| Measurement | Result |
| --- | --- |
| FP64 input peak | 0.001009452572547457 |
| Max absolute decoded PCM minus immediate FP64 input | **1.488792134742 LSB** |
| RMS error | **0.499623419681 LSB** |
| Mean error | -0.000986979167 LSB |
| Integrated RMS error | -144.500942254 dBFS |
| Fault / overrange / clipped-block / rail-clamp / nonfinite counts | all zero |
| Packed and left-aligned decoded PCM | bit-exact |

Every sample satisfies the unchanged 1.5-LSB bound. Selected coherent bins of
the introduced error range from approximately -185.4 to -194.6 dBFS peak-bin
amplitude. At 50 kHz the input component is approximately 1e-5 (-100 dBFS peak);
introduced error-bin amplitude is 4.34247518e-10 (-187.245 dBFS). The input tone
is not a spur introduced by PCM24. Full selected-bin values are in numeric.json.
These are selected coherent projections, not a complete FFT/DAC spectrum.

Integrated noise and individual frequency-bin levels use different bandwidths.
The theoretical full-scale-sine SNR for standard 24-bit TPDF below rail is
141.4841 dB from `(1/sqrt(2))/(0.5 LSB)`. It is not the level of every FFT bin.
At 192 kHz, Nyquist is 96 kHz. No large converter-error peaks were observed on
this probe; this does not certify DAC-internal or amplifier analogue behaviour.

## Existing bounded FP64 captures — overload location

Only the two six-second captures already stored during Stage012 were reused.
No new audio recorder or upstream attachment was added. The quiet fragment
gave DAC A peak 0.6498262631 and DAC B 0.2030037438, with clean counters. It
cannot explain overload on different, louder material. Its limited PASS is
retained as historical-fragment evidence, not a PASS for the live interval.

The existing louder fragment reproduces overload under the current profile:

| Domain / policy | Pre-Headroom peak | Post-Headroom peak | Post-ClockSync peak | PCM overrange | PCM clipped blocks |
| --- | --- | --- | --- | --- | --- |
| A, original BITPERFECT | **2.7547460817** | **2.7547460817** | **2.7547460817** | **379,313** | **714** |
| B, original BITPERFECT | 0.8374289252 | 0.8374289252 | 0.8374289252 | 0 | 0 |
| A, explicit MANUAL SAFE -12 dB copy | 2.7547460817 | **0.6919609305** | **0.6919609305** | **0** | **0** |
| B, explicit MANUAL SAFE -12 dB copy | 0.8374289252 | **0.2103526354** | **0.2103526354** | **0** | **0** |

The first overrange is already present before Headroom. Original BITPERFECT
pre/post samples match exactly, maximum difference 0. Converter overload is
therefore not repaired by changing TPDF. Native clock simulation did not
increase the measured maximum on this fragment. Original replay exits **2**
with `FAIL POST-HEADROOM/PCM OVERRANGE`, retained in the report and logs.

All 1,152,000 graph frames are scanned at pre/post Headroom. Clock/PCM render
1,146,880 frames after prebuffer; buffered tail is not flushed. Their temporal
windows are different. RMS changes and differences in overrange counts cannot
be attributed to ASRC gain. JSON explicitly marks nonidentical windows and
qualifies clean output as `PASS MEASURED SUBSET`; full post-Headroom overrange
still causes FAIL even if omitted from clock rendering. The clock model has
independent nominal 192 kHz timestamps, not the exact live phase/scheduling.

The explicit comparison copy changes only Headroom policy to MANUAL_SAFE and
manual control to -12 dB. Filter, Matrix, gain/delay, timing/attack/release,
device assignments and all unrelated profile metadata are retained; native
ProfileStore reload and document comparison verify this. Original mode 1
remains untouched. The copy is not loaded or armed live automatically.
It is a visible policy choice, not hidden engine attenuation. Its clean result
applies to the measured fragment, not every future programme or physical DAC.

## ClockSync and analogue scope

`EntropyBandlimitedAsrc.h` implements a 96-tap, 4096-phase sinc bank. Linear
blend interpolates adjacent coefficient sets; interpolation sums 96 history
samples. It is unchanged. No two-sample replacement or Thiran clock converter
was introduced. The final dither remains after ClockSync.

No calibrated ADC/instrument capture with verified ultrasonic bandwidth and
anti-alias response was supplied or executed. Analog attribution is BLOCKED.
The current live finite saturation must be addressed through the user's visible
gain/Headroom policy before attributing any analogue spur to dither. Production
PCM24 configuration, error bound and counters are correct: **no PCM24 algorithm
patch is justified**. Headroom/PCA/ASRC mathematics remain unchanged.

## Reproduction and delivery

`tools/Capture-Pcm24Telemetry.py --runtime-version <actual-version> --output
<private-directory> --seconds 16` reads two short IPC sessions. Close only Qt
first; restart your same Qt afterwards. Use an already saved profile for offline
replay. The capture tool sends telemetry queries only, never ARM/DISARM or
profile commands, and returns nonzero for
overload. Match its source header to the actual runtime before decoding enums.

`tools/Validate-Stage014.ps1 -Profile <saved-profile> -ExistingCapture
<existing-six-second-raw> -CreateManualComparison -ManualComparisonGainDb -12`
builds, runs one converter probe plus the original replay, then the optional
explicit comparison. Original FAIL remains nonzero even when the copy passes.
No profile/capture arguments runs only the sub-rail converter probe.
For a fresh build, provide `-Stage04Reference <native-reference-directory>` and
`-StableStage3Directory <protected-stable-directory>` from the existing pinned
Stage04 prerequisites. This checkout's build used
`D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference`;
the protected stable directory was `D:/Projects/3Stage-3/Stage3`.
The reference SDK/source directory is not bundled in the evidence.
The telemetry tool was checked against the two saved live snapshots: it
reproduces the reported deltas and preserves the overload exit status without
sending IPC. The PowerShell wrapper passed syntax validation.

Build PASS: MSVC 19.51.36257.0 x64, Intel IPP 2026.0. No broad CTest suite,
benchmark or permanent instrumentation. Previously reported Stage04 realtime
FAIL and formal V07 acceptance BLOCKED remain; they were not re-evaluated.
The published 0.1.2 and accepted stable references remain unchanged. Stage014
sources/evidence are saved locally on the separate branch; no Stage014 push.
Full raw snapshots, profiles and raw music remain private under local/.
