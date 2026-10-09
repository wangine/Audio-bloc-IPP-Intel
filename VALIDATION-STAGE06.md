# Stage06: real Stage2 input and independent Core/UI

2026-10-09. Baseline `8bb68f9`; branch `codex/stage06-stage2-ingress`.
User explicitly authorized Stage2 input, with no physical DAC execution.

## Result

| Gate | Result | Evidence |
|---|---|---|
| Native MSVC/IPP build and probe | PASS | `validation/stage06/probe.txt` |
| Complete configured native suite | PASS, 30/30, 104.15 seconds | `validation/stage06/native-suite.txt` |
| Original Qt pages, controls, profile cache and executable IPC | PASS | `validation/stage06/qt-regression.txt` |
| New-PC profile prompt, cancel and reopening | PASS | `validation/stage06/qt-first-run.txt` |
| Real running Stage2, original transport, native Core, existing Qt meters | PASS | `validation/stage06/live-verification.txt`, `live-summary.json` |
| UI exit, same-engine reconnect and retained parameters | PASS | Native Stage06 integration test and launcher reuse test |
| Protected DSP/egress/source integrity | PASS | 88 files compared with baseline; SHA256 manifest |
| Stable Stage3 integrity | PASS | 28 previously recorded files rehashed; no differences |
| Physical DAC authorization | BLOCKED | Existing Stage04 FAIL REALTIME remains; no sink opened |
| Formal V07 acceptance | BLOCKED | Reference acceptance remains unavailable; this work does not resolve it |

Toolchain: MSVC x64 19.51.36257, Intel IPP static 2026.0, C++20. Qt 6.11.2,
MinGW GCC 13.1.0, separate process and build. No mandatory AVX2 added.

## What changed

`Engine/Input/Stage2Ingress` receives the existing same-user StageLink lease and
StageBus ABI on a dedicated transport worker. The copied 18 transport files are
byte-identical to stable Stage3 originals. Payloads remain 11 FP64 planes at
192 kHz, 1024 frames, L/R/C followed by eight CINEMA planes. The two unused graph
ABI inputs are positive zero. An allocated-before-start SPSC queue passes blocks
to the single graph thread. Queue capacity is four; the worker targets at most
one queued block. Graph parameters adopt at existing processing boundaries.

No transport requests, peer waits, lease management, filesystem or allocation
occur in `tryRead`. Processing-scope audit tests pass with zero allocation,
deallocation, locks, waits and I/O; audit positive controls remain visible in test
output. This is scoped evidence, not a new end-to-end physical realtime guarantee.
No-data polling waits outside processing and does not inject synthetic zeros into
DSP histories. Producer loss invalidates the queue generation and retries with
a fresh lease. Busy original render ownership is retried without stealing it.

Qt only receives IPC snapshots. It owns no StageBus, DSP, WASAPI, ClockSync or
audio buffers. Existing visual pages remain in their separate files. Closing Qt
does not stop Core or erase its parameters. The launcher reconnects to a compatible
Core and refuses a conflicting executable or offline/live input mode.

Real-input startup loads the exact Windows Documents `entropy.entropy` through
the native validator. A missing/invalid profile leaves Core alive and waiting,
without attaching Stage2. Qt offers Create / Load / Keep waiting. Ctrl+Shift+P
reopens the choice after cancellation; SAVE AS can also create the first profile.
New profiles use original defaults plus explicit Manual Safe -6 dB, DISARMED.
Existing loaded profiles retain their original gains. A profile saved elsewhere
must be selected explicitly on the next startup if the mother file is absent.

Save/load while the producer is absent uses a joined scheduler and a control-only
quiescent boundary. Matrix/Headroom snapshots can be persisted without processing
invented samples. Pending audible Crossover application remains explicitly pending.

## Actual user-source observation

The running original `EntropyDspEngine.exe`, PID 24620, delivered valid CINEMA
blocks. The captured sample had 11 received/consumed blocks, zero transport faults,
fresh meters and CINEMA peaks 0.268414169549942, 0.2593017816543579,
0.27131104469299316 and 0.0268718171864748. L/R/C were zero in this observation;
they are not claimed as active. The deterministic fixture separately verifies all
11 lanes, bit identity including signed zero, MUSIC/CINEMA meter propagation,
starvation, producer recovery, ownership recovery and parameter persistence.

The mother profile was loaded unchanged from
`C:\Users\bobi\Documents\entropy.entropy`. It selects MUSIC, while the observed
source delivers CINEMA. The captured existing UI shows active CINEMA input bars
and quiet graph output for that selection. The test did not change the user's
program or routing. DAC A/B remain DISARMED; FIFO/clock estimates are not invented.

The actual Qt window was rendered and captured using the application's own
`--capture-ui` flag. The screenshot and full raw telemetry remain local in
`local/stage06/live-ui.png` and `live-stage2-telemetry.json`.

## Failures found and fixed

The initial ingress red test delivered no blocks. The initial profile red test
exited instead of keeping a missing-profile Core available. The first full suite
then failed Core startup with STATUS_STACK_OVERFLOW (3221225725): the preallocated
ingress object exceeded the default MSVC stack. It is now heap-owned before thread
startup, with no callback allocation. Original failure logs are preserved under
`validation/stage06`; no tolerance or acceptance gate was weakened.

Fresh-context review found an absent-producer save barrier timeout, no retry after
busy render ownership, offline launcher reuse of a live owner, and a cancelled
profile prompt that could not reopen. Each was corrected and exercised by passing
regression/launcher tests. No DSP mathematics changed.

## Reproduction

From the repository in PowerShell:

```powershell
tools/Build-Windows.ps1 -BuildFolder build/msvc-stage06 -Stage02 -Stage04 -Stage05 -CompileOnly
C:/Qt/Tools/CMake_64/bin/ctest.exe --test-dir build/msvc-stage06 --output-on-failure
tools/Build-Stage05-UI.ps1 -BuildFolder build/qt-stage06
tools/Test-Stage05-UI.ps1 -NativeBuild build/msvc-stage06 -UiBuild build/qt-stage06
tools/Test-Stage06-UI.ps1
cmd /d /c Cold-Start.cmd -SmokeTest
```

`Cold-Start.cmd` normally starts/reconnects Core with real Stage2 input and opens
the existing Qt UI. Core automatically validates the mother profile. Closing UI
leaves Core running. To stop Core deliberately, close Qt and run
`Stop-AudioCore.cmd`; this releases Stage2 ownership gracefully. No physical ARM
is authorized by either launcher. Do not start stable Stage3 as a competing owner.

## SHA256 and local evidence

EntropyAudioCore.exe:
`7F90EB4CD7305A5B71B7C20A734DC3DFD3645F4FFEEA73443692F30C00AEFC79`

EntropyUI.exe:
`0A80E71C8D298384DB8887645597D86507D27545A48F5C418B76F0D1B1271562`

The manifests record exact working-file hashes and repository-normalized hashes
for protected sources (Windows CRLF accounted for), copied transport hashes,
stable Stage3 hashes and the unchanged user-profile hash. The user profile itself
is not committed. All raw builds, test logs and screenshots remain in ignored
`local/stage06`. No binaries, Intel libraries, SDK installation or build directory
are included. No push or merge is performed.
