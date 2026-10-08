# EntropyAudioCore — Intel IPP / FP64

Native Stage3 C++20 **integration candidate** on a separate private repository.

Status: source and build configuration published. **First Windows MSVC/IPP compile is pending.** This is NOT an approved production runtime and cannot start DACs in Phase 01.

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
