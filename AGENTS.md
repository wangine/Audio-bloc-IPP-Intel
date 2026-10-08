# Codex instructions: EntropyAudioCore MSVC/IPP baseline

## First action: build and report, do not modify DSP algorithms

1. Check exact branch, commit SHA and clean git status.
2. Detect installed Visual Studio MSVC x64, CMake, Ninja, Intel oneAPI IPP 2026.0 static libraries.
3. Run tools/Build-Windows.ps1 and capture configure/build/CTest/--probe output.
4. If the compile fails, fix ONLY new AudioGraph, BufferPool, CMake or build-script integration in a separate work branch. Keep all pinned module DSP/Egress source files untouched. Do not skip, silence or replace failing tests.
5. Report test totals, compiler version, IPP path, produced executable hash, any errors and unresolved limitations. STOP and do not push/merge without user approval.
6. Never run actual physical output or connect Stage2 during this first phase.

## Immutable boundaries

- CrossoverIPP is pinned to upstream commit 99b84d13a6808486690e24feba35128d7675be95.
- Golden Crossover V07 is 2d5b35165de2ac6be5afa9420851fc24895c6416.
- Other native engines are pinned to 6cbac37233c8094e39828de8309a9fe13d724ad4.
- Do not touch stable Stage3 repository, tags, working folders, binaries, profile or DAC assignments.
- Native C++20 / FP64 / fixed 192 kHz / 13 inputs / 16 outputs / one Graph thread.
- Final architecture: Crossover -> OutputMatrix -> Headroom -> FIFO per DAC -> ClockSync -> optional Guardian exact bypass -> PCM24 -> WASAPI sink.
- Qt may communicate via versioned IPC only, no JUCE/Qt DSP dependencies, no module DLL boundaries within the final audio process.
- No hidden gain, clipping, limiter, normalization, SRC, additional latency or acoustical compensation.
- Realtime processing must have no heap allocation, blocking locks, Windows device scanning, filesystem, synchronous telemetry, logging or hidden kernel warmup.
- Do not compile the whole binary unconditionally with /arch:AVX2 (Xeon E5-1620 v2 does not support it). Intel IPP does CPU dispatch.
- The Headroom unity setting in AudioGraph.cpp is a synthetic OFFLINE fixture ONLY. Physical ARM requires valid real graph headroom policy.
- Existing Crossover bulk reset spike and Bass PCA output overrange are unresolved; never hide them in other modules.

## Next stage (requires explicit user instruction)
Numerical differential graph tests, resolving native ClockSync/SRC dependencies, dual DAC FIFO and event-driven WASAPI, safety/PCM24, profile control IPC and Qt telemetry. No physical playback until end-to-end validation.
