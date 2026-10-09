# Rectification 3 — Headroom detector mask and fixed gain reference

Date: 2026-10-09. Candidate: AudioCore 0.1.1 beta test.
Baseline source publication: `48f979f489264885ab1a16351f81c43ddf64b0f1`.
Accepted Core executable remains the unchanged 0.1.0 reference.

## Actual session

Read and saved the user's actual Core state through IPC without altering it:
MUSIC, Headroom **BIT PERFECT**, manual control -3 dB, reserve 3 dB,
ATTACK 217 ms, RELEASE 4000 ms. AUTO was not active, so AUTO modulation does
not explain the sound of that current session. DAC A lanes 0–7 and DAC B lanes
8–15 were configured, giving physical `renderLaneMask=0xffff`. The warmed
offline graph's valid Matrix intersection was `0x01ff` for this saved profile.
No source or DAC identifiers or private profile are committed.

## Implementation

Control derives the mask from enabled physical configurations, independently of
sink ARM/liveness, then intersects it with Matrix enabled destinations that have
active contributors. The same subset determines the many-to-one ledger. Changes
are queued in a coherent parameter snapshot and adopted at an audio block
boundary. Device command failure restores its previous configuration. Profile
load and rollback retain the corresponding mask. The preflight cache includes
the mask, so altered physical routing cannot reuse a stale result.

Native Headroom `analyzeFinal()` skips channels outside its existing active mask.
This is the only production Headroom source change. One global linked group is
preserved. Requests for multiple groups or nonzero channel groups are rejected;
no Qt visual/backend code was changed. Excluded channels retain the native exact
bypass behavior. No Crossover, Matrix, ClockSync, Guardian, PCM24, WASAPI, Graph,
Stage1 or Stage2 source changes are part of rectification 3.

AUTO's original `force(safetyTarget)` remains an immediate gain-step to protect
the already inspected block. Hold remains one second. Preventive attack and
release follow the original configured timing. AUTO can change gain; it is not
presented as transparent fixed attenuation. Its manual setting is the maximum
allowed AUTO attenuation, so inadequate user capacity remains observable rather
than being silently overridden. No limiter or lookahead was introduced.

## Evidence

MSVC x64 19.51.36257, C++20, Intel IPP 2026.0 static; no mandatory AVX2 flag.
Build PASS and packaged Core/UI startup are verified separately. Red/green
regressions reproduce the ignored physical mask, excluded loud channel and
silently accepted group command before the fix. Detector unit test:

| Mask | Included input | Loud lane | Detector peak | Included lane output |
|---|---|---|---|---|
| 0x0001 | lane 0 = 0.1 | excluded lane 1 = 2 | 0.1 | 0.1 |
| 0x0003 | lane 0 = 0.1 | included lane 1 = 2 | 2 | 0.05 |

Both included lanes share the same original scalar in the second case; the loud
lane outputs 1. Tests also verify boundary adoption, repeated-mask stability,
Matrix removal, unrendered summing exclusion, DISARM invariance, binding clear,
and explicit unsupported-group rejection.

Complete configured suite: **36 PASS / 1 FAIL / 37** in 131.76 seconds. Retained
FAIL: `EntropyStage04Offline`, `unsafe PCM rejected before converter`. It expects
finite overrange to fault before PCM conversion, whereas the user-authorized
PCM24 V2 policy intentionally clips and reports it. No acceptance tolerance or
gate was weakened. The initial Stage07 manual fixture also failed because it
injected all 16 lanes directly while assuming all were routed. Its assertion now
requires exact native gain on the used mask and exact bypass on unused lanes;
it passes with no tolerance. Initial failure logs are retained.

The actual original DSP/Qt boundaries were checked by Git diff and SHA256. The
read-only code review found and corrected three offline-tool issues: ramp
settling, physical PCM pin selection and the original MUSIC-only analysis
condition. It found no further material production or numeric issue.

## Same-fragment comparison

Passive StagePublish V2 capture: 1,125 consecutive 1,024-frame blocks, six seconds
of real FP64 input at 192 kHz. The tool has no ARM/source-control commands. It
replays the unchanged profile's Crossover and Matrix and supplies identical
post-Matrix buffers to the original AUTO and MANUAL Headroom engines. All ramps
are settled before measurement. PCM counters use each DAC's actual pin mapping.
The converter is evaluated before ClockSync; no post-ASRC guarantee is claimed.

MANUAL copy changes only mode and gain to -12 dB. AUTO copies change only mode,
and the capacity setting when explicitly stated. Original profile and active
live engine settings remain unchanged during these comparisons.

| Six-second fragment / AUTO capacity | Peak before gain | AUTO gain range | AUTO peak | MANUAL peak | AUTO / MANUAL PCM overrange | Status |
|---|---:|---|---:|---:|---|---|
| First, original -3 dB | 2.75474608173 | 0.70794578438–1 | 1.95021087561 | 0.69196093050 | 173566 / 0 | FAIL AUTO capacity |
| Same first fragment, explicit -12 dB copy | 2.75474608173 | 0.27673271242–1 | 0.99999869816 | 0.69196093050 | 0 / 0 | PASS |
| Second, original -3 dB | 0.64982626310 | 1–1 | 0.64982626310 | 0.16322897731 | 0 / 0 | PASS |

For every used lane/sample in both fragments, MANUAL output is exactly
`input * 0.25118864315095801`, maximum absolute error **0** over 10,368,000
sample values per fragment. Native PCM rail-clamp counters were zero in the
reported runs; the first AUTO overrange remains explicitly reported.

Level matching is a numerical **analysis-only** reduction of AUTO RMS to MANUAL
RMS; the scale is never applied to Core or saved profiles. Safe first-fragment
AUTO scale: 0.71263585713; matched difference RMS: 0.0245053955934. This difference
tracks the changing AUTO scalar because both processors received the exact same
Matrix buffer. For the second fragment AUTO stayed at unity: matched difference
RMS 8.7161608648e-16, attributable to FP64 scale estimation/rounding. Physical
analog level matching is **BLOCKED pending calibration**; listening for this
candidate is pending the user. No claim that the midrange has been repaired.

True-peak remains **NOT IMPLEMENTED**. Historical Stage04 realtime FAIL and formal
V07 acceptance BLOCKED remain visible. The V07 integration test passing in this
suite does not constitute the separate formal acceptance.

## Reproduce and test

`tools/Validate-Stage012.ps1 -BuildFolder build/msvc-headroom-mask` runs build,
targeted offline regressions and `--probe`. Add `-FullSuite` for all configured
tests; it retains the known FAIL. Add `-Profile <private entropy file>` to capture
and compare a real fragment passively and make private comparison profile copies.
No DAC is opened by the script. Results and raw captures remain under `local`.

For live audition use the separate `release/0.1.1` `Live-Test.cmd`. Start DISARMED,
retain/load your own profile and ARM explicitly. Comparison copies are optional;
their fixed reserve was demonstrated on these fragments only, not on arbitrary
future material. Equalize analog levels before judging AUTO versus MANUAL.

The accepted 0.1.0 reference, Stage1 v0002 and Stage2 v0008 remain unchanged. New
Headroom candidate sources are on `codex/stage012-headroom-render-mask`; they are
not pushed or merged as part of the earlier accepted-version publication.
