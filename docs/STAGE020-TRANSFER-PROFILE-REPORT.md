# Audit 04 — MID/MIDLOW electric transfer and diagnostic profile

2026-10-10. Baseline: user-accepted v0.1.8, tag commit
`eb0a6a224d19e0d78c352f5e25fb16a522e2e619`. Both 32k and 64k builds are published
references; user hears no reliable difference between capacities. Both retain
target4096, rather than changing latency with capacity.

**PASS: bounded Stage3 coefficient transfer, Matrix, Headroom, source/tap
continuity, scalar ASRC arithmetic/indexing. BLOCKED: acknowledged applied
Stage2 EQ/Center/alignment state; actual live A/B alignment/acoustic cause.**
No unexpected Stage3 electric transfer was demonstrated. No DSP mathematics,
audio callback, production executable, UI, source process or acoustic tuning
has been changed. This is a diagnostic result, not a claim that an acoustic
mid/midlow complaint has been corrected.

## State and bounded evidence

One applied-state export was taken while the existing 0.1.8-64k Core remained
ARMED and Qt was closed. A control-side save-profile barrier exported a private
copy. Its SHA256 equals the untouched mother profile:
`0C7FB25B0A8A5910BBAFC004EBA984AB08DE28C4D9B1753BEAA4AF0383DE3292`.
The copy and recorded audio remain local. Applied parameters and Matrix routes
match the replay; generations/revisions are bookkeeping rather than audio
changes. `validation/stage020/ACTIVE_AUDIO_PATH.md` gives the lane table.

The existing six-second passive FP64 fragment was reused: 1125 blocks ×1024,
192kHz, 11 lanes L/R/C then eight CINEMA lanes, two legacy Graph inputs zero.
No recording, live benchmark, soak or additional source attachment was made.
Source first frame559001600, epoch1, sequence545901; consecutive source frames
and sequences pass with no gap. One Graph pass per profile captured input,
post-XO, post-Matrix and post-Headroom. Both ASRC variants were subsequently
replayed from those same saved taps; Graph was not replayed to debug the
comparator. Source frame/epoch/QPC sideband remains in local identity.csv.

The same window0.75–5.5s is used throughout the coefficient comparison; delay
is included in expected transfer, not removed from one RMS window only.
Preparation settles controls on silence; the fragment then starts with zero
history. This validates fixed applied transfer, not the live engine's past
history. Actual Headroom is MANUAL_SAFE -3dB, gain=target0.7079457843841379,
attack217ms, release4000ms; replay does not substitute AUTO or unity.

## Independent electric comparison

NumPy evaluates exported real HP/LP/AP section coefficients, input trim,
signed gain, integer delay and Thiran/legacy fractional coefficients. It does
not call native Crossover processing or VisualizationWorker response. FIR
coefficient export/reconstruction is supported, but the seven current linear
LR/Center lanes are IIR; nonlinear PCA bass lanes are measured rather than
assigned a false LTI oracle. FFT zero-padding4194304 samples and the declared
window avoid circular-boundary/tail mismatches.

| XO lane | Original max abs error | Without manual LR AP max abs error |
|---|---:|---:|
| CH1 L WAY1 | 5.827e-12 | 5.799e-12 |
| CH2 L WAY2 | 6.639e-14 | 5.008e-14 |
| CH3 L WAY3 | 7.041e-14 | 6.212e-14 |
| CH4 Center | 3.813e-13 | 3.813e-13 |
| CH5 R WAY1 | 5.979e-12 | 5.983e-12 |
| CH6 R WAY2 | 5.246e-14 | 4.236e-14 |
| CH7 R WAY3 | 4.521e-14 | 4.777e-14 |

Fixed absolute tolerance2e-9; meaningful excitation and exactly seven oracle
lanes are required. The lowest expected band RMS is approximately1.61e-5.
A zero expected/actual window reports BLOCKED SILENT WINDOW, never PASS.
Matrix independent route accumulation maximum absolute error0; Headroom
manual-gain multiplication maximum absolute error0 in both cases. Full RMS,
peak, crest,20ms envelope residual and complex transfer for200–400,
400–800,800–1600,1600–4000Hz are in numeric.json. No additional burst was
needed: the fixed transfer did not diverge from its intended coefficients.

## Diagnostic A/B, no acoustic migration

Only allPass1/2/3/5/6/7 changes1→0. Those manual sections are1kHz/Q≈0.71;
they are separate from Thiran. Thiran modes, coefficients, delays, frequencies,
gain, input trims, topology, Headroom and DAC lanes are retained. Center/Sub/
Scoop outputs compare exactly: maximum absolute difference0. Their profile
blobs are unchanged; Matrix generation/Headroom revision bookkeeping changes.

