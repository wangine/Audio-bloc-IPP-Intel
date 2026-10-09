# Stage011 — Stage1 / Stage2 / Stage3 lifecycle independence

Date: 2026-10-09. Baseline: `7222030a9473a8e141d55313f785c06c5a336553`.
Branch: `codex/stage011-complete-independence`. Local delivery only; no push or merge.

**PASS: the new Stage1 continues processing while Stage2 and Stage3 are stopped,
terminated or reconnected. PASS: real FP64 input reaches the unchanged Core.**
The complete legacy suites still contain failures, listed below. This report does
not declare those suites or historical realtime/hardware acceptance to be PASS.

## Delivered versions and startup

The original Stage1 release, Stage2 v0006/v0007, all previous Core releases and
stable Stage3 are preserved.

| Component | New release directory | Launcher |
| --- | --- | --- |
| Stage1 | `D:\Projects\Entropy Codex Lucru\Modular 3 stages\Etapa 1 Sources\releases\stage1-passive-publish-v0002-20261009` | `ColdStart-Source.cmd` |
| Stage2 | `D:\Projects\Entropy Codex Lucru\Modular 3 stages\Etapa 2 DSP\versions\v0008-passive-source` | `ColdStart-DSP.cmd` |
| Core/Qt | Existing `D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.0\EntropyAudioCore-0.1.0-beta-win-x64` | `Live-Test.cmd` |
| Combined startup | `D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\stage011-independence` | `ColdStart-All.cmd` |

Stage1 retains `Stage1/runtime/stage1`. Stage2 retains its existing flat runtime,
`modules` and local `state`. The source snapshots and manifests accompany each
new release. The combined launcher starts independent processes; it never stops
an upstream process when another exits and refuses to choose between old or
duplicate instances. Reconnecting to the currently running new instances was
tested successfully. Individual fresh startup was exercised by the live test.

Order: new Stage1 → Stage2 v0008 → Core 0.1.0 → Qt. New Core starts DISARMED.
The launchers do not send ARM. `Live-Test.cmd` and the combined launcher retain
the previously authorized manual live-test session; the user must explicitly ARM
in Qt. No physical DAC was opened during this stage's verification.

The user's running Core profile, including device assignments, was saved locally
before DISARM/shutdown and loaded into the handover Core. It is not in Git. New
Stage2 inherited the saved local v0007 state. No unsaved Stage2 UI edit was claimed
to have been saved during the transition. Normal profile controls remain local.

For rollback, explicitly close the new instances and launch the old compatible
pair. Old V1 Stage2 cannot consume new passive Stage1: there is no V1 fallback.

## Architecture and code changes

```text
Stage1 original capture / SRC / matrix, original local cadence
  → passive StagePublish.Source.V2, FP64/192 kHz, MIX_CINEMA10
Stage2 passive subscriber, original EQ / raw Center / LRC processing
  → passive StagePublish.V2, FP64/192 kHz, LRC_CINEMA11
Unchanged AudioCore 0.1.0 → unchanged DSP Graph / independent egress
Unchanged Qt → IPC / telemetry only
```

Stage1's controller no longer permits V1 demand/lease takeover. The existing
local SourceDemandHarness / SourceFrameDemand / SourceBlockExecutor remain the
processing cadence. V1 Attach/Release and external BusMode are explicitly
refused. Subscriber closure, full queues and liveness affect only that subscriber.
Publisher connection servicing uses synchronized control boundaries, preserving
the processing epoch/deadline across publication-only pauses. An optional GUI
`--status-file` records read-only diagnostics outside the audio callback.

Stage2's production controller/AutoLink/runtime subscribe passively and execute
one original DSP block per valid publication. Gaps, invalid data and producer
loss invalidate/reconnect only local state. Source generation changes reset
local DSP history; repeated cumulative discontinuity flags do not repeatedly
reset it. Production no longer contains DspLocalDemand/DspDownstreamLink or
StageLinkClient upstream request/fault calls; legacy implementations remain
explicit test fixtures.

