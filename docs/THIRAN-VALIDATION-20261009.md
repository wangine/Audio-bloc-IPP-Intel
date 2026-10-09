# FP64 fractional delay — 0.0.4 experimental candidate

Baseline: immutable stable beta 0.0.3, commit
`3e36e367741ab47f7ed861a21662866001ea8f13`.
Branch: `codex/crossover-thiran-delay`. No push or merge performed.

## Result

**PASS focused offline validation. Listening acceptance pending.**
18 selected native/control CTests passed; actual Core/Qt IPC test passed on an
isolated offline endpoint. No source transport or physical device was opened.
Historical Stage04 **FAIL REALTIME** and formal V07 **BLOCKED** are unchanged;
this does not claim full engine deadline or hardware certification.

## Implementation

- Per-output `fractionalDelayMode1..16`: Linear reference, Thiran 1, 3 or 5.
  New IDs append to the existing snapshot indices. Old profiles select Linear;
  MUSIC/CINEMA scene state and channel snapshots retain the new choice.
- Independent FP64 state for 16 outputs. Fixed-size two-bank crossfades; no
  new delay ring, sample-rate conversion, gain, normalization or limiter.
- Coefficients compile on the serialized control owner. Parameter snapshots
  adopt only at block boundaries. For Thiran, a delay/order/mode edit crossfades
  the old and new complete delay paths for 40 ms with fixed coefficients;
  fast edits coalesce after the current fade. Linear retains the original
  40 ms moving-tap ramp. The surrounding control snapshot fade remains 50 ms.
- For target `K+f`, a Thiran order `N` uses ring delay `K-(N-1)` plus all-pass
  delay `N-1+f`. No added acoustic latency. Order 5 needs `K>=4`; order 3 needs
  `K>=2`. Short delays select the highest causal eligible order (5→3→1),
  explicitly reported by Core telemetry and UI. Exact integer/zero delays use
  the existing ring tap directly, without a marginal all-pass pole.
- Emergency Mute/topology disable clear only the fixed-size filter states and
  existing logical ring validity. Reactivation cannot read pre-reset history.
  Ordinary Mute retains its existing gain ramp and history semantics.
- UI adds a selector and effective-order/transition readback via IPC JSON.
  It owns no audio, coefficients or DSP. Global Base remains until audio
  acceptance and the separately requested removal/profile migration.
- Output Matrix, Headroom V2, ClockSync, Guardian, PCM24, WASAPI and source
  transport logic remain unchanged. The original stable Stage3 directory guard
  is explicitly preserved when building inside the isolated worktree.

