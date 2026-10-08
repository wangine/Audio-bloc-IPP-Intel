# Stage04 blockers: clock history and realtime attribution

Status: **PASS CLOCK / PASS NUMERIC / FAIL REALTIME / BLOCKED HARDWARE**.
Formal V07 acceptance remains **BLOCKED**. No hardware output was run.

This follow-up starts at Stage04 `c02f897cf39292927836284aed32b926560edade`, on
`codex/stage04-native-egress`, retaining Stage03 baseline
`429589ad0831f034384b345eb6f3b875a8b49888`. Measurements were made on the local
Windows PC on 2026-10-08/09. Historical failures in `VALIDATION-STAGE04.md` and
`validation/stage04/` remain intact. No push or merge is part of this work.

## Scope and source integrity

The only production-source edit is the estimator's preallocated observation
capacity, **512 -> 8192**. Its original regression, confidence calculation,
4-second window, interpolation and controller law are unchanged. At 128 frames /
192 kHz, 8192 observations span approximately 5.46 seconds, sufficient for the
original estimator; the diagnostic feeds 20,000 observations per independent
clock and block size, including history wraparound. Initialization remains outside
processing; no dynamic observation storage was introduced.

All remaining edits are test instrumentation, CMake test integration, reproduction
scripts, integrity checks and evidence. AudioGraph, Headroom, PCM24, Crossover,
FIFO processing, sink processing and public engine/IPC ABI are unchanged from
Stage04. The reference checker accepts exactly the capacity substitution in that
one header, rather than waiving source integrity: **84/85 files byte-identical,
1/85 exact authorized substitution**. Four stable Stage3 runtime binaries retain
their recorded SHA256 values. See `source-integrity.csv` and
`stable-stage3-integrity.csv` in `validation/stage04-blockers/`.

No AVX2 requirement, gain policy, clipping, SRC, new interpolation or live
FIR8193 x 16 authorization was introduced. The existing live rejection remains.
MMCSS registration is in the offline benchmark driver, outside measured audio
callbacks; this work does not add a production sink scheduling policy.

## Gates and complete regression run

| Check | Result | Evidence |
|---|---|---|
| Configure/build, MSVC x64 + static Intel IPP | PASS | final/build.txt, build-decoder-final.txt |
| Complete configured CTest suite | **21/21 PASS**, 104.22 s | final/ctest-final.txt |
| AudioCore `--probe` | PASS | final/probe-final.txt |
| Stage04 numeric comparison | **96/96 PASS** | final/numerical.csv |
| Startup/stop/error injection/recovery and FIFO contracts | PASS | final/contracts.txt |
| Clock sign, validity and accuracy | **8/8 PASS** | green/clock-domains.csv |
| ETW interval/control tests, including Python `-O` guards | **5/5 PASS** | etw/correlation-tests.txt |
| MMCSS registration and dispatch capture | PASS | final/scheduling.json, etw/mmcss/scheduling.json |
| Wall-clock realtime gate | **FAIL** | all four benchmark runs below |
| Physical DAC authorization | **BLOCKED** | no offline-pass token created |

CTest includes all previously configured Stage01/02/03/04 tests plus the explicit
clock gate and scheduler positive control. The hardware-gate CTest verifies refusal
without authorization; it does not render hardware audio. The runner's aggregate
exit code is **1**, retained as a failure, even though CTest itself exits 0.

The 96 stereo egress comparisons cover MUSIC/CINEMA, packed24/24-in-32,
128/256/512/1024 frames and the existing signal fixtures against the pinned original
module oracle. Maximum absolute error, RMS error, phase difference, latency
difference, FP64 bit mismatches, PCM byte mismatches and nonfinite counts are all
**zero**. Amplitudes equal their references; active sinus output is approximately
0.0009994322, not silence. These are the existing first-DAC offline fixtures,
not a new claim of physical-driver or formal V07 validation.

## Clock gate: reproduced failure, then capacity-only fix