The V2 transport accepts ten or eleven planes. Queue ABI remains **1,444,480
bytes**, sixteen slots, four subscribers, eleven-plane preallocated storage.
Only the declared channel planes are copied. Existing 128-byte block headers,
sequence/frame/QPC/session/generation/control metadata are preserved verbatim.
The header has no program field: no new program field or guessed program was
introduced. The native transparent executor's original epoch-one contract uses
a local view; actual input epoch is restored unchanged in the output header.

No Crossover, Matrix, Headroom, ClockSync, Guardian, PCM24, WASAPI or Core/Qt
algorithm change. No new SRC/interpolation, gain, normalization or AVX2 mandate.
Stage1 UI visuals are original; its historical StageBus V1 label is still present,
while its new production output is V2.

## Actual live lifecycle proof

Evidence: `validation/stage011/live-independence.json`, `live-console.txt`,
`handover.json` and `combined-launcher.txt`. This used the actual running music
source and production executables, not silence or a synthetic publisher.

Source UUID throughout: `fecf21a5bc56cb4e96dd08a61571f8be`.

| Observation | Stage1 completed blocks | Stage1 faults |
| --- | ---: | ---: |
| Stage1 alone | 45 | 0 |
| Stage1 alone continues | 120 | 0 |
| Stage2 connected | 652 | 0 |
| Stage2 clean stop | 893 | 0 |
| Stage2 restart, no Source Retry | 1129 | 0 |
| Stage2 forced process termination | 1204 | 0 |
| Stage2 restart after termination | 1433 | 0 |
| Stage3 connected, DISARMED | 1481 | 0 |
| Stage3 closed, upstream continues | 1564 | 0 |
| Stage3 reconnected | 1612 | 0 |
| Handover to Stage2 GUI + Core/Qt | 51900 | 0 |

Handover Core had fresh three-channel MUSIC peaks
`0.5879 / 0.6027 / 0.2142` (the detailed snapshot contains
the measured values for each phase). Core input reported zero transport faults,
zero release failures and the same source UUID. GUI was inspected: input and
Graph meters active, output DISARMED. No source retry, source command or session
rotation occurred. `publishDrops=97` at handover is retained in telemetry: these
missed subscriber deliveries during long build/lifecycle activity did not stop
Stage1. No uninterrupted playback/deadline claim is inferred from this test.

## Build, tests and retained failures

Stage1 and Stage2: Qt 6.11.2, MinGW GCC 13.1.0, C++20, Release/Ninja.
Both full builds PASS. Both final source snapshots independently configured and
rebuilt their production executable targets PASS (39 build steps each).
Both package `--verify-only` processes exited zero.

| Check | Result |
| --- | --- |
| Stage1 complete CTest suite | 33 PASS / 4 FAIL of 37 |
| New SourcePassivePublisher | PASS: absent/full/slow/closed/terminated subscriber, twelve reconnects, UUID fixed, exact offline FP64 |
| Stage2 complete CTest suite | 21 PASS / 23 FAIL of 44 |
| New DspPassiveInput | PASS: real 10 → 11 planes, exact payload/header, gap/epoch/restart remain local |
| New DspPassiveNative | PASS: original native EQ reference, raw Center tap, LRC bypass, Cinema signed-zero/NaN bit preservation, generation resets |
| New DspPassiveAllocation | PASS: 1000 warmed cycles, zero audited alloc/free/wait/lock/I/O, hook coverage demonstrated |
| V2 transport MinGW / MSVC | 1/1 PASS each; MSVC 19.51.36257 |
| Cross-process MSVC ↔ MinGW | 1/1 PASS in each direction |
| Unchanged Core functional regression | Final 31/31 PASS |
| SHA256 protected sources and native DLLs | PASS |
| Actual independent live lifecycle + handover | PASS, no DAC |