The coefficient definition follows Julius O. Smith's
[Thiran Allpass Interpolators](https://www.dsprelated.com/freebooks/pasp/Thiran_Allpass_Interpolators.html).
An all-pass preserves steady-state magnitude at fixed delay; it does not give
perfect broadband linear phase. During a crossfade, two phases can cancel
partially. That temporary behavior is not a hidden gain adjustment.

## Measurements

192 kHz FP64, input sine amplitude 0.125, delay 8.5 samples. Phase error is
against an ideal 8.5-sample delay; positive delay error means extra phase delay.

| Order | Hz | Gain error dB | Phase error degrees | Phase-delay error µs |
|---:|---:|---:|---:|---:|
| 1 | 20000 | +3.86e-14 | -0.515698881 | 0.071624845 |
| 1 | 40000 | -9.64e-16 | -4.479969651 | 0.311109004 |
| 1 | 60000 | -3.57e-14 | -17.365229564 | 0.803945813 |
| 3 | 20000 | -3.09e-14 | -0.002289374 | 0.000317969 |
| 3 | 40000 | -2.51e-14 | -0.265738826 | 0.018454085 |
| 3 | 60000 | -2.22e-14 | -4.054243265 | 0.187696447 |
| 5 | 20000 | -1.64e-14 | -0.000015514 | 0.000002155 |
| 5 | 40000 | -4.82e-15 | -0.023560496 | 0.001636146 |
| 5 | 60000 | -1.06e-14 | -1.299161935 | 0.060146386 |

Recommend **Thiran 5 for the initial wideband audition**, where the existing
integer delay permits it. It still has phase error at 60 kHz; unity magnitude
does not imply an exact ideal fractional delay.

216 numerical records: 120 helper cases and 96 actual Crossover output fits.
Helper frequencies: 10, 1000, 20000, 40000, 60000 Hz; requested delays:
0.01, 0.5, 0.99, 1.5, 2.5, 4.5, 8.5, 500.5 samples; all three Thiran modes.
Actual Crossover: all 16 outputs, MUSIC/CINEMA, all three modes, 60 kHz;
distinct per-output delay settings; mixed blocks 1/7/31/64/127/256/511/1024.
Maximum absolute measured gain error across those records: **3.31e-13 dB**.
An active output with no signal fails the amplitude checks.

Impulse energy is unity within 1e-12; DC group-delay moments at 8.5 samples:
8.500000000000004 (order 1), 8.499999999999996 (order 3), 8.5 (order 5).
Actual 16-output impulse moments agree with requested acoustic delays; the
offline one-FIR8193/15-IIR fixture also retains the original 4096-sample common
FIR latency. No extra Thiran latency is inserted.

DC rapid delay/order-edit fixture, input 0.125: maximum sample step
`4.89252444888e-05`, maximum output `0.12501364661`. No NaN/Inf detected in
near-integer pole stress, native mode changes or FIR/IIR/All-Pass transitions.
Near-integer stress verifies finite behavior, not complete settling of a pole
arbitrarily close to the unit circle. No tolerance was relaxed after failure.

**Linear vs 0.0.3:** 524288 compared samples, zero bit mismatches, max absolute
error 0. The pristine baseline is separately compiled under another namespace.
Zero/integer Thiran bypass also preserves signed zero bit-exactly in the delay
helper. New scene/profile round-trip and old-profile Linear migration pass.

Instrumented actual Crossover processing: C++ new/delete=0, imported
HeapAlloc/HeapFree=0, waits=0, locks=0, filesystem=0, including native mode and
FIR/IIR/AP transitions. This audit covers the existing instrumented C++/Windows
imports; it is not a new kernel scheduling qualification.

## Integrity and failure evidence

92 files outside the authorized delay/UI/control/build change were checked.
88 are byte-identical to the original checkout; four source transport files
have checkout line-ending differences only, with identical normalized text.
Git reports no changes in those module/source-transport directories. Stable
Core/Qt SHA256 and PIDs remained unchanged throughout this work.

Expected red tests are preserved: missing fractional-delay control before
implementation; departing epoch on the final crossfade frame before its fix.
The fix returns the exact destination value and marker at fade completion.
The first UI test lacked an available isolated server and failed; the corrected
fixture uses its own offline Core/endpoint and passes. A PowerShell fixture
null-read error was corrected before running that fixture. MSVC's sandbox PDB
manager error C1902 was resolved by running the build outside the sandbox.
Existing SoXR CMake CMP0115 and optional Vulkan-header warnings remain nonfatal.

## Reproduce / evidence

Run `tools/Verify-Thiran.ps1` using local MSVC x64 and Intel IPP 2026.0. It
extracts the fixed 0.0.3 baseline, reuses the pinned native egress dependency
cache, builds the complete Core, and runs only the selected focused tests.
Use `-Stage04Reference <native-reference-directory>` if the original cache is
elsewhere. Build the UI with `tools/Build-Stage05-UI.ps1 -BuildFolder build/qt-thiran`.
Actual UI integration uses `EntropyStage05Ui.exe <isolated-endpoint>` and a
separate Core with `--offline-test-signal`; never use the LIVE endpoint for it.

Committed numerical/log/integrity evidence: `validation/thiran-20261009/`.
Complete local raw logs, baseline and captures remain under
`local/thiran-results`, `local/thiran-ui`, `local/stage05/ui-captures` and
`build/msvc-thiran`. Review found no outstanding important findings after the
epoch fix and targeted native integration regressions.

Final MSVC 19.51.36257 x64 / Intel IPP 2026.0 candidate hashes:

- Core: `FB800425171F5A7962FF7434B08819D39C8FD44FD879FFEBD93AFAA17132410B`.
- UI: `A777E0B537DE6DB8E2528B5D37DD01C42B86FFA04C6D15D98435C7A0A10D60AC`.

No new push, merge, live playback, Stage1/2 modification or Global Base removal.