Before the production edit, the strengthened diagnostic exited **1** with
**8/8 FAIL**, retaining the failing CSV (`red/clock-domains.csv`). With 8192
observations, every clock has valid estimates after 4.01 seconds, correct drift
sign, zero missing estimates thereafter and worst error
**0.027500245869305218 ppm**, below the **0.05 ppm** criterion established before
the capacity fix. No tolerance was relaxed. Observation durations range from
13.3313335 to 106.6720005 virtual seconds.

| Frames | Final estimate -100 ppm | Final estimate +100 ppm | Worst error ppm, either sign |
|---:|---:|---:|---:|
| 128 | -99.99694405 | +99.98911979 | 0.02750025 |
| 256 | -99.99693851 | +100.01000100 | 0.02749401 |
| 512 | -99.99694403 | +99.98898923 | 0.02748151 |
| 1024 | -99.99692185 | +100.01000100 | 0.02745656 |

The approximately 4.01-second warmup is explicit in the test; no empty/invalid
estimate can satisfy this gate.

## Benchmark runs: acceptance unchanged

Each run contains 80 configurations: five measured paths (Complete, AudioGraph,
FIFO, ClockSync, PCM24), two programs, two PCM formats and four block sizes.
Each configuration has 2,000 warmups and 10,000 measured callbacks: **800,000
measured callbacks per run**, **3.2 million across four runs**. Deadlines remain
frames / 192000: 666.667, 1333.333, 2666.667 and 5333.333 microseconds.

| Run | PASS configs | FAIL configs | Wall deadline misses |
|---|---:|---:|---:|
| Normal priority, no kernel ETW | 79 | 1 | 2 |
| MMCSS Pro Audio / critical, no kernel ETW | 79 | 1 | 7 |
| Normal priority, kernel ETW | 78 | 2 | 3 |
| MMCSS Pro Audio / critical, kernel ETW | 79 | 1 | 1 |

Each full 80-row CSV retains median/p99/maximum and FAIL rows. They are under
`normal/`, `final/`, `etw/normal/` and `etw/mmcss/` in the evidence directory.
An observed lower miss count in a separate run does not prove MMCSS causality or
worst-case suitability. ETW instrumentation and ordinary machine load also affect
observations. The acceptance criterion is still **zero wall deadline misses**.

### Execution interval versus descheduling and interrupts

`EnableThreadProfiling` supplies cycle counts and dispatch counts; the positive
control observes a deliberately sleeping test thread, outside audio processing.
MMCSS registration, critical priority and 1,600,002 successful snapshots are
recorded for each run. Cycle counts are reported as cycles, never converted to
microseconds; Windows CPU-time ticks are too coarse for individual short callbacks.
The untraced MMCSS run's seven misses all observed a context switch in the
instrumentation bracket. That bracket is wider than the callback, so those
counters alone cannot establish exact callback descheduling durations.

Kernel ETW originally failed with Access denied. Following explicit user approval,
the helper ran via UAC, captured CSwitch + DPC/ISR events and stopped only its own
logger. The final inventory says **No Active Loggers**. The trace contains
**4,280,511 CSwitch events** and **1,078,210 interrupt events**, with **zero lost
events/buffers, zero state mismatches and zero invalid interrupt intervals**.
Raw QPC clock frequency is 10 MHz. The parser clips each measured callback to its
thread's per-CPU running intervals, supports migration, unions nested interrupts,
then reports separately:

- wall interval;
- scheduled interval minus recorded DPC/ISR (execution proxy);
- time descheduled;
- recorded DPC/ISR time while that thread was current.

The execution proxy includes on-CPU measurement/bookkeeping and may include
untraced firmware stalls. It is not an isolated DSP instruction timer. Native
xperf `cswitch -thread -exc_dpcisr` cross-check totals differ by 1,107.1 us (normal)
and 1,414.4 us (MMCSS) across approximately 74 seconds of whole-thread runtime,
less than 0.002%. This small discrepancy is retained, not represented as bit-exact
agreement. Callback classification is based on clipped event intervals.

