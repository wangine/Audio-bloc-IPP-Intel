# EntropyAudioCore — Intel IPP / FP64

## MUSIC / CINEMA architecture branch

`codex/music-cinema-architecture` publishes Stage3 **0.1.9-v2**, derived from
the selected **0.1.8** baseline, and the independent MUSIC source snapshots:

- [Stage1 native MUSIC source v0002](Integration/Music/Stage1): native Core, separate Qt UI, L/R FP64 at 192 kHz.
- [Stage2 MUSIC DSP v0009](Integration/Music/Stage2): existing DLL processing, stereo input and L/R/C output.
- Stage3 at this repository root: independent passive MUSIC3 / CINEMA6 or CINEMA8 inputs, selector, original Crossover and output matrix, DAC A/B.

See [architecture, source provenance and next steps](docs/architecture/MUSIC-CINEMA.md),
[Stage3 operation](docs/MUSIC-CINEMA-v2.md) and
[accepted validation](docs/VALIDATION-MUSIC-CINEMA-v2.json).
The Cinema decoder with its own SRC and the third independent Stage1 driver
are future work. The target for that driver is WASAPI Exclusive, Event-Driven,
complete PCM delivery, correct clock synchronization and zero lost frames.

The complete source snapshots include the existing DLL/runtime dependencies.
[Original source and runtime ZIPs](release/music-cinema) for all three Stages
are included, with [SHA256 provenance](docs/architecture/delivery-artifacts.json).
The imported runtimes come from the immutable delivery archives; active listening
folders are untouched. `Integration/Stage1` and `Integration/Stage2` below are
historical snapshots; the active MUSIC snapshots are under `Integration/Music`.

## Historical release notes

Native Stage3 C++20 **integration candidate** on a separate private repository.

Local **0.1.0** source-isolation candidate requires the separately versioned
**Stage2 v0007-source-isolation**. Core lifecycle tests on the real publisher pass,
including forced termination without changing the source UUID. See
[version pairing and launch](docs/RELEASE-0.1.0.md) and
[verification evidence](docs/STAGE010-SOURCE-ISOLATION.md).
The preserved stable reference below is unchanged; this candidate is not pushed.

## Current release: 0.0.3 stable beta

Native AudioCore, IPC-only existing Qt UI, profile/device control and manual
DAC A testing are now integrated. See [startup and version folders](docs/RELEASE-0.0.3.md)
and the [actual live test report](validation/stage07/AUTHORIZED-LIVE-TEST.md).
0.0.3 packages the unchanged 0.0.2 Core/Qt binaries and the local launcher fix.
The user reports two DACs working without apparent audible artifacts. Each
release has its own folder and version-specific runtime/launchers; no overwrite.
No suites/benchmarks rerun; see the release notes for the scope of verification.
Historical 0.0.1: **PASS NUMERIC / FAIL REALTIME; V07 acceptance BLOCKED.**
Cold-Start starts a new Core DISARMED; Live-Test selects explicit process-scoped
manual hardware testing without declaring the offline gate PASS. The GitHub
prerelease contains the Windows runtime ZIP; this Git tree contains sources,
tests, reproduction scripts and reports, excluding SDK/build/private files.

The sections below describe the original Stage01/02 baseline and retain its
historical findings. Current Stage07 behavior and qualification are in the beta
documentation linked above.

**Stage02 = PASS NUMERIC / FAIL REALTIME. Formal V07 acceptance remains BLOCKED.**
The Windows MSVC x64 / Intel IPP build and offline validation have been executed.
This is not an approved production runtime; physical output remains disabled.

See the [complete Stage02 report](docs/VALIDATION-STAGE02.md),
[all 224 CPU benchmark rows](docs/BENCHMARK-STAGE02.md), and
[published numerical results and evidence](validation/stage02/README.md).
The original 10 tests pass; Stage02 CTest is 12/13 because strict Matrix signed-zero
identity fails. Large callback resets and observed deadline overruns remain unresolved.

Reproduce Stage02 offline with `powershell -ExecutionPolicy Bypass -File .\tools\Run-Stage02.ps1`.
The script intentionally returns a nonzero exit for FAIL/BLOCKED checks after collecting
the remaining evidence. Intel libraries, binaries, build folders and the complete local
raw-result archive are excluded from Git.

## Run with Codex on your Windows PC

1. Clone this repository to an empty work directory.
2. Open it in Codex and read AGENTS.md.
3. Run: powershell -ExecutionPolicy Bypass -File .\tools\Build-Windows.ps1
4. Report configure, compiler, linker and CTest results. STOP before any live audio or UI changes.

## Current offline build
- MSVC x64, C++20, static Intel IPP, FP64/192, no JUCE/Qt.
- AudioGraph: Stage2-style 13 logical inputs -> Crossover native IPP -> OutputMatrix -> Headroom -> 16 logical outputs.
- PCM24 native library compiled and independently tested, but not connected to a physical sink.
- Egress ClockSync, Guardian, WASAPI Sink and DeviceManager sources are staged and not yet compiled.
- IPC/Protocol.h defines future command types; no running IPC server/Qt/Stage2 integration.
- Headroom unity policy in AudioGraph.cpp is an **offline-only fixture**, never an approved hardware safety setting.

Target runtime: one DSP Graph thread and per-DAC A/B FIFO/ClockSync/Guardian optional/PCM24/WASAPI threads, with Qt outside AudioCore through versioned IPC.

## Pinned references
- Crossover JUCE-free IPP: wangine/RenderEngine-Stage-3 commit 99b84d13a6808486690e24feba35128d7675be95.
- Crossover JUCE V07 golden: commit 2d5b35165de2ac6be5afa9420851fc24895c6416.
- All other native engine/ABI source: main commit 6cbac37233c8094e39828de8309a9fe13d724ad4.

See docs/ARCHITECTURE.md and docs/SOURCE-PROVENANCE.md. Never overwrite the Stage3 stable runtime. Known issues (Crossover large delay ring reset on Emergency Mute, Bass PCA/PCM overrange) are not hidden or changed here.
