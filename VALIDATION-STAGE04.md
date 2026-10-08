# Stage04 — Native Audio Egress: execution report

2026-10-08, Windows x64. Baseline `429589ad0831f034384b345eb6f3b875a8b49888`;
branch `codex/stage04-native-egress`. No push or merge. No physical endpoint
enumerated/opened by the executed tests. No Stage2 or Qt connection.

**Stage04 = PASS NUMERIC / PASS OFFLINE LIFECYCLE / FAIL REALTIME DEADLINES /
FAIL CLOCK DRIFT ESTIMATION. Hardware test authorization = BLOCKED.
Formal V07 acceptance = BLOCKED.** The software builds; it is not approved for live output.

## Implemented architecture

One Graph owner receives **11 FP64 channels at 192 kHz: L/R/C followed by the
eight CINEMA channels, in that order**. No input SRC or nominal sample-rate
conversion. A pointer adapter adds zero to the two unused legacy slots of the
unchanged 13-input Crossover ABI. The Graph produces 16 outputs. The first-DAC
probe selects outputs 0/1. Stage2 is represented by this transport contract,
without connecting the Stage2 process.

`AudioGraph → preallocated SPSC FIFO → original ClockSync → exact Guardian
bypass → original PCM24 → original WASAPI Exclusive`.

`DacPipeline` owns each DAC's FIFO, clock engine/controller/history, PCM engine,
buffers, fault state and XRUN counters. Graph publish and DAC consume/render
have different owners. The current executable exposes the first DAC only;
DAC B hardware and simultaneous dual-DAC routing are not exercised. A second
DAC must consume the same Graph's selected outputs through another pipeline,
not another Graph. Qt remains an IPC-only architectural boundary; no Qt/DSP
dependency or IPC implementation was added here.

The pinned ClockSync retains its original fractional clock correction. Its
nominal source engine and vendor dependencies are linked unchanged; the host
accepts only 192000→192000, using the original wire path. Linking those sources
does not enable input SRC or an alternative interpolator.

Preparation/warmup/reset are control operations after both owners have stopped.
SPSC publication uses lock-free release/acquire indices. Overflow/starvation
latch FAULT and increment XRUN; no overwrite of unread data. DISARM preserves
FAULT. Recovery requires joined owners, prepare and a new ARM. The sink owner
performs open/start/event wait/submit/stop/close and COM lifetime on one thread.
Thread construction/worker errors and sink-open exceptions are contained.

The only prepared physical probe is fixed stereo, internal 1 kHz sine, peak
0.01 FP64 (−40 dBFS), declared manual Headroom attenuation **20 dB**, output
approximately 0.001 (−60 dBFS), no filters/delays/positive gain, dither NONE.
The bounded ledger applies only to this restricted routing and input bound;
external offline test inputs exceeding 0.01 are rejected. The 1 ms Headroom
ramp is settled with zero input before ARM. Arbitrary production profiles and
FIR8193×16 are not authorized. PCM out-of-range/nonfinite blocks are rejected
before conversion and replaced by silence; rail-clamp telemetry stays zero.
Guardian processing is not enabled; bypass preserves bits including −0.0.

## Executed tests and numeric results

- MSVC x64 Release / Intel IPP static build: **PASS**.
- All **19/19 CTest tests PASS**, final suite 98.68 s. These include the ten
  Stage01 tests, the configured Stage02/03 regressions and four Stage04 tests.
- `EntropyAudioCore.exe --probe`: **PASS**, no hardware or IPC started. Its
  printed 13-input count describes the preserved internal Graph ABI.
- Stage04 numerical comparison: **96/96 PASS**: MUSIC/CINEMA × packed PCM24/
  24-in-32 × 128/256/512/1024 frames × sine/impulse/deterministic noise × two
  active DAC channels. A separately executed original Graph/ClockSync/PCM24
  oracle uses explicit ×0.1 Headroom policy; no candidate output serves as oracle.