All four misses observed inside the kernel trace are retained:

| Mode / callback | Wall us | On-CPU non-IRQ us | Descheduled us | DPC/ISR us | Outcome |
|---|---:|---:|---:|---:|---|
| Normal, AudioGraph CINEMA packed24 128, i4365 | 1267.3 | 1263.2 | 4.1 | 0.0 | FAIL, on-CPU interval exceeds budget + descheduling |
| Normal, Complete CINEMA 24-in-32 1024, i9819 | 24124.4 | 404.5 | 23719.9 | 0.0 | FAIL, descheduling |
| Normal, Complete CINEMA 24-in-32 1024, i9960 | 10282.8 | 449.0 | 9833.8 | 0.0 | FAIL, descheduling |
| MMCSS, AudioGraph MUSIC 24-in-32 128, i2525 | 755.8 | 58.9 | 0.0 | 696.9 | **FAIL, DPC/ISR** |

The MMCSS miss had **zero context switches**, showing why context-switch counters
alone are insufficient. Its callback execution proxy was below the 666.667-us
budget, while interrupts caused the wall miss. There are zero execution-proxy
misses in that MMCSS trace. Conversely, the normal trace has one excess on-CPU
interval; its 236,700 cycles do not justify attributing all 1263.2 us to DSP math.
It remains a visible unresolved on-CPU stall/cost observation. The previous seven
untraced misses cannot be retroactively given exact CSwitch timestamps; their raw
bracket evidence and the independently traced repetition are both preserved.

### MMCSS traced CPU and wall measurements

Below, values are median / p99 / maximum in microseconds. All 80 configurations,
including both formats and every module, have separate execution, descheduling,
interrupt and wall statistics in `etw/*/etw-callback-cost.csv`. ETW p99 uses nearest
rank (9,900th of 10,000 sorted samples); raw benchmark p99 retains its existing
implementation. Percentile components need not add to wall percentiles.

| Program | Path | Frames | CPU median | CPU p99 | CPU max | Wall median | Wall p99 | Wall max |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| MUSIC | Complete | 128 | 47.1 | 92.6 | 137.2 | 47.1 | 95.8 | 220.8 |
| MUSIC | AudioGraph | 128 | 21.6 | 55.2 | 146.5 | 21.6 | 55.6 | 755.8 |
| MUSIC | FIFO | 128 | 0.5 | 0.6 | 31.4 | 0.5 | 0.6 | 31.4 |
| MUSIC | ClockSync | 128 | 17.8 | 46.5 | 96.9 | 17.8 | 47.9 | 225.2 |
| MUSIC | PCM24 | 128 | 2.2 | 15.8 | 34.4 | 2.2 | 15.8 | 47.6 |
| MUSIC | Complete | 256 | 71.4 | 131.1 | 441.4 | 71.4 | 145.7 | 445.1 |
| MUSIC | Complete | 512 | 137.4 | 237.8 | 1067.7 | 137.4 | 249.6 | 1067.7 |
| MUSIC | Complete | 1024 | 356.9 | 529.4 | 671.6 | 358.7 | 550.9 | 1255.5 |
| CINEMA | Complete | 128 | 46.8 | 91.3 | 147.6 | 46.8 | 92.1 | 298.5 |
| CINEMA | Complete | 256 | 71.5 | 132.6 | 193.8 | 71.5 | 138.5 | 288.8 |
| CINEMA | Complete | 512 | 137.2 | 229.5 | 316.0 | 137.2 | 245.9 | 929.8 |
| CINEMA | Complete | 1024 | 360.6 | 524.2 | 1425.3 | 363.2 | 552.2 | 1930.6 |

## Realtime audit, remaining restrictions and toolchain