Realtime audit scope is the warmed Stage2 block step, including passive queue
publication/read and original enabled native modules. Control servicing, lifecycle
changes and explicit history reset are excluded; this is not a whole-process
zero-allocation claim. Source publication servicing is outside the audio callback.

Stage1 retained failures: `SourceApp`, `SourceLink`, `SourceIndependence`,
`SourceLiveLocalControl`. Their expectations exercise the retired V1 external
ownership path. They remain registered and unweakened.

Stage2 retained failures: `DspBusIntegration`, `DspLiveness`, `DspAllocation`,
`DspLifecycle`, `DspProcess`, `DspPendingFailure`, `DspRestartProcess`,
`DspPeerDeathPending`, `DspSourceIsolation`, `DspPublishedOutput`,
`DspCenterRoute`, `DspCenterController`, `DspCenterUi`, `DspCenterAllocation`,
`DspCenterProcess`, `DspLrcRoute`, `DspLrcController`, `DspLrcUi`,
`DspLrcAllocation`, `DspProfileUi`, `DspEqRoute`, `DspEqUi`, `DspEqAllocation`.
Twenty-two require retired V1 attach/demand/ownership; `DspProfileUi` additionally
pins the v0006 window title. No failure was removed, skipped or turned into PASS.

The first Core functional run was 30/31: `EntropyStage07Live` failed “explicit
lane order lost”. Unchanged isolated retry passed, then a complete run after
concurrent builds finished passed 31/31 in 50.83 s. Original failure log is
retained; scheduling causation was not proven. Initial cross-compiler tests
failed with missing MinGW DLL search path (C0000135/child readiness timeout);
reruns with the dependency PATH passed. Their initial failures are also retained.

Historical Stage04 realtime qualification and V07 formal acceptance were not
rerun or reclassified. No performance benchmark or physical listening test is
claimed by this transport-independence change.

## Integrity and SHA256

Stage1: 247 original production source files byte-identical. Only five integration
files changed: top/host CMake, SourceController.cpp/.h, SourceMain.cpp. Original
capture, SRC, executor and UI sources remain intact. Stage2's ten protected
executor/module/alignment source files match the v0007 snapshot. All eleven
native Stage1/Stage2 module DLLs match original manifest SHA256. Core DSP,
Engine/Egress, IPC, Devices and UI diff is empty against the baseline.
All 91 native-source files outside the original replay build tree also match
the original release byte-for-byte. Git whitespace checking reports inherited
whitespace in copied original/vendor source and verbatim build logs; those
sources and evidence were preserved rather than reformatted.

| Executable | SHA256 |
| --- | --- |
| Stage1 new | `676746BAB183748AA92D9FDE4E11F066F5E66459D81A2C32F9CD76F77F1F36BB` |
| Stage2 v0008 | `79945155C782C0677A8AEA2FD405A996AB464C5B2C2481C16BADF4F0738F807C` |
| Core 0.1.0 unchanged | `1F17AA286CE556E94271FFDAE74C8F762340670C7BA639F9BEC80812B150DA7C` |
| Qt unchanged | `431404FDDD05D6FE174944AA11FDB32EFB6E8ABAEEEC4209049DA4A973381074` |

Reproduction: `Integration/Stage1/tools/Build-Stage1Passive.ps1`,
`Integration/Stage2/tools/Build-Stage2V0008.ps1`, package scripts, protected-file
check `Tests/Stage011Integrity.py`, lifecycle check
`Tests/Stage011LiveIndependence.py`. The latter deliberately starts only owned
test children, leaves them DISARMED on PASS and requires fresh evidence folders.
Close conflicting versions explicitly first; never run two source owners.

Raw build/test/live logs and preserved private configuration remain under
`local/stage011`. Relevant shareable logs are in `validation/stage011`.
Runtime binaries, native libraries, Intel SDK, build trees and user profiles are
excluded from Git. New release manifests identify the final local source commit.
