# PCA Bass + Scoop — candidate 0.1.4

Baseline corrected by the user: **v0.1.3**, commit
`ebd114e3e293f614cfeaae634fc103114f254105`. Branch `codex/pca-pr-bass-scoop`.
The instruction's earlier 0.1.2 base is superseded. Existing published tags,
release folders, live profiles/runtime and Stage1/2 are unchanged.

## Implementation

Only PCA mode 3 and its internal lane state change in production. Audio is:

`y[n] = sum(k=0..4,c selected) w[k,c,n] * B[k,c,n] + sum(c selected) R[c,n]/N`.

For each input, five parallel two-stage one-pole LP filters delimit
40/63/98/152/240 Hz. B0=LP40; subsequent bands are consecutive LP differences;
R=x-LP240. Their sum telescopes to x within FP64 rounding. No extra lookahead,
decimation, FFT, SRC or latency buffer. This does not imply nonlinear PCA perfect
reconstruction for arbitrary mixtures. Existing output HP/LP remains responsible
for physical SUB/SCOOP band limits; the residual intentionally retains broadband
content before those output filters.

The original complex envelopes/covariance at 32/50/78/122/190 Hz estimate
spatial weights only. No principal*carrier audio reconstruction remains.
Signed phase-cosine weights are L1 normalized, confidence blends ambiguous/low
energy estimates toward the fixed average, and weights evolve with a 30 ms
one-pole time constant and are L1 renormalized at use. No RMS normalization,
AGC, hidden attenuation or limiter. Reference-channel energy hysteresis is 1%.

A targeted polarity-change probe exposed a nonprincipal-vector lock: after L=R
and silence, a retained [1,1] seed could stay orthogonal to L=-R. Fixed bounded
four-step iterations compare the retained seed and each selected coordinate by
Rayleigh energy, then preserve phase alignment. No heap/locks/waits added to
PCA processing; coefficient exponentials are computed only at prepare.

All three input LP/envelope/covariance states stay current during PCA processing,
including channels outside the feed. A feed edit invalidates the estimator but
keeps LP histories; source-generation/re-entry resets are bounded internal state
clears with prepared coefficients. COMMON EXTRACTOR still copies Bass to Scoop
identically and invalidates the unused independent state. MUSIC independent
Scoop and CINEMA L/R/C Scoop keep their own lanes. CINEMA SUB/LFE is untouched.
Weight smoothing does not certify arbitrary feed edits or source discontinuities
as acoustically click-free; no broad transition/live test was performed.

## Bounded numerical acceptance

The original summed-envelope implementation failed the new explicit
0 +/-0.02 dB / 0 +/-0.2 degree acceptance. After warmup, Sub and linked Scoop
pass all five frequencies, with nonzero output and identical linked samples.

| Hz | Old dB | Old phase degrees | New dB | New phase degrees |
| --- | --- | --- | --- | --- |
| 32 | +10.846771 | +4.092571 | -9.643e-15 | 2.139e-15 |
| 50 | +11.041103 | -8.565770 | 3.472e-14 | -1.559e-15 |
| 78 | +10.522730 | -19.719191 | 5.979e-14 | 8.822e-16 |
| 122 | +9.715691 | -31.391574 | 5.979e-14 | 1.914e-15 |
| 190 | +8.533480 | -46.596468 | 4.822e-14 | -1.645e-15 |

L=R impulse maximum absolute error: 5.4210108624e-20; no added impulse latency.
Independent MUSIC L=R=C and LEFT/RIGHT/CENTER, plus CINEMA Scoop L=R=C:
maximum absolute error <=5.5511151232e-17. Direct L/R/C fixture samples match
bit-exact; original direct routing canonicalizes input signed zero in the
antipolarity fixture. The full replay direct-lane sample hash is also identical
before/after: `12771570129609610592` (covers every sample, including zero).

L=-R at 50 Hz after L=R plus silence is nonzero and not boosted:
-0.369037834 dB, phase -23.442950582 degrees, peak 0.119800359.
Unity is not expected for antifaze: the residual fixed average cancels, leaving
the weighted LP bands. CINEMA SUB retains the direct LFE fixture exactly.

## One existing six-second fragment, original profile unchanged

The same existing Stage012 fragment was replayed on baseline and patch, never
attached to a source or physical sink. Original filters, gains, routing and
Headroom policy are retained. The saved profile has BITPERFECT effective unity;
manual control -3 dB is inactive, and pre/post-Headroom maximum difference is 0.

| Measurement | Before | After |
| --- | --- | --- |
| SUB output peak FP64 | 2.754746081730 | 0.813002314414 |
| SCOOP output peak FP64 | 0.837428925152 | 0.263313985891 |
| DAC A pre-Headroom peak FP64 | 2.754746081730 | 1.479562185519 |
| DAC A PCM24 overrange samples | 379313 | 1310 |
| DAC A clipped blocks | 714 | 16 |
| DAC B PCM24 overrange samples | 0 | 0 |

**PASS PCA acceptance; FAIL remaining fragment PCM overrange.** No tolerance
relaxed and no forced Headroom change. No claim of zero overrange on arbitrary
signals. The new DAC A peak exceeds both new Sub/Scoop peaks; unchanged direct
lanes and the original routing remain relevant to the remaining headroom need.

Graph/Headroom cover 1,152,000 frames; each ClockSync/PCM domain covers
1,146,880 frames after startup prebuffer, with queued tail unflushed. Before/after
comparison uses identical windows; cross-layer RMS differences are not gain
measurements. Nominal independent 192 kHz clock model, not live scheduling or
actual hardware clock phase. Raw music and profiles remain private under local/.
Full numeric evidence: `validation/stage015/numeric.json`.

## Build / reproduction / delivery

MSVC x64 19.51.36257, Intel IPP 2026.0. Built only Core and existing targeted
EntropyStage013Crossover dependencies; Core --probe passed offline.
No full CTest, benchmark, Stage1/2/4/7 suite or live DAC execution.
Historical Stage04 realtime FAIL and formal V07 BLOCKED are not re-evaluated.

Using the existing configured native build and MSVC x64 environment:

```
cmake --build build/msvc-thiran --target EntropyAudioCore EntropyStage013Crossover --parallel 4
build/msvc-thiran/EntropyStage013Crossover.exe --pca-only local/pca.json
build/msvc-thiran/EntropyStage013Crossover.exe --pca-replay <saved-profile> <existing-6s-raw> local/replay.json
```

Replay returns 2 for measured overrange, preserving FAIL. No capture is requested
if the fragment is unavailable. Saved profiles may need normal-user execution
outside the filesystem sandbox for weakly_canonical.

Core SHA256: `C1871A96B8CF3C45A11EBEABB235888436A3E4EB410F7A948B31735086605CD9`.
Implementation was delivered in one local candidate commit. The user then
authorized a separate 0.1.4 test package and publication with sources/binaries.
Release numbering is **0.1.4**; the artificial PCA reconstruction boost is
removed, while the documented residual overrange FAIL remains visible.
