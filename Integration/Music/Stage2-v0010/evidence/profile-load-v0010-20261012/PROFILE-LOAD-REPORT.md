# Stage 2 v0010: LOAD and remembered profile

Date: 2026-10-12. Scope: Stage 2 host/profile controls only.

## Behavior

- LOAD beside SAVE accepts existing `.Edsp` profiles and validates them before issuing the existing Center/EQ control requests.
- Successful SAVE/LOAD remembers an absolute path in `state/LastProfile.json`, with a cached copy in `state/Stage2State.json`.
- Startup reads the actual selected external profile, including edits made since its last selection. Missing/invalid selected files fall back to the valid cache with a visible explanation.
- Cancel/invalid LOAD retains the current requested settings and remembered selection. Cached module editor dialogs are destroyed after successful LOAD to avoid stale editor callbacks overwriting the profile.
- New saves include LRC alignment intent; restoration uses the existing APPLY API after valid latency and Center revision acknowledgment. Old profiles without this field retain compatibility and require manual APPLY after a fresh startup.
- No audio-thread file I/O, new DSP algorithm, gain compensation, contract change, or native DLL change.

## Verification

The initial UI regression failed because LOAD was absent. After implementation:

| Check | Result |
| --- | --- |
| DspState | PASS |
| DspCenterState | PASS |
| DspProfileUi | PASS |
| DspEqState | PASS |
| DspProfileRestartTests, packaged v0010 | PASS |
| Packaged `--verify-only` | Exit 0 |

CTest: 4/4, zero failures, 0.68 s. UI tests cover real dialogs, Unicode paths, cancel/invalid input, Center/EQ control requests, editor disposal, remembered startup path and LRC intent. The isolated process test covers selected external profile taking precedence over cache, graceful restart, missing-file fallback and native LRC restoration through existing APPLY. It uses a private synthetic StagePublish endpoint; no real Stage 1 or DACs participate.

During fixture development, holding the diagnostic file open across the polling wait caused Windows to reject QSaveFile replacement. The child exited with diagnostic-write status 3, zero audio faults and zero source drops. The same behavior reproduced with v0009. The test reader now closes before waiting; production transport/DSP was not modified for this test issue.

## Local deployment

Runtime: `D:\Projects\3Stage-3\Stage2\release\stage2-music-v0010`.
Launcher: `ColdStart-DSP-Music.cmd`.
Executable SHA256: `4ED07784371B52A2845A15A11E5D3B275C16806552731176B4F3FF45AA03A50E`.

The user saved `D:\Projects\3Stage-3\Stage2\Profiles\DSP Engine.Edsp` from v0009. It was copied unchanged into the new startup cache and its actual path selected. Original profile/cache SHA256: `DB4BBAD7EB415FEB3A75B3D3FBA55373D609BF85DD07E596F772DBF696EB5DC9`. An original-profile backup is preserved as `DSP Engine-v0009.Edsp` beside this report.

Live v0010 UI observations: LOAD and SAVE present; `Profile: DSP Engine.Edsp`; `CONNECTED / SIGNAL ACTIVE`; EQ `ACTIVE / FP64 / STEREO`; Center `ACTIVE / FP64 / C ONLY`. Initial legacy-profile LRC state was `BYPASS EXACT — APPLY TO ALIGN`; a subsequent live observation showed `ALIGNED`. The original legacy file did not contain LRC intent; SAVE in v0010 persists the current alignment choice for subsequent startup.

Only Stage 2 was replaced. Stage 1 PID 17204 and Stage 3 Core PID 4996 were retained during the switch. Original v0009 package is preserved. A hidden v0010 launch was removed, then v0010 was launched interactively through the app automation API; the temporary second interface was closed before relaunching the sole interactive instance.

Stage 1/3 sources, binaries and configurations were not edited for this feature. No general test suite was run. No Git push of this feature has been performed; the previous verified Git reference remains commit `4016d27`.

## Commands

```powershell
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH='C:\Qt\6.11.2\mingw_64\plugins\platforms'
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir Stage2\build -R '^(DspProfileUi|DspState|DspEqState|DspCenterState)$' --output-on-failure
& '.\Stage2\build\Host\EntropyDspEngine\DspProfileRestartTests.exe' '.\Stage2\release\stage2-music-v0010\EntropyDspEngine.exe'
```

Named-pipe and Qt temporary-file tests ran with normal Windows filesystem/process access. `stage2-load-save.png` is an offscreen UI test artifact, not a live audio measurement.
