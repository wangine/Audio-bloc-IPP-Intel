# Stage3 LiveV1.1 — local Windows build record

Build date: 15 October 2026 (Europe/Bucharest). The owner selected this revision for publication. The locally built source was `chatgpt/stage3-live-renderer-v1-20261014` at exact commit `9ed04a1b0c71e061e41f32a1ec0d7be90724e3cd`. This record adds no source or binary changes.

## Build and offline probe

The existing `D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source` worktree had clean tracked files. Its prior HEAD `07bf7582f0ba3ad75ea36fec4d8ccb3a55269d67` was advanced only by fast-forward after `FETCH_HEAD` matched the requested SHA. The build command was:

```powershell
& .\RendererLive\Build-LiveV1.ps1 -ExpectedCommit '9ed04a1b0c71e061e41f32a1ec0d7be90724e3cd'
```

The incremental Release/MSVC x64/Intel IPP Core build succeeded in 9/9 steps with `BUILD_TESTING=OFF`. The existing offline `--probe` returned exit code 0:

```text
AudioGraph native FP64/192 prepare PASS; 13 inputs -> 16 outputs; no hardware or IPC started
```

The previously verified Qt UI was reused, Qt runtime deployment succeeded, and `runtime/platforms/qwindows.dll` is present. The new package is `D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1.1-20261015`; `INCOMPLETE.txt` is absent. The original `LiveV1-20261014` package remains present. No configure, compile, link, probe or deployment errors occurred. The build log has CMake developer warnings for policy CMP0115 in bundled SoXR and one MSVC C4244 warning for a possible `wchar_t` to `char` conversion. No warning was suppressed or repaired.

## SHA256 identity

| Item | SHA256 |
| --- | --- |
| Packaged `runtime/EntropyAudioCore.exe` | `761E7BD69757B5DBF499A22812242B8C62D20D6FA84A9129AFC4A83AB9E7B227` |
| Reused, packaged `runtime/EntropyUI.exe` | `0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D` |
| Saved `LiveV1-20261014/profile/listening.entropy` input | `F516153F44B2C968493260518B51F4C05E174D825C2E4D748F68B8C049DDB2C2` |
| Packaged `profile/listening.entropy` | `F516153F44B2C968493260518B51F4C05E174D825C2E4D748F68B8C049DDB2C2` |
| Distribution archive `LiveV1.1-20261015-GitHub.zip` | `B10B615A163DEF9827DD51177AEB755CB0D93127953ECCD0091011290A582EAA` |

`LOCAL-PACKAGE-SHA256SUMS-20261015.txt` preserves all 34 build-time file checksums. All 34 matched the local package when this record was prepared. The distribution ZIP contains those 34 files plus `SHA256SUMS.txt` (35 entries); each ZIP entry's content hash was verified. It includes the copied profile at the owner's request. Four runtime log files created after packaging are excluded. The binaries and profile are distributed as a prerelease asset, not committed to the source branch.

The local build and probe do not establish audible quality, general DAC fault resolution, tonal balance or inter-DAC phase alignment. Codex did not run playback, UI, Core, ARM, a physical DAC or any new capture/test session during packaging or publication.
