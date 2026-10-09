# Entropy AudioCore 0.0.3 — stable beta

User-selected stable beta checkpoint after successful user listening on two
DACs, with no apparent artifacts reported. This is a user audio observation,
not a new controlled dual-DAC measurement or a formal realtime qualification.
GitHub publication remains a prerelease. No merge into main.

Audio binaries are exactly the 0.0.2 PCM24 V2 build:

- Core SHA256: `4DB05C031ACF317312A2E3F628CF6D4C558EEC1D1D0990EB3DB7FDEFBD656BBD`.
- Qt SHA256: `2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`.

CLIP_AND_REPORT, flat TPDF and independent production seeds remain restored.
Finite PCM overrange and quantization clamp do not disarm; native NaN/Inf
protection and cumulative PCM counters remain. Headroom V2 AUTO/ATTACK/RELEASE/
manual and all DSP mathematics, FP64 architecture, original egress and Qt are
unchanged. No additional suites, benchmarks or hardware executions are required
for this packaging checkpoint. The existing live session is left running.
Historical formal Stage04 status remains FAIL REALTIME; V07 BLOCKED. Historical
0.0.1 numeric evidence remains historical; production TPDF is stochastic.
FIR8193 x16 remains forbidden live.

## One folder per version

The local release directory contains independent version folders:

```
release/
  0.0.1/EntropyAudioCore-0.0.1-beta-win-x64/
  0.0.2/EntropyAudioCore-0.0.2-beta-win-x64/
  0.0.3/EntropyAudioCore-0.0.3-beta-win-x64/
```

Each folder contains its own runtime/Core, Qt, dependencies, Cold-Start.cmd,
Live-Test.cmd, Stop-AudioCore.cmd, tools, documentation and manifest. Each GitHub
release has its own source/runtime ZIPs and SHA256SUMS. Never extract a different
version on top of another version. The packaging script refuses an existing
version directory; a later build must use the next version number. Generated
release directories and private .entropy profiles are excluded from Git.
Original raw result archives remain in local/, outside the publication assets.

Launch **Live-Test.cmd inside the chosen version's folder** for the expressly
authorized listening session. New Core starts DISARMED; load/select your profile
and DAC configuration, then explicitly ARM. No device or profile is hardwired.
Cold-Start.cmd retains the default formal gate. Closing Qt leaves Core running.
The launcher prints its exact version, source commit and Core executable path.
It refuses to reconnect to a Core from another folder, preventing version mixing.

Before changing versions, save your project, close Qt and use the old version's
Stop-AudioCore.cmd. Launch the new version afterward. Do not stop Stage1/Stage2.
The current running Core is not restarted merely to publish this checkpoint.

The shared repository developer launcher now defaults to the corrected
build/msvc-pcm24-v2 executable, fixing the old Stage07 executable selection.
For reproducible version selection, use the selected release folder's launcher.

## Reproduce packaging

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\Package-Beta.ps1 -Version 0.0.3 -Channel "stable beta" -NativeBuild build/msvc-pcm24-v2 -UiBuild build/qt-stage07
```

SDK/build/static archive files and private profiles are not uploaded. Qt/MinGW
runtime dependencies and third-party license notices remain in the runtime ZIP.
The manifest identifies the committed source and includes payload SHA256 hashes.