| Electrical Graph sum | Original RMS | No LR manual AP RMS | Difference dB |
|---|---:|---:|---:|
| Three L ways | 0.26256948 | 0.26255636 | -0.000434 |
| Three R ways | 0.23213536 | 0.23213516 | -0.000007 |
| L ways + measured Center | 0.27164726 | 0.27654894 | +0.155333 |
| R ways + measured Center | 0.25644840 | 0.27463203 | +0.595024 |
| L ways + Center + measured Scoop | 0.26624096 | 0.27469435 | +0.271497 |
| R ways + Center + measured Scoop | 0.26569307 | 0.30903972 | +1.312681 |

These use unit electrical weights on the common Graph timeline. They are
neither a loudspeaker response prediction nor an aligned analog A/B sum.
R/Center correlation changes -0.03926→+0.13629; R/Scoop -0.01163→+0.56766.
Individual LR energy remains nearly unchanged while relative phase and sums
change. A crest change after AP/filtering is not evidence of compression.
Manual AP therefore remains a plausible acoustic discriminator, not a proven
code fault. Original frequencies/alignment/profile are preserved.

Stage2 code confirms Center's raw-LR tap before MUSIC LR EQ. Runtime applied
EQ static/dynamic/bypass, Center and LRC alignment are not available in the
current acknowledged export. Saved Stage2State.json is not applied-state
evidence. The reused fragment contains the actual historical Center; its RMS
and correlations are measured. It cannot establish current runtime EQ or
reconstruct historical pre-EQ LR. No Stage2 module or tap was changed.

## Per-DAC ASRC and limits

Physical mapping is read from the exported profile: LR ways, Center and SUB
all use DAC A; Scoop uses DAC B. The clock of B alone cannot explain a
divergence between LR and Center on A. Actual live snapshot reports
estimatedABDifferenceFrames=-82.84245571565629, softwareAlignmentVerified=false,
analogSkewMeasured=false, resync/fault/deadline counters0. LOCKED is not a
certificate of simultaneous analog playback. This unresolved limit remains
visible, including in the published0.1.8 release documentation.

Offline original ASRC uses calibrated virtual clocks at the snapshot rates
192001.73211183Hz /192001.69472621Hz,1024 blocks,target4096,capacity65536.
Graph origin is rebased to zero for fractional precision, with the original
absolute identity retained separately. Each block saves actual start/end
position, epoch, stepBegin and stepEnd. Mapping includes the per-frame slew
and sequential FP64 rounding, not a linear interpolation between endpoints.
Initial selected Graph position256 is declared; no surprise resync or frame
jump occurs. The scalar96-tap coefficient-bank dot product checks every64th
sample per lane,142976 comparisons per DAC/configuration.

| Configuration | DAC A max abs / RMS error | DAC B max abs / RMS error |
|---|---|---|
| Original | 6.661e-16 /4.190e-17 | 1.388e-16 /4.804e-18 |
| No LR manual AP | 7.772e-16 /4.198e-17 | 1.388e-16 /4.804e-18 |

This oracle checks accumulation/indexing against the real coefficient bank;
it does not independently validate that bank's design. Virtual timing is not
a replay of recorded hardware callbacks or an analog measurement. No egress
rewrite is justified by this evidence.

## Implementation, reproducibility and failures retained

Production integration adds only read access to already existing Graph
buffers through GraphControlAccess. No permanent recorder or callback work
is added. Tests/Stage020Transfer.cpp performs the bounded offline replay;
tools/Analyze-Stage020.py supplies the independent transfer oracle.
tools/Export-Stage020.py is a one-shot control-side export helper.
tools/Verify-Stage020.ps1 builds only the harness and uses explicit private
profile/fragment/export paths against an already configured MSVC/IPP cache.
NumPy is required; no SciPy is used. Neither reproduction tool launches
sources, Qt or a hardware sink. Export requires Qt closed because the current
control pipe has one client; Core remains running.

MSVC19.51.36257 x64 /IPP2026.0 build PASS. 101 protected source hashes match
v0.1.8, including DSP, UI, original ClockSync/sinc/controller, PCM24 and
Guardian. Existing32k/64k production binaries remain unchanged. No full suite
or benchmark was run for this bounded audit.

Retained failed evidence: red-tap.txt shows absent read-access compilation;
replay.txt records the initial **comparator** failure. Root cause was linear
within-block position interpolation despite slewed ASRC step, compounded by
large-origin phase precision. Comparator-only correction retains1e-8 gate;
asrc-replay.txt and diagnostic-asrc-replay.txt show near-FP64 errors after the
fix. It did not change the audio algorithm or weaken acceptance.
Historical Stage04 FAIL REALTIME and formal V07 BLOCKED are unchanged.

For user audition, load the separate DIAGNOSTIC-no-manual-LR-AP.entropy copy
in existing0.1.8, then use explicit ARM if loading closes egress. Do not
overwrite the mother profile. Center/Scoop are not muted. This profile is an
optional A/B diagnostic, not a recommended acoustic replacement. If it does
not discriminate the complaint, the next evidence is one authorized
simultaneous analog comparison rather than another DSP rewrite.
