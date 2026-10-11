# Entropy Music Render V2

Independent stereo virtual WaveRT/PortCls render driver for MUSIC, with raw PCM
transport to the frozen Stage1 Provider V2 ABI. This is implementation nr3; `V2`
names the ingress ABI. Existing Transport/Virtual drivers are separate products.

Status2026-10-11: development-signed kernel0.3.0.10 is installed on the explicitly
approved sole ROOT\\MEDIA\\0003 device. All12 Exclusive Event-Driven format cases
pass complete raw PCM comparison inside the unchanged Stage1v0002 Core:
12,894,000 stereo frames, zero mismatched bytes/lost frames/audit overflow.
All12 Polling cases fail Initialize with0x80070032 (unsupported position-only
allocation); they are not PASS. The1800s raw stress passed345,600,000 frames;
all12 PAUSE/RUN and period2208-frame cases passed raw comparisons. Requested
192-frame periods passed at44.1/48k and were refused below the device minimum at
higher rates. Native192k FP64 tests found inserted interior zero clock blocks in
frozen Stage1, even with AUX disabled. Valid sample values compare exactly, but
output continuity and ambiguous publication shutdown do not pass. Core is unchanged.
The initial unsigned0.3.0.1 package is retained as historical provenance.
No TIDAL listening or full A/B/C acceptance is claimed. AUX remains deferred.
See [loaded results and limitations](../../validation/music-render-v2/LOADED-DRIVER-0.3.0.10.md).

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
is for after authorized deployment and requires `-Package <manifested-package>`.
It verifies package artifacts, installed SYS and frozen Core hashes before any
audio action, and runs the packaged audit executable/DLL. It uses separate config/publish/control
endpoints and a hash-identical copy of frozen Core; raw capture comes from that
Core's sole Provider reader. Every new bundle starts with `acceptance=NOT_RUN`;
external validation reports qualify only its recorded modes/formats/durations.
See `audit/TEST-TOOL.md` for preserving/repeating raw and native192k FP64 tests.

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
and no more than65536 frames per packet, with a minimum packet duration of1ms.
Production rejects zero-notification/position-only allocations: elapsed QPC
does not establish that DMA bytes were committed by the OS. The position model
exists only for offline research. **Polling remains unqualified until the actual
live path proves valid packet commits; an unsupported path fails explicitly.** WASAPI servicing flags are
not a substitute for the observed WaveRT allocation/commit mode.

The sole OS commit producer uses a separate fixed inbox, snapshots committed
bytes without taking the publication/read gate, and preserves pre-RUN commits.
Payload pressure retains chronological loss markers; exhausted descriptor
capacity becomes a terminal fault with known-frame loss evidence. STOP fences
writers and completes reads before storage reuse. Device-lifetime loss and last
terminated-generation evidence survive stream free/reallocation. `lostFrames`
on published packets is cumulative within the generation.

Publication operations acquire a nonblocking owner gate once; a competing query can
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
