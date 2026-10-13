# Stage3 r3 startup and 32k package — 2026-10-13

The local r3 live command failed before it reached AudioCore because its
launcher queried `Win32_Process` through `Get-CimInstance`. On this Windows
session that provider returns “Access to a CIM resource was not available to
the client.” The launcher now checks active process names without CIM. The stop
command identifies the named-pipe server PID and verifies that PID's executable
path matches the selected package before it sends a shutdown request.

## Versioned live commands

Run one Stage3 variant at a time:

```cmd
D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.9-v2-r3\Stage3-r3-32k-Live.cmd
D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.9-v2-r3\Stage3-r3-64k-Live.cmd
```

Each command starts that bundle's Core DISARMED and opens its UI. It does not
auto-arm. Load the validated profile, verify the DAC configuration and arm in
the UI when ready. Stage1 and Stage2 remain separate processes and may be
started independently.

The launcher refuses to start another Stage3 Core or UI while one is already
running. Close the current UI and run the matching bundle's `Stop-AudioCore.cmd`
before switching FIFO variants. `Cold-Start.cmd -SmokeTest` is an offline
packaged UI check; it uses a private endpoint, synthetic input and no DACs.

## Build and validation

The 32k Core was rebuilt from the r3 source tree with
`ENTROPY_LIVE_FIFO_FRAMES=32768`. Its SHA256 is
`941500A1D82830F8599C62F6D55CAD3D5D8D1BC9752B5C969BF84888E872D8CA`.
The native `--probe` passed and the focused `EntropyStage019Egress` test passed
(1/1). Packaged Core/IPC/Qt smoke checks passed for both 32k and 64k with the
Core DISARMED. The package stop command also passed against a separate offline
test Core: it verified the pipe owner and executable path, then shut down only
that test process. No general test suite or physical output was run for this
packaging repair.

The 64k audio binary is unchanged from the previously accepted r3 DTS trial.
The 32k package has not yet been qualified by live listening. These offline
checks verify launch and packaging; they do not establish audio quality for the
32k FIFO configuration.

The reproducible focused build command from the repository root is:

```powershell
& .\tools\Build-MusicCinema.ps1 -FifoFrames 32768 -BaselineOnly -Targets @('EntropyAudioCore','EntropyStage019Egress') -Parallel 2
```

The build wrapper now preserves CMake developer warnings in its log without
mistaking them for a failed native command.
