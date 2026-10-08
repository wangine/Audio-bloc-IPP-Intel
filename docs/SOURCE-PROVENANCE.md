# Original sources and strict provenance

CrossoverIPP: wangine/RenderEngine-Stage-3 branch codex/crossover-ipp-native-fp64, commit 99b84d13a6808486690e24feba35128d7675be95, source at Stage3/native-reference/Modules/CrossoverIPP.
Copied to DSP/Crossover/include, src, tests, notices. The preserved C ABI adapter is DSP/Crossover/compat/src/CrossoverModule.cpp and is not compiled into the native AudioGraph.

Gold reference Crossover V07: wangine/RenderEngine-Stage-3 commit 2d5b35165de2ac6be5afa9420851fc24895c6416. Keep golden DLL and tests external; do not merge JUCE source.

OutputMatrix, Headroom, PCM24, ClockSync, Guardian, WASAPI Sink, DeviceManager, Shared ABI and nominal SRC engine sources: wangine/RenderEngine-Stage-3 main commit 6cbac37233c8094e39828de8309a9fe13d724ad4.

Imported sources were copied as pinned snapshots, not linked by Git submodule. Any future source deviation must be documented as an independent patch.

CMake Phase 01 builds only native Crossover + Matrix + Headroom + PCM24 + offline Graph/Tests. ClockSync/Guardian/WASAPI/DeviceManager source is staged but not built. Dependencies/SRC includes MultiChannelSrcEngine.cpp/h provenance; SoXR and r8brain vendor source are not included and are not to be silently reimplemented.

Intel SDK (IPP headers/static libraries) is NOT redistributed. DSP/Crossover/notices contains source vendor license notices. This repository has no live audio acceptance yet.
