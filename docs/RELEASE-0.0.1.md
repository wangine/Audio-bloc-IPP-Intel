# Entropy AudioCore — 0.0.1 beta test

Windows x64 test release, tag `v0.0.1`, from `codex/stage07-live-egress`.
No merge into main. Core is MSVC/C++20, FP64 at 192 kHz, with static Intel IPP;
UI is Qt 6.11.2 and communicates through versioned local IPC only.

## Contents and startup

The runtime ZIP contains `runtime/EntropyAudioCore.exe`, `runtime/EntropyUI.exe`,
Qt/MinGW runtime dependencies, startup/stop scripts, reports, notices and SHA256
manifest. DSP and native egress engines are compiled into AudioCore; no audio
DLL is loaded by Qt. GitHub source archives and the release branch contain all
tracked C++ engines, CMake, Qt sources, tests and reproduction scripts. Intel
SDK/static library archives, build directories, private profiles, device bindings
and full raw test captures are excluded. Raw evidence stays locally archived.

Extract the ZIP to a writable directory. It does not require a Qt or Intel SDK
installation to run. An existing external Stage2 transport provides ordered
L/R/C plus eight CINEMA FP64 channels; the beta does not install, modify or start
Stage1/Stage2. No endpoint, pin mapping or profile is hardwired.

- `Cold-Start.cmd`: starts Core and Qt. A new Core starts DISARMED and retains
  the formal offline gate, which currently blocks physical ARM.
- `Live-Test.cmd`: starts the explicitly selected manual hardware-test session.
  A new Core still starts DISARMED. Load/create a profile, configure devices and
  pins, then explicitly ARM in the UI. Manual authorization is process-scoped;
  it is not saved in a profile and does not declare offline qualification PASS.
- Either launcher accepts `-Profile "C:\path\project.entropy"`. Without this
  argument, Core loads `Documents\entropy.entropy` or the UI requests Create/Load.
- Closing Qt leaves Core running with its last state. Reopening the same launcher
  reconnects. Loading a profile and editing DAC bindings/configuration DISARM.
- To switch from another Core executable, save the project, close Qt and use
  that version's stop script first. The beta refuses to replace another owner.
- `Stop-AudioCore.cmd`: after closing Qt, sends the explicit Core shutdown.
- `powershell -NoProfile -ExecutionPolicy Bypass -File tools\Start-Beta.ps1
  -SmokeTest`: isolated offline packaged Core/UI test; no actual source or DAC.

## Qualification and limits

**PASS NUMERIC / FAIL REALTIME. Formal V07 acceptance remains BLOCKED.**
Latest full numeric evidence: 96/96 comparisons, zero absolute/RMS/phase/latency
and PCM differences; clock tests 8/8 PASS. The strict Stage04 benchmark retains
three deadline misses across 79/80 passing rows. Tolerances were not relaxed.
FIR8193 x16 is forbidden live. Native faults, starvation/device loss and invalid
PCM fail closed; recovery requires explicit DISARM/ARM. Guardian is exact bypass.
Headroom follows the original module's user-owned mode/gain policy, with no
extra minimum attenuation imposed by the host.

The observed manual DAC A session exceeded five minutes with 67,166,208 accepted
frames, 64,764 nonzero PCM blocks and zero XRUNs/faults/recorded sink deadline
misses. Closing UI did not interrupt output. This finite observation does not
establish indefinite realtime stability or an auditory-quality verdict. DAC B
was not physically tested. Historical failed attempts remain in the report.

Protected DSP/Egress/Devices/Shared/Transport sources and stable Stage3 match
their integrity baselines. See `validation/stage07/AUTHORIZED-LIVE-TEST.md`,
`authorized-live-result.json`, benchmark CSVs and executable SHA256 manifest.

## Rebuild and third-party components

On a development PC install MSVC x64, Intel IPP 2026 and Qt 6.11.2/MinGW 13.1.
From the source checkout run:

```
powershell -ExecutionPolicy Bypass -File tools\Build-Windows.ps1 -BuildFolder build\msvc-stage07 -Stage02 -Stage04 -Stage05
powershell -ExecutionPolicy Bypass -File tools\Build-Stage05-UI.ps1 -BuildFolder build\qt-stage07
powershell -ExecutionPolicy Bypass -File tools\Package-Beta.ps1 -Version 0.0.1
```

Qt is dynamically linked; its runtime DLLs remain separate and replaceable.
Notices and installed Qt/Intel/MinGW license texts are included under `notices`.
Qt source and its third-party notices are available from the versioned Qt source
distribution: https://download.qt.io/archive/qt/6.11/6.11.2/submodules/ .
This repository does not grant a new license to third-party engines/components.
