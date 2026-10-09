# Rectification 04 — Crossover Bass / Thiran evidence

Date: 2026-10-10. Baseline: AudioCore 0.1.1, commit
`cfce5849dc520ec589758fa8fac5046fa0a8deb2`. Local candidate 0.1.2.

## Confirmed defect and narrowly scoped patch

The original bounded zero-delay fixture failed: at frame 1 direct L expected
0.00390561424816576, observed 0.0038151366980152019, with trim/gain both zero dB.
The nonnegative millisecond range retains an 800 ms encoding offset for legacy
positive FP32 control compatibility. Its offset step multiplication rounds
`800000 * 0.001f` above 800, turning exact zero into a positive fractional delay.
This enters the all-pass path instead of exact bypass. See the retained RED log.

Only `DSP/Crossover/include/NativeSupport.h` changes in production: exact zero
in a zero-based, offset-encoded range bypasses quantization. Positive arithmetic
is unchanged, verified bit-exact against a frozen legacy conversion oracle.
PluginProcessor, FractionalDelay, filter coefficients, gain, PCA, Matrix,
Headroom, ClockSync, Guardian, PCM24, WASAPI and Qt sources are unchanged.
No hidden delay base, normalization, limiter or attenuation was introduced.
No algorithm patch in Bass/PCA/Thiran is justified by these probes.

## Actual profile, captured from the running Core

MUSIC; Bass feed L+R; link 2 COMMON EXTRACTOR. Bass extractor 3 COMPLEX MULTIBAND
PCA is active. Stored Scoop extractor 1 AVERAGED MATRIX is overridden in MUSIC
by link 2, so MUSIC Scoop also uses PCA. PHASE SAFE is inactive: no polarity
transition patch or correlation-change test is justified. CINEMA is inactive.
Headroom is BITPERFECT; stored manual -3 dB is not the effective unity scalar in
that mode. No Headroom settings or DAC mappings were edited by this stage.

`actual-lanes.csv` preserves all 16 lane values, including disabled lanes,
MUSIC/CINEMA positive delay, polarity, HP/LP, AP frequency/Q, protection, gains,
per-lane IIR/FIR mode and reported effective fractional order. MUSIC active lanes
are CH1-8 and CH12. CH8 uses FIR (global Bass process value does not override its
per-lane FIR setting); CH1-7 use IIR. CH1/4/5/8/12 select legacy linear mode;
CH2/6 select Thiran3; CH3/7 select Thiran5. No mode was silently changed.
All captured polarities are normal. CH10 CENTER WAY2 is disabled by topology.
Saved tiny positive values `0.00006103515625 ms` are retained as positive: the
adapter cannot infer their intent. Explicitly request zero where intended.

The private comparison copy changes only active Bass extractor 3 to CLASSIC 0,
in current parameters and MUSIC scene. Linked Scoop therefore follows fixed
mono. Matrix, DAC configuration, other module payloads, filters, gains, delays
and unrelated scene parameters are identical. Original profile is unchanged;
private profiles and endpoint IDs are not committed or put in the release.

## Bounded numerical results

PASS: CLASSIC and AVERAGED each retain `(L+R)/2`. L=R unity, R=0 half gain and
L=-R cancellation all have maximum absolute error 0. Direct L/R is unchanged.

PASS: actual-profile comparison uses distinct nonzero L/R/C over one 98304-frame
fragment. 786432 direct-role output samples match bit-exact, maximum absolute
error 0; all three output-family energies are nonzero. Bass/Scoop difference
0.16221838760813026 is localized to the requested extractor change. Existing
filters/protection are retained; all outputs are finite. This shows extractor
isolation, not source-route or acoustic-system certification.

PASS: native IIR LR24 at fc=1000 Hz, without AP/acoustic delay in the offline
fixture. LP -6.020599913280868 dB, HP -6.020599913279616 dB. Complex sum magnitude
0.99999999999992895, phase approximately 180 degrees. Maximum complex error
9.285e-14 against independent -0.5-per-branch oracle (unchanged 2e-9 tolerance).
This single electrical pair does not certify the actual multiway acoustic sum.

PASS: exact zero control, eight representative positive FP32 controls bit-exact
to legacy, zero/integer sample-delay bypass including signed zero, and effective
orders 0/0/1/3/5 at 0/1/1.5/2.5/4.5 samples. One 7680-frame bank edit is finite,
ends at the original duration and adopts order 5. It does not certify constant
amplitude through the edit. No delay algorithm or transition code changed.

### PCA rank-one characterization

Input L=R=0.125*sin at each of the five bank centres; one second warm-up and one
second synchronous measurement per centre. Direct L/R max error 0; all PCA
outputs finite and non-silent. Results describe the five summed, overlapping
complex-envelope reconstructions, not five isolated unity-gain filters.

| Hz | Gain | Gain dB | Phase degrees | Peak |
| --- | --- | --- | --- | --- |
| 32 | 3.486089548 | +10.846771 | +4.092571 | 0.435761183 |
| 50 | 3.564964155 | +11.041103 | -8.565770 | 0.445620439 |
| 78 | 3.358431674 | +10.522730 | -19.719191 | 0.419803959 |
| 122 | 3.060444913 | +9.715691 | -31.391574 | 0.382555614 |
| 190 | 2.671000691 | +8.533480 | -46.596468 | 0.333875086 |

PCA transparency: FAIL if expected to be a unity sum. Characterization: PASS.
The adaptive effect and overlapping reconstruction explain the level/phase
change. No automatic normalization hides it. PCA is disabled only in the
comparison copy. A redesign for transparent reconstruction needs a separate
transfer-function specification and acceptance; it is outside this patch.

## Delivery and retained limitations

MSVC x64 build PASS; Visual Studio 2026 v18.10.0, compiler 19.51.36257;
Intel IPP 2026.0. Core `--probe` PASS. Only the bounded evidence executable was
run; no whole-project sweeps, benchmarks or per-sample logging. The generated
probe target is registered with CTest for future reproduction.

Historical Stage04 REALTIME FAIL and formal V07 acceptance BLOCKED remain.
The prior 36/37 suite result includes the retained finite-overrange policy
assertion FAIL; that suite was not rerun here. AUTO -3 capacity FAIL remains
documented in Stage012. No new acoustic/analog listening PASS is claimed.

Runtime 0.1.1 and its live settings were left running unchanged during offline
work. Stage1/2 were not restarted or modified. No test opened hardware. The
CLASSIC reference was not loaded/armed: actual Scoop CH12 has HP/LP/AP off, so
its fixed mono would be broadband. Configure intended Scoop filtering before
auditioning that copy. Synthetic bypass fixtures are offline only.

Reproduce: `tools/Validate-Stage013.ps1 -Profile <saved-MUSIC-profile.entropy>`.
Without Profile, it runs synthetic bounded probes only. Each reproduction uses
a new private output directory and never controls a live engine or DAC.
Source/EXE SHA256 inventory: `validation/stage013/source-integrity.json`.