- Maximum absolute error **0**, RMS error **0**, FP64 bit differences **0**,
  PCM byte differences **0**, phase difference **0 rad**, latency difference
  **0 samples**, NaN/Inf **0**. Both channels are explicitly checked active.
- Measured sine fundamental amplitude 0.000999432201504717–
  0.0009999607380858915, equal to the reference. This is a finite-window sine
  projection, not a claim of additional normalization.
- Absolute first active output index 2176–34688 is recorded per case. This
  includes preroll and sparse impulse arrival; it is not a universal transport
  latency. Candidate/reference first-active indices match exactly.
- Fake sink: startup/stop/DISARM, open/start/wait/submit errors, device loss,
  partial submit, invalid driver size, starvation, FIFO overflow, nonfinite
  source, PCM rejection and explicit recovery **PASS**. Graph256 with negotiated
  render128/256/512/1024 **PASS**. No automatic endpoint substitution or restart.
- Standalone SPSC concurrent test: **200000 exact samples PASS**, wraparound,
  signed zero and XRUN counters checked. Graph/Sink test: 2000 synchronized
  renders plus **2000 overlapping renders**, 1996 concurrent producer calls.

The first intentional RED checks remain recorded: missing Headroom policy and
missing FIFO header. The audit self-test intentionally records nonzero allocation,
wait/lock/I/O counters, then resets before measuring production callbacks.
Those positive controls must not be mistaken for callback failures.

## CPU benchmark

80 configurations, 2000 warmup + **10000 measured callbacks per configuration**,
192 kHz. Five modules × two programs × two PCM formats × four block sizes.
Nearest-rank p99 and median are calculated from all measurements. CSVs retain
every result and FAIL. The complete path includes sine generation, Graph,
FIFO push/pop, original ClockSync, Guardian bypass, preflight and PCM24;
it excludes device event waits and hardware I/O.

**77/80 rows PASS, 3/80 FAIL, 11 deadline misses.** All failures are Complete/CINEMA:

| PCM format | Frames | Median µs | p99 µs | Maximum µs | Budget µs | Misses |
|---|---:|---:|---:|---:|---:|---:|
| packed24 | 256 | 65.000 | 179.500 | 4891.700 | 1333.333 | 1 |
| packed24 | 512 | 135.300 | 287.400 | 4157.200 | 2666.667 | 1 |
| 24-in-32 | 256 | 62.000 | 148.300 | 29559.500 | 1333.333 | 9 |

[All module/block summaries](validation/stage04/benchmark-table.md) show ranges
over MUSIC/CINEMA and both formats. [All 80 rows](validation/stage04/benchmark.csv)
retain each program/format separately, including median/p99/maximum thread cycles.
The complete 800000 timing samples are preserved locally in the raw archive.

QPC reports elapsed time, including scheduling delays. QueryThreadCycleTime
reports thread execution cycles, including measurement/audit overhead. No empty
measurement-floor subtraction, frequency conversion or CSwitch tracing was used;
**exact DSP-only cost and scheduling delay in µs remain BLOCKED/unmeasured**.
The largest QPC samples cannot be attributed exclusively to DSP or scheduling.
Tests ran under the existing PC workload; no other audio apps were stopped.
These results do not authorize FIR8193×16; the Stage03 deadline FAIL remains.

## Realtime and clock limitations

**7996 two-owner callback scopes and 800000 benchmark scopes**: zero C++
new/delete, Windows heap alloc/free, imported waits, locks and filesystem calls.
The original Stage02 realtime regression also passes unchanged. Hook coverage is
host C++ operators and loaded named imports; dynamic resolution, direct syscalls
and newly loaded images are outside coverage. This is not an OS-wide absence proof.
The sole `ippInit()` source call is FFT construction during control-thread prepare,
not Graph/render. No source DSP algorithm or reset implementation was changed.

