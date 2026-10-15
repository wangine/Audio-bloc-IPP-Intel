# MUSIC ClockFollow C1 — local Codex BUILD ONLY

This isolated branch implements an optional clock-rate actuator in Drivers/EntropyMusicRenderV2. Read CLOCKFOLLOW-C1-CODEX.md and CLOCKFOLLOW-C1.md in that directory. Historical Stage1/2/3, B1, LiveV1 and driver deployment instructions are not current tasks.

Use `D:\Projects\Entropy-ClockFollow-C1-20261015\source` on `chatgpt/music-clockfollow-c1-20261015` at the exact SHA from the current handoff. One fetch and an exact clean fast-forward before building are allowed. Never reset, stash, force, remove user files, repurpose the original checkout or merge this branch into the Stage3 renderer.

Run `Drivers/EntropyMusicRenderV2/tools/Build-C1.ps1 -ExpectedCommit <exact SHA>`. It builds the bounded offline C1 model/packet contracts and the unsigned kernel SYS/INF with the existing approved local MSVC/WDK/SDK. No general CTest, historical qualification, unmodified-reference build, dependency substitution or tool installation.

At the first actionable failure, preserve logs and STOP. Do not modify code, tests, scripts, PCM routing, flags, SDK paths or security settings to obtain PASS. Report the actual result, versions/hashes and tracked status; no audio-quality claim.

No signing, catalog generation/deployment, INF/device registration, SYS installation, service restart, reboot, registry/ACL/boot changes, Administrator, global sandbox disable, certificate import, live audio/DAC/ARM/capture or clock-control IOCTL execution. Earlier driver-deployment approvals do not apply to this candidate. No Stage1/2/3, AUX, CINEMA, WDL/r8brain or profile changes. Scoped normal-user fetch/file/execution permission may be requested for the build only.

C1 is NOT a new live playback build. It leaves the installed driver and all current audio runtimes unchanged. The ClockBridge, Stage1 pacing integration and Stage3 direct-primary mode are later explicit deliveries. Stop at the unsigned compile-candidate report; do not push/merge automatically.
