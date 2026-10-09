# Entropy AudioCore 0.0.2 beta test — PCM24 V2 integration

Narrow correction to 0.0.1 for the user's LIVE audio test. Branch:
`codex/pcm24-v2-integration`. Stable reference:
`stage3/ref-solid-lowcpu-headroom22-pcm24v2-20261007`, commit
`fb96c3c7644b467f6585a733af53c3819407d9f6` (read only).

## Correction

- Native PCM24 configuration is CLIP_AND_REPORT, flat TPDF and independent
  production seed. Each DAC's non-zero seed comes from Windows BCryptGenRandom
  during quiescent control-thread preparation, outside Graph/DAC processing.
  Seed-generation failure rejects configuration rather than using a test seed.
- Finite PCM overrange is handled by the existing PCM24 V2 converter at the
  final integer boundary and reported; it no longer faults/disarms the domain.
  The quantization rail-clamp counter is no longer treated as a fatal fault.
  NaN/Inf, native converter errors and invalid produced-byte counts remain fatal.
- Core IPC domain telemetry exposes pcmOverrangeSamples, pcmRailClampSamples,
  pcmClippedBlocks, pcmNonfiniteSamples and pcmFaults, plus dither/seed modes and
  processed frames. Counters retain the original module's cumulative semantics
  within each configuration. No PCM conversion algorithm was changed.

Headroom V2 is identical, including AUTO, ATTACK/RELEASE and manual control.
Crossover, Output Matrix, ClockSync, Guardian, WASAPI and Qt are unchanged.
FP64 architecture is unchanged; no extra limiter, normalization or gain.

## Verification and limits

Only MSVC Release build and executable startup were requested and performed.
Both PASS: Core reached native IPC READY, DISARMED, then shut down normally.
Core SHA256: `4DB05C031ACF317312A2E3F628CF6D4C558EEC1D1D0990EB3DB7FDEFBD656BBD`.
Qt is the unchanged 0.0.1 executable, SHA256:
`2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`.
No test suites, benchmarks or new physical playback were run for this patch.
Historical 0.0.1 reports remain included as historical evidence, not fresh
qualification of this build. TPDF is stochastic, so previous deterministic
PCM byte-identity results do not describe this production configuration.
Formal realtime qualification remains FAIL and V07 acceptance BLOCKED.
FIR8193 x16 remains forbidden live.

## Start the LIVE test

Extract the runtime ZIP into a separate writable directory. Save your project
and close Qt; stop the previous Core using its Stop-AudioCore.cmd. Then launch
this package's **Live-Test.cmd**, optionally with
`-Profile "C:\path\project.entropy"`. A new Core starts DISARMED. Load/select
your profile and DAC configuration, then explicitly ARM. Endpoints, pin maps
and Headroom settings remain user-selected. No source application is changed.

Cold-Start.cmd retains the default formal gate; Live-Test.cmd explicitly selects
the manually authorized test mode. Closing Qt leaves the separate Core running.
Stop-AudioCore.cmd requests Core shutdown after closing Qt. Runtime dependencies,
third-party notices, source archive, reports and SHA256 manifest are included.
Intel SDK/static archives, build folders and private profiles are excluded.

Reproduce the native build with tools/Build-Windows.ps1 -CompileOnly -Stage04
-Stage05 -BuildFolder build/msvc-pcm24-v2; package with tools/Package-Beta.ps1
-Version 0.0.2 -NativeBuild build/msvc-pcm24-v2 -UiBuild build/qt-stage07.
