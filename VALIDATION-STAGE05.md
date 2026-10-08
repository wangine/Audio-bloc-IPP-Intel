# Stage05 — AudioCore control plane and original Qt interface

Native baseline: `1fb726bc103cbb630ea55ca27647e85d897c637d`. Branch: `codex/stage05-control-plane-qt`. No push or merge. No Stage2 or physical DAC connection.

## Gates

| Gate | Result | Evidence |
|---|---|---|
| Native MSVC/IPP build and configured regressions | PASS, 27/27 | validation/stage05/native-suite.txt |
| FP64 controlled graph / original native path | PASS, 67,584,000 samples, absolute/RMS error 0 | control-benchmark.csv; active peak 0.0019999874512689774 |
| Block-boundary safety controls and processor isolation | PASS | Tests/Stage05BoundaryTests.cpp; failing legacy oracle retained locally |
| Native callback allocation/deallocation/locks/waits/filesystem imports | PASS, 8,801 audited callbacks, all counters zero | local/stage05/controlled-validation.txt |
| Actual Core process IPC, profiles, device state, fake DAC workers | PASS, 7 integration cases | EntropyStage05IpcIntegration CTest |
| Existing Qt pages and actual EntropyUI executable | PASS | qt-controls.txt, qt-executable.txt; original widgets/screens captured locally |
| Stage04 numeric/clock regressions | PASS | stage04-numerical.csv, stage04-clock.csv; no tolerance changes |
| Stage04 realtime gate under IPC | **FAIL** | 1 deadline miss in 800,000 measured callbacks; ipc-load-deadline-misses.csv |
| Physical ARM / V07 formal acceptance | **BLOCKED** | Stage04 remains FAIL; V07 reference absent |

## Implementation and boundaries

`Engine/AudioGraph.cpp`, `AudioScheduler.cpp`, `ParameterSnapshot.cpp` and `Telemetry.cpp` remain separate. DSP stays in its Crossover/OutputMatrix/Headroom directories; Egress keeps original ClockSync, Guardian, PCM24 and WasapiSink modules. `IPC/CommandServer.cpp` and `TelemetryServer.cpp` carry explicit versioned little-endian frames/strict JSON, with same-user local named-pipe ownership. Main selects the native server or the original offline probe.

Core owns every actual native parameter, original MUSIC/CINEMA scenes, Matrix routes, Headroom policy/timing/groups, device approval/configuration and profile Save/Load. Validation rejects invalid candidates before live changes. Matrix/Headroom use a bounded preallocated 64-snapshot queue, one state per block. Save waits for previous audible Crossover and queued module states even when END gesture follows SET. Load validates native blobs/checksums/device approvals on candidates, joins owners, installs or rolls back, and remains DISARMED. Stable Stage3 profile paths are rejected. Unknown project template fields remain opaque and preserved.

The existing Qt presentation sources are copied under UI/Legacy. RenderController, module DLL loading, audio objects and authoritative device QSettings are removed from the copy. The separate MinGW client links Qt Core/Gui/Widgets/Network only. Requests are asynchronous, bounded, correlated and not replayed across connection loss. Original delay control quantization is retained exactly: native normalized float control 3 ms reads 3.00006103515625; this is unrelated to FP64 audio arithmetic and is tested without a relaxed numerical tolerance.

Physical sink threads are never enabled by this stage's control server. Explicit `--offline-test-signal --offline-test-sinks` permits two separate preallocated eight-channel pipelines and owner threads, carrying synthetic independent +100/-100 ppm clock observations, original ClockSync and PCM24, exact Guardian bypass, and no WASAPI construction. `offline-arm` requires a declared safe Headroom policy; normal ARM always returns the Stage04 prohibition. The production default reports WAITING FOR STAGE2 and DISARMED.

## Integrity classification

85 protected sources were checked. Every protected implementation and C ABI is byte-identical to the Stage04 baseline. **One Crossover header integration seam is deliberate:** an optional owner/processor-specific immutable parameter view freezes the existing immediate mute/topology/safety reads for the whole block, plus read-only role activity. The unset path retains original behavior. Removing precisely these additions reproduces the baseline header exactly. No coefficient, formula, normalization, tap count, interpolation/controller law or phase/latency computation was edited. Generated native control adapter equals the original compatibility source after only unique_ptr→shared_ptr storage and exported-symbol renaming.

Original Headroom MUSIC analysis is called on the Matrix FP64 buffer before the original gain actuator, matching stable RenderBlockExecutor. Existing regression fixtures leave this host integration disabled. Source ownership and telemetry publication changes are not DSP redesigns.

All 24 original stable Qt source files and four stable Stage3 runtime binaries are unchanged. SHA256 manifests and Qt PE imports are in validation/stage05. UI status text identifies offline/unavailable source/clock/true peak and Guardian exact bypass instead of fabricated measurements.

## Measurements and their limits

Control benchmark: MUSIC/CINEMA, 128/256/512/1024 frames, 1,000 retained samples per cell; median/p99/max and dispatch cycles/context switches in control-benchmark.csv. It validates a non-FIR probe plus controlled Matrix summing; it does not authorize live FIR8193×16. Existing Stage03 FIR8193 deadline failure remains in force.

Under 5,250 real IPC telemetry requests, Stage04 measured 800,000 callbacks over 80 module/program/format/block combinations. MMCSS Pro Audio and critical task priority were successfully registered. One complete-chain CINEMA/PCM24-in-32/128 callback took **745.1 us**, above its **666.667 us** deadline; 317,400 thread cycles and 10 context switches were observed in the surrounding profiling bracket. This establishes descheduling in the bracket, **not** exact execution-versus-descheduling durations. No new kernel ETW capture is claimed; prior timestamped Stage04 ETW findings remain in VALIDATION-STAGE04-BLOCKERS.md. QPC telemetry is explicitly callback wall time, not on-CPU execution cost. The deadline gate is unchanged and remains FAIL.

Import-hook audit covers C++ allocation operators and named Windows imports of images loaded at installation; it does not prove absence of dynamically resolved syscalls or every IPP internal operation. Positive controls triggered every audit class before reset. Original IPP initialization/filter preparation stay off callbacks; source inspection and numeric regressions complement the measured audit.

## Reproduction and launch

```powershell
./tools/Build-Windows.ps1 -BuildFolder build/msvc-stage05 -Stage02 -Stage04 -Stage05
./tools/Build-Stage05-UI.ps1
./tools/Test-Stage05-UI.ps1
python ./tools/Verify-Stage05.py
python ./tools/Benchmark-Stage05.py # Retains failing realtime gate; exit 1 means FAIL.
./tools/Run-Stage05.ps1 # Core first, existing Qt UI second; no physical ARM.
./tools/Run-Stage05.ps1 -OfflineTestSignal # Explicit internal signal; no physical output.
```

MSVC x64 19.51.36257.0, Intel IPP static 2026.0; UI Qt 6.11.2 / GCC 13.1.0. No unconditional AVX2/FMA3. Built executables/build folders/vendor SDK are local only. Full raw logs, sample-level benchmark data, captures and debugging evidence are retained locally; compact evidence is tracked.
