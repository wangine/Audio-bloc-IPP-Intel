# Device inventory visibility and scroll — 2026-10-09

Result: **PASS**. This is an IPC Qt presentation fix; physical listening remains **NOT EXECUTED**.

## Reproduction and cause

The running Core returned 26 Windows render endpoints in 0.05 seconds, including two active DIYINHK eight-channel DACs. The Qt inventory contained all 26 rows. Windows discovery order placed the connected DACs at zero-based rows 13 and 16, below the initial viewport. A 250 ms timer rebuilt the table and called `selectRow`, which scrolled back to the selection and cancelled independent browsing. The physical endpoint column also stretched into too little remaining space to display the names.

## Changes

- Present active endpoints first, preserving discovery order within each group. Retain offline records and resolve bindings by endpoint identity.
- Preserve vertical/horizontal browsing positions during table refresh; block intermediate selection notifications while rebuilding.
- Size endpoint names to their contents and provide the full name in the tooltip. Horizontal scrolling remains available for other columns.
- Add `EntropyStage07DeviceInventory` to the reproducible Stage07 Qt suite. It covers a neutral unbound inventory, retained offline records, name width, explicit Refresh, and periodic scroll preservation.

No Core, DSP, transport, profile, ABI or device assignment change was made for this fix. The current user profile was retained in Core PID 4944. A read-only comparison confirmed identical Crossover parameters, Matrix routes and Headroom gain after testing. DACs remained DISARMED.

## Validation

| Check | Result |
|---|---|
| Pre-fix real Core inventory ordering | FAIL: active DACs below initial viewport |
| Pre-fix deterministic scroll test | FAIL: requested 14, reset to 0 |
| Pre-fix real Core name width | FAIL: names truncated |
| Post-fix deterministic inventory test | PASS |
| Post-fix real Core IPC → Qt table, 26 endpoints | PASS |
| Post-fix real Core explicit/periodic scroll | PASS: requested 19, remained 19 |
| Stage07 selected identity/reordering/exact DAC format | PASS |
| Stage07 long preflight and queued IPC timeout | PASS |
| Stage05 original Qt pages, controls, profile cache and executable smoke | PASS |
| Protected source integrity | PASS: 101 protected module files, 28 stable Stage3 files |
| Qt import ownership | PASS: IPC only |
| `git diff --check` | PASS |

Historical failures, before/after captures and detailed execution logs are preserved locally under `local/stage07/`. The native executable is unchanged, so its previously completed 32/32 CTest and offline numeric/clock/benchmark results remain applicable; they were not rerun for this Qt presentation change.

## Reproduce

From the repository root:

```powershell
tools/Build-Stage05-UI.ps1 -BuildFolder build/qt-stage07
$env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
build/qt-stage07/EntropyStage07DeviceInventory.exe
# Optional read-only test against an existing Core with its UI closed:
build/qt-stage07/EntropyStage07DeviceInventory.exe EntropyAudioCore.Control.v1
```

Core SHA256: `2A8B107E92A2D8B8545731BC2D2FC1AAFE16F2D35B27CF0C5B4513EB03DAFCA4`

UI SHA256: `2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`

No push or merge performed.