All four benchmark runs recorded zero C++ allocation/deallocation, zero imported
heap allocation/free, waits, locks and filesystem calls in the measured callback
scopes. The Windows audit covers 25 loaded named imports; it excludes dynamically
resolved APIs, direct syscalls and subsequently loaded images. Scheduler setup,
positive controls, file output and trace processing are outside those scopes.
No blanket proof of every possible Windows/kernel operation is claimed.

Build: MSVC **19.51.36257.0**, x64 toolset **14.51.36231**, CMake 3.30.5,
Ninja 1.12.1, static Intel IPP **2026.0**. CPU: Intel Core i7-9700F, 8 logical
processors, 16 GB RAM. No system-wide scheduling/driver policy was changed.

AudioCore SHA256:
`31D5E252474F8C6C6B65DB55D48EF86AB55628D80C4ECDD7209EADD949F5C652`.
Validation executable SHA256:
`320512D6A6C77D965D1CDA31C7FB8FDFA8B35DF3C420897EE8CAAF86AE2D30BB`.
Only hash records are included in the repository, not the executables.

The two requested gates are **CLOCK PASS** and **DEADLINE FAIL**.
No hardware authorization token is created. Hardware remains BLOCKED, including
any first-DAC probe; FIR8193 x 16 remains unauthorized for live. No DAC, Stage2,
Qt, interpolation/controller redesign or DSP-math changes were executed.
V07 formal acceptance remains independently BLOCKED.

## Reproduction (offline only)

```powershell
./tools/Run-Stage04.ps1 -BuildFolder build/msvc-stage04-blockers -ResultsFolder local/stage04-blockers/repeat
# Expected exit 1 while any wall deadline misses remain. CTest includes clock gate.
./build/msvc-stage04-blockers/EntropyStage04Validation.exe local/stage04-blockers/clock-repeat --clock-only
# Kernel capture needs explicit UAC approval and an elevated PowerShell:
./tools/Trace-Stage04.ps1 -BuildFolder build/msvc-stage04-blockers -ResultsFolder local/stage04-blockers/etw-repeat
# Analyze saved trace; pass an installed Python executable path if necessary:
./tools/Analyze-Stage04-Trace.ps1 -BuildFolder build/msvc-stage04-blockers -ResultsFolder local/stage04-blockers/etw-repeat -Python python
# Built-in independent whole-thread cross-check, no hardware output:
xperf -i local/stage04-blockers/etw-repeat/scheduler.etl -a cswitch -thread -exc_dpcisr
```

Tracing refuses to replace an existing NT Kernel Logger and stops only its own
successfully started logger. The analysis tool rejects event loss, invalid clock,
invalid interval coverage and failed positive controls, including under Python
optimization. It does not change the wall gate or authorize hardware.

## Timing API references

Cycle counts exclude interrupts in the dispatch profiler, and CPU cycles must not
be converted to elapsed time: [PERFORMANCE_DATA](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-performance_data),
[QueryThreadCycleTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime).
Kernel-event layouts and the independent xperf exclusion mode follow
[CSwitch](https://learn.microsoft.com/en-us/windows/win32/etw/cswitch),
[DPC](https://learn.microsoft.com/en-us/windows/win32/etw/dpc),
[PerfInfo](https://learn.microsoft.com/en-us/windows/win32/etw/perfinfo) and
[xperf CSwitch](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cswitch).

## Raw evidence retained locally

The ETL and all four 800,000-row callback CSVs, decoded interval CSVs and regression
outputs remain under `local/stage04-blockers/` and in the local raw archive recorded
in `validation/stage04-blockers/archive-sha256.csv`. ETL SHA256 is
`6D6FB58F9F172B68DD686C413EC8C346C86899EB422F4157E2DFD6A8451761F8`.
The repository includes compact summaries, every failing callback record,
regression logs and integrity manifests. No Intel SDK, binary, library or build
folder is included. SHA256 of the compact evidence is in `evidence-sha256.csv`.
