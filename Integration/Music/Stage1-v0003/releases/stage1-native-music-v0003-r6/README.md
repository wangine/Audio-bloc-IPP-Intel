# Stage1 v0003-r6 — runtime, sources and audit tools

Paired with [Music Render V2 user-mode bundle r3](../../../../Drivers/EntropyMusicRenderV2/0.3.0.11-signed-dev-r3/README.md).
Installed signed kernel .11 remains unchanged. Source commit: `97e2233`.

Runtime ZIP SHA256:
`A0A0A9BF30AD1224D23C97F9ED37AC2FAB3F41BDA5F3DF1C765D1A2C46E904E8`.
Source ZIP SHA256:
`45321569A378E29EC23C7C94EB12C9BB281E54CA4101913BBE3EC0A96B5E4CA8`.
Core SHA256:
`FE3479AE0060A3D90336526C45417CADDBB0A95ECD48FDA87542CE3402334F87`.

Extract the runtime ZIP and run `Start-Stage1-Music.cmd` inside its r6 folder.
ColdStart/Stop control only the launcher-owned Core, guarded by PID, path and
creation time. Qt remains independent. This private delivery preserves local
MUSIC raw V2 and AUX Shared selections; endpoint IDs require reselection on a
different machine. Frozen v0002 and earlier runtime/Provider pairs are preserved.
v0002 cannot load the lifecycle-capable Provider; retain its original bundle.

[The report](../../docs/V0003-R6-LIVE-RESULT.md) documents 14 finite known-pattern
cases: 2,578,800 raw frames with pause/resume and 7,680,000 exact native-192k FP64
frames. Post-tail RUN counters and startup/stop invalid blocks are reported
separately. No original TIDAL PCM equivalence or all-load guarantee is claimed.
Automatic WASAPI Shared/Exclusive detection remains unavailable in V2 metadata;
Qt displays `Raw PCM · WASAPI access unverified`.

Core 6/6, Provider 12/12, terminal 9/9, Qt and guarded launchers pass. The source
archive was rebuilt with all six tests using the actual adjacent Stage2 v0009
interop reference at `../Stage2/release/stage2-music-v0009`. Its production/test
files are identical to this final documentation-corrected archive, as recorded
in [archive closure](../../evidence/r6-final-archive-closure.json). The dependency
is already available under `Integration/Music/Stage2`; it is only a test reference.

Runtime retains IngressProbe/ControlProbe/OutputPublisherFixture and the strict
PCM/FP64 diagnostic runner. Invoke the runner with this manifested Provider bundle,
exact Core/SHA and a fresh evidence directory. The source-continuity helper rejects
truncated ingress/tick traces and binds their coverage to verified PCM and final
publication. Its bounded trace must cover the entire requested case.
