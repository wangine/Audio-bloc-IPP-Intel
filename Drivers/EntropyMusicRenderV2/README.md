# Entropy Music Render V2

Independent stereo virtual WaveRT/PortCls render driver for MUSIC, with raw PCM
transport to the frozen Stage1 Provider V2 ABI. This is implementation nr3; `V2`
names the ingress ABI. Existing Transport/Virtual drivers are separate products.

Status: uninstalled kernel candidate and offline model. No live mode or PCM
identity acceptance is claimed yet. Raw bridge, production/audit Provider DLLs
and WASAPI verifier compile and pass offline tests. Candidate0.3.0.1 packages an
unsigned catalog; deployment needs the explicit approval described in
`DEPLOYMENT.md`. AUX diagnosis remains deferred.

## Build (no deployment)

1. `tools/Import-Reference.ps1` imports/verifies the exact Microsoft commit.
2. `tools/Prepare-LocalSdk.ps1` extracts pinned Microsoft SDK/WDK28000.2526
   NuGet packages into `build/kits/sdk`, checking SHA256. It runs no installer.
3. `tools/Build-Kernel.ps1 -ReferenceOnly` builds the unmodified sample.
4. `tools/Build-Kernel.ps1` builds the MUSIC-only candidate.
5. `tools/Build-Model.ps1`; `ctest --test-dir build/model --output-on-failure`.
6. `tools/Package-Driver.ps1 -Output <new-workspace-directory>` creates SYS/INF/CAT,
   providers, audit tool, source archive and SHA256 inventory; no deployment.

`tools/Run-Live-Acceptance.ps1` defaults to no action. Its explicit live switch
is for after authorized deployment. It uses separate config/publish/control
endpoints and a hash-identical copy of frozen Core; raw capture comes from that
Core's sole Provider reader. All live acceptance is currently `NOT_RUN`.

The installed MSVC x64 compiler is required. SDK/WDK binaries are build
dependencies, not part of the driver source import. Their provenance is recorded
in `build/kits/sdk-provenance.json`. These commands do not install/activate a
driver, create trust certificates, alter boot policy or touch active Stage1/2/3.

## Endpoint and data ownership

One new `ROOT\EntropyMusicRenderV2` render device; one system stream, stereo.
Only PCM16/16 and PCM24-valid/32, at44100/48000/88200/96000/176400/192000Hz.
PCM16 WAVEFORMATEX metadata is represented canonically as extensible PCM/mask3.
PCM24/32 requires extensible validBits24. Payloads are copied byte-for-byte,
including unused container bytes. No tone, capture, offload, file-save transport,
mute/volume topology node, DSP, conversion or SRC is included in the PCM path.

Packet commits are snapshotted before RUN and delivered only after timed
transfer. The clock retains integer QPC fractions across PAUSE. STOP ends the
generation; readers holding immutable bytes must finish before storage is reused.
The private payload ring holds262144 frames and4096 descriptors. Overflow drops
new valid packets and records loss; it never overwrites an outstanding read.

Notification allocations support one or two packets per DMA buffer, frame-aligned
and no more than65536 frames per packet. Position allocations have no notification
DDIs and are bounded to65536 DMA frames. Position-driven snapshots occur before
exposing a reusable DMA offset; an unseen full lap is refused. **Polling remains
unqualified until the actual live path is audited.** WASAPI servicing flags are
not a substitute for the observed WaveRT allocation/commit mode.

Realtime operations acquire a nonblocking owner gate once; a competing query can
return the previous coherent position/QPC cache. PCM copies do not hold a spin
lock. A timer that cannot acquire ownership tries again on its next wake; QPC,
not wake count, determines transferred packets. Passive teardown/read completion
may wait for the owner/read lease. No waiting/heap/logging is added to the timer.

## Provenance

`upstream/` is the byte-identical MS-PL reference at
`2dc3fd3a0cc84a2933f2194e7ec0871584979071`, with47 Git blob hashes in
`upstream-manifest.json`. `kernel/shell/` is an adapted subset; original notices
are retained. `model/` and `kernel/WaveRtTimeline.*` compile unchanged into kernel
and offline tests. See `docs/architecture/DRIVER-V2-HANDOFF.md` and the approved
implementation plan for acceptance and the later installation approval boundary.