WASAPI's event wait is explicitly outside Graph/render, on the DAC owner. Join
and producer pacing are outside callbacks. A lost device cannot receive a software
silence buffer; stop/close and latched FAULT prevent further useful submission.
Already queued/in-flight hardware audio cannot be retrospectively removed.

Clock diagnostic: two separate original estimators, simulated −100/+100 ppm,
20000 observations each at every tested block size: **8/8 FAIL_NO_VALID_ESTIMATE**.
The retained 512 observations span at most approximately 0.34/0.68/1.36/2.73 s;
the original estimator requires 3.9 s of a 4 s window. No valid rate/confidence
is produced. Numeric equivalence to this original does not establish valid clock
tracking. The three-second hardware probe cannot validate long-term drift.
The diagnostic checks availability of a valid estimate; rate accuracy/sign would
need further validation after the original limitation is resolved. No change to
ClockSync mathematics was made. **No offline PASS gate was generated.**

No mandatory AVX2/FMA3 flag was introduced. Original SoXR builds three optional
AVX dispatch objects; the Stage04 nominal wire path does not execute conversion.
IPP retains CPU dispatch. Compatibility on an older SSE2-only/AVX-less CPU has
not been executed; no fallback FIR/SRC implementation was introduced.

## Integrity, build and reproduction

MSVC **19.51.36257.0**, VS Community toolset 14.51.36231; CMake 3.30.5 / Ninja
1.12.1; Intel IPP 2026.0 installed locally at
`C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0`. CPU Intel Core i7-9700F,
8 cores / 8 logical processors. Static C++20 egress integration.

**85 immutable files byte-identical** to Stage03 baseline: DSP, Egress, Devices,
Dependencies and Shared. The compiled original nominal-wire source also matches
its staged copy. **Four stable Stage3 binaries retain their baseline SHA256.**
Only host integration, CMake, tests, reproduction scripts and reports changed.

`EntropyAudioCore.exe` SHA256:
`D0507DE639D3F5C3C8E07CD864ADB7F2F3FE643978D6636B66DAAB788DA62D9E`.
Executable/source/stable hashes and build/test logs are in `validation/stage04`.
Executables, vendor dependency extraction, Intel SDK/libraries and build folders
remain local and ignored by Git.

From this repository, using installed MSVC/IPP and access to the pinned private
reference repository:

```powershell
./tools/Run-Stage04.ps1
./tools/Summarize-Stage04.py # run with an installed Python, or bundled runtime
./tools/Archive-Stage04.ps1
```

The runner prepares pinned references, builds, executes all CTest tests, probe,
numeric/lifecycle/audit/clock diagnostics and the complete CPU benchmark.
Exit **1** is expected for the recorded deadline failures; exit 2 from the harness
means a processing/test error. It invalidates any previous gate first and writes
one only if all required measurements, including clock diagnostics, pass.
It never executes the hardware mode.

Local raw archive: `local/stage04-archives/results-20261008-230329.zip`.
SHA256 `4D63384107E5BA3D352082986A7BE964506B79881CC44BD4647E58A253A4DB4F`.
Formal V07 acceptance is still BLOCKED; native comparisons and source integrity
do not substitute for the separately required V07 acceptance contract.

## First DAC probe instructions — currently BLOCKED

Do not run this command on the current FAIL result. Once all offline requirements
pass and the runner creates a gate matching the current AudioCore SHA256, a separate
explicitly authorized test can use an exact endpoint ID and driver-supported format:

```powershell
./build/msvc-stage04/EntropyAudioCore.exe --hardware-test --arm --endpoint '<exact endpoint ID>' --format packed24 --frames 256 --offline-gate ./local/stage04-results/offline-pass.gate
```

Use `24in32` only if that exact format is supported. No shared-mode, alternate
endpoint or rate fallback. The negotiated driver buffer must fit the prepared
maximum 1024 frames. Duration is three seconds; Ctrl+C requests stop, both owners
join, and the sink closes. This is the bounded stereo −60 dBFS probe only, not a
live production/FIR/Stage2 profile. No hardware run occurred in this stage.
