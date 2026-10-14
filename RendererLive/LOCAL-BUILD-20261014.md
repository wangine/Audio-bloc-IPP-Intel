# Stage3 LiveV1 — local Windows build record

Build date: 14 October 2026 (Europe/Bucharest). This records the local test candidate built from `chatgpt/stage3-live-renderer-v1-20261014` at exact source commit `8e053cc2f34ccf5abd7ed8ca364d3bc956d218b7`. This document does not change the candidate's source or binaries.

## Build and offline probe

The isolated worktree was `D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source`. Its tracked files were clean before and after packaging. The command was:

```powershell
& .\RendererLive\Build-LiveV1.ps1 -ExpectedCommit '8e053cc2f34ccf5abd7ed8ca364d3bc956d218b7'
```

Configure and Release/MSVC x64/Intel IPP build of `EntropyAudioCore` succeeded with `BUILD_TESTING=OFF`. The configuration identified `Stage3 physical output: LiveV1, FP64 WDL, prepared PCM, independent WASAPI owners`. The existing offline `--probe` returned exit code 0:

```text
AudioGraph native FP64/192 prepare PASS; 13 inputs -> 16 outputs; no hardware or IPC started
```

The previously verified Qt UI was reused. Qt runtime deployment succeeded and `runtime/platforms/qwindows.dll` is present. The package at `D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1-20261014` contains `Live-Test.cmd` and `Stop-AudioCore.cmd`; `INCOMPLETE.txt` is absent. The package manifest identifies the same source commit and hashes shown below.

There were no configure, compile, link, probe or deployment errors. The build log contains CMake developer warnings about policy CMP0115 in bundled SoXR and three MSVC C4244 warnings for possible `wchar_t` to `char` conversion in existing input/telemetry code. CMake's optional `DETECT_SIMD64_C_FLAGS` check failed without stopping configuration. These warnings were not suppressed or repaired for this packaging task.

## SHA256 identity

| Item | SHA256 |
| --- | --- |
| Packaged `runtime/EntropyAudioCore.exe` | `7C1C659B3BFDD2214E76263861EE7944015CE990DEEE5F1D331D510DB7F351D4` |
| Reused, packaged `runtime/EntropyUI.exe` | `0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D` |
| Saved original profile at build time | `F516153F44B2C968493260518B51F4C05E174D825C2E4D748F68B8C049DDB2C2` |
| Packaged `profile/listening.entropy` | `F516153F44B2C968493260518B51F4C05E174D825C2E4D748F68B8C049DDB2C2` |
| Distribution archive `LiveV1-20261014-GitHub.zip` | `BC01A05B38ACBCE280EF2E037BCCD69713F34B2E360B3557D9116C5FFB12DF5D` |

`LOCAL-PACKAGE-SHA256SUMS-20261014.txt` preserves the package's build-time checksum list (34 entries). All 34 listed files still matched their checksums when this record was prepared on 15 October. Four later runtime log files are outside that build-time list. The executable package, saved profile and runtime logs are not committed to Git.

The distribution archive contains the 34 checksummed build files, including the copied profile, plus `SHA256SUMS.txt` (35 entries total). It excludes the four later runtime logs. The owner explicitly requested public distribution of the complete package with its profile. The archive is a release asset, not a file committed to the source branch.

The local build/probe does not establish an audible result or certify precise alignment between independent DACs. Codex did not start the UI/Core, access a DAC, send ARM or change Stage1/Stage2 during packaging.
