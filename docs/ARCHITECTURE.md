# EntropyAudioCore architecture

Target:

Qt GUI  <->  versioned IPC commands / telemetry  <->  AudioCore control thread
                                       |
                                Stage2 FP64 / 192
                                       |
                           one DSP AudioGraph thread
                     CrossoverIPP -> OutputMatrix -> Headroom
                                       |
                             output 16-way fanout
                                 /           \
                             FIFO A          FIFO B
                               |               |
                        ClockSync A        ClockSync B
                               |               |
                        Guardian bypass    Guardian bypass
                               |               |
                            PCM24 A           PCM24 B
                               |               |
                      WASAPI DAC A         WASAPI DAC B

Separate clock domains: each DAC has its own FIFO, ClockSync state and event-driven WASAPI sink thread. Only the DSP Graph thread runs Crossover/Matrix/Headroom. DeviceManager owns stable DAC identities; no DAC enumerations in the DSP process callback. Guardian bypass must preserve exact FP64 samples with zero added delay or allocations.

## Phase 01 scope: native offline build only

Currently compiled source: DSP/Crossover, DSP/OutputMatrix, DSP/Headroom and Egress/PCM24. Native AudioGraph processes 13 -> 16 planar FP64 channels at exactly 192000 Hz with preallocated scratch buffers. The offline identity OutputMatrix and explicit Headroom unity-mode fixture are not production routing or safety policies.

The PCM24 engine is built and tested separately, not written to hardware. The physical Egress/ClockSync, Guardian, WasapiSink and Devices/DeviceManager C++ trees are present as pinned source snapshots but excluded from Phase 01 compilation. ClockSync has a nominal SRC dependency requiring oneAPI-independent third-party code not yet imported; do not stub or substitute that algorithm.

The IPC/Protocol.h contains only data declarations, not a working server. No Stage2 IPC, DAC ARM, GUI linkage, clock scheduling or live physical outputs are implemented.

## Quality and stop gates

The imported Crossover source uses Intel IPP FP64 FIR 128-tap direct head + partitioned tail and is numerically close to V07, not FIR bitwise-identical. First compile is not a numerical or hardware acceptance.

Crossover still includes full delay-ring clears on certain emergency mute changes, risking callback deadline. The existing Bass PCA hot-level signal can exceed PCM24 range if final headroom is inadequate. Do not mask either defect through clipping or unrelated DSP stages.

Only after Windows native build/CTest PASS should Codex proceed to separate correctness, realtime, clocking and hardware stages.
