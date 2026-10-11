# Entropy Music Render V2 Driver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Execution is direct in this chat, as already requested by the user; no subagent dispatch or additional worktree is requested.

**Goal:** Build the independent stereo MUSIC WaveRT driver and raw Provider V2, then verify integral PCM and clock continuity before diagnosing AUX.

**Architecture:** A pinned Microsoft Simple Audio Sample supplies the PortCls/WaveRT shell. A common tested model owns format validation, the QPC timeline and immutable PCM commits; a separate private bridge feeds the existing Stage1 ABI. The driver, Provider and audit harness have their own build outputs; Stage1 v0002 and active Stage2/3 deliveries remain reference artifacts.

**Tech Stack:** Windows x64, MSVC, installed WDK/SDK 10.0.28000.0 subject to successful toolset verification, WaveRT/PortCls, C++20 user-mode tests, CMake/Ninja, WASAPI and the existing Provider V2 C ABI. No Qt or DSP libraries in the driver/Provider.

**Spec:** [DRIVER-V2-HANDOFF.md](../../architecture/DRIVER-V2-HANDOFF.md), approved direction; this implementation plan awaits review. [AUX follow-up](../../architecture/FOLLOW-UP-STAGE1-AUX.md) is outside this implementation.

## Global Constraints

- Microsoft reference commit: `2dc3fd3a0cc84a2933f2194e7ec0871584979071`; sample `audio/simpleaudiosample`; retain MS-PL and individual notices.
- New identity `Entropy Music Render V2`; one eRender stereo endpoint, independent of Transport and Virtual.
- Rates: 44100, 48000, 88200, 96000, 176400, 192000 Hz; PCM16/container16 and PCM24/container32, MSB-aligned; channelMask3, PCM SubFormat.
- No Q31, FP64, SRC, gain, mute, dither, limiter, APO processing, Qt, physical DAC, ASIO, MME transport or Direct KS audio client.
- Provider ABI stays `0x00020000`; format64, packet160, info256, endpoint1376, API336 bytes; max65536 frames per packet.
- Private payload ring262144 source frames, 4096 descriptors; pending snapshots bounded separately by accepted WaveRT allocation; no mandatory prebuffer delay.
- Commit, timed transfer and Provider delivery have distinct counters; pre-RUN committed data is retained; only one publication owner per buffer mode.
- STOP ends a generation; PAUSE/RUN preserves it. PCM bytes never change in driver/Provider; packet.mode is UNKNOWN unless independently verified.
- No install, activation, certificate-store change, boot-policy change or live source switch as a build side effect. Installation is a later explicit authorization step.
- Do not edit Stage1 v0002 Core/ABI, AUX, DSP, existing drivers, active runtime folders, profiles, DAC settings or earlier packages.

## Review Focus

1. A consumer disappears during a pending read: cancellation must finish without deadlock, use-after-free or blocking the driver clock; covered by Task4.
2. A reader starts late or the ring overflows: original frame indexes and loss evidence must survive rather than being relabeled frame0; covered by Task2.
3. WASAPI servicing style differs from WaveRT allocation mode: no fallback activated merely by lack of an observed callback; covered by Tasks3/5.
4. STOP races with a committed prefill or a partial EOS packet: old data must not enter the next generation, and a drained tail must not be dropped; covered by Tasks2/3/5.
5. A foreign device has a similar friendly name: discovery and test rendering must require the new device/interface identity, never a physical DAC; covered by Tasks4/5.

## Planned files and boundaries

All new implementation lives under `Drivers/EntropyMusicRenderV2/` in this repository.

- `upstream/`: exact imported Microsoft files and notices, with `upstream-manifest.json`.
- `model/Format.h`, `model/Clock.h`, `model/PacketTimeline.h`, `model/PcmRing.h`, matching `.cpp`: kernel-compatible fixed-size types and deterministic algorithms; storage supplied externally, no STL heap ownership.
- `protocol/PcmBridgeV1.h`: versioned pointer-free wire metadata, IOCTL IDs, device identity and limits; independent of the user-mode Provider API declarations.
- `kernel/`: adapted sample shell plus `WaveRtTimeline.cpp`, `PcmCommit.cpp`, `BridgeDevice.cpp`; corresponding headers; x64 driver project and INF/INX.
- `provider/RawMusicProvider.cpp`, `BridgeClient.cpp`, `EndpointIdentity.cpp`, headers and `.def`: C ABI export, cancelable private I/O and exact endpoint identity.
- `audit/WasapiClient.cpp`, `PcmPattern.cpp`, `PcmVerifier.cpp`, `StereoMetrics.cpp`, `main.cpp`: controlled virtual rendering, byte comparison and diagnostic reports.
- `tests/ModelTests.cpp`, `BridgeTests.cpp`, `ProviderTests.cpp`, `AuditTests.cpp`, `PackageTests.py`: offline/model verification; kernel live acceptance is recorded separately.
- `tools/Import-Reference.ps1`, `Build-Model.ps1`, `Build-Kernel.ps1`, `Build-Provider.ps1`, `Build-Audit.ps1`, `Package-Driver.ps1`: explicit non-deploying import/build/package commands.
- `CMakeLists.txt`, `README.md`: user-mode builds, target names and reproducible operation.
- `validation/music-render-v2/`: hashes, build logs, test results and deployment/acceptance records; raw audit captures are kept separately from concise reports.

## Task 1: Pin the source and establish a real kernel build

**Files:** create the upstream tree/manifest, import and build scripts, initial kernel project, user-mode CMake project and `tests/PackageTests.py` under the planned root.

**Interfaces:** `Import-Reference.ps1 -Commit <sha> -Destination <path>` verifies every imported Git blob. `Build-Kernel.ps1 -Configuration Release -Platform x64` produces a SYS and build report without installing it. The report states actual compiler/toolset/SDK/WDK and SHA256, not inferred compatibility.

- [ ] Write `test_upstream_manifest_exact_pin` and `test_build_has_no_deployment_side_effects`: reject a different reference SHA, absent license and scripts invoking install/boot/certificate mutation.
- [ ] Run `python Drivers/EntropyMusicRenderV2/tests/PackageTests.py --reference-only`; confirm a missing import/manifest fails.
- [ ] Import only the pinned sample and its required notices/shared build references, keeping a complete per-file manifest; create build scripts that locate the installed kernel toolset and capture its explicit paths.
- [ ] Build the uninstalled x64 reference shell using `Build-Kernel.ps1`; confirm actual SYS output and nonzero size/hash, retain warnings. If WDK integration is unavailable, preserve the report and continue only independent model work; do not install or silently downgrade a toolset.
- [ ] Run the manifest/non-deployment checks again and commit the independently buildable reference baseline. This build is not a MUSIC endpoint acceptance result.

## Task 2: Deterministic format, clock, ownership and ring model

**Files:** create `model/*`, `protocol/PcmBridgeV1.h`, `tests/ModelTests.cpp`, `tests/BridgeTests.cpp` and `tools/Build-Model.ps1`.

**Interfaces:** define `PcmFormat` with the full PCM metadata; `bool validateFormat(const PcmFormat&) noexcept`; `Clock::reset(uint32_t rate,uint64_t frequency,uint64_t generation)`, `start(uint64_t qpc)`, `pause(uint64_t qpc)`, `bool position(uint64_t qpc,uint64_t& frames) const noexcept`. `PacketTimeline::commit(const PacketDescriptor&,const uint8_t*,uint32_t bytes)` returns an explicit accepted/late/duplicate/overrun result. `advance(uint64_t sourceFrame)` marks eligible committed snapshots exactly once; `stop()` terminates the generation. `PcmRing::peek(PacketView&)` returns a stable non-owning view; `consume(uint64_t sequence)` releases only that packet. Memory spans are supplied at prepare and bounded by the spec.

- [ ] Write `FormatRejectsUnsupported` for the exact twelve formats, bad GUID/mask/align/rate/reserved fields, Float32/PCM32/packed24 and overflow lengths.
- [ ] Write `ClockFractionalRatesAndPause`: 44100 frames after one QPC second, no movement during PAUSE, no cumulative rounding across small updates, checked overflow and unchanged results across repeated position queries.
- [ ] Write `PrefillSurvivesRun`: frame0 committed before RUN is retained, not readable early, readable once when transferred; duplicate commit/query never republishes.
- [ ] Write `RingWrapAndLateReader`: verify the last frame across wrap, fixed descriptor/payload limits, explicit overflow loss, nonzero firstSourceFrame for late attachment and preserved terminated-generation/lifetime totals.
- [ ] Write `StopRestartAndPartialEos`: distinguish PAUSE from STOP, reject old-generation data, preserve exact last valid bytes and accept zero-length EOS only as control state. Simulate ULONG packet-number wrap separately from DMA wrap.
- [ ] Run `Build-Model.ps1` and `ctest --test-dir Drivers/EntropyMusicRenderV2/build/model --output-on-failure`; confirm the named assertions fail before implementing the model.
- [ ] Implement the exact integer QPC model, immutable snapshot lifecycle and bounded ring; define wire version `0x00010000`, explicit fixed-width fields and compile-time size/offset checks. Add invalid-version, short/oversized payload and insufficient-read-buffer assertions to `BridgeTests`.
- [ ] Run the complete model/bridge suites; require PASS and commit. Model PASS is not kernel acceptance.

## Task 3: One stereo WaveRT endpoint with the tested timeline

**Files:** adapt `kernel/*`, INF/INX and speaker filter tables; add kernel bindings for the model and focused model adapter fixtures in `tests/ModelTests.cpp`.

**Interfaces:** existing WaveRT `SetState`, `SetWritePacket`, `GetPacketCount`, position and notification DDIs delegate to the tested model. `CaptureCommittedPacket(...)` snapshots only an OS-committed packet. `AdvanceTransfer(qpc)` advances one clock and eligibility; position queries/timers cannot create a second producer. Buffer allocation selects one explicit packet or position-driven owner.

- [ ] Add `KernelBindingsKeepPrefill` and `NoDualProducer`: RUN must not reset valid queued data; multiple position/timer calls must not duplicate raw output.
- [ ] Add `AllocationModeIsNotWasapiFlag`, `PositionDrivenReadOwnership` and `StopDuringPendingCommit`: validate the ownership invariant separately for both buffer paths and lifetime teardown. The latter test forbids freeing/reusing storage while a snapshot/read still references it.
- [ ] Run the focused model adapter tests and confirm failures before bindings exist.
- [ ] Adapt the sample to the new PnP/service/interface identities and one render stereo endpoint; exclude microphone/tone/file-saving/offload facilities from this product. Advertise only the twelve allowed formats and exact supported buffer/period bounds.
- [ ] Connect format/clock/commit/transfer/ring model to the DDIs, preserve pre-RUN snapshots, implement partial EOS and explicit underrun/loss diagnostics. Use nonpaged/pinned memory and short metadata synchronization; no heap/logging/filesystem or waits in the realtime path.
- [ ] Build x64 Release, rerun the model/adapter tests and source/package audits. Preserve actual driver errors/warnings; inspect INF endpoint count/format tables and SYS imports. Commit this uninstalled candidate.
- [ ] Record polling as unqualified until the live harness demonstrates actual ownership. A timer/position model passing offline cannot authorize RAW_IDENTITY for a prematurely read buffer.

## Task 4: Private bridge and raw Provider V2 compatible with frozen Core

**Files:** create `kernel/BridgeDevice.*`, `provider/*`, the DLL definition and `tests/ProviderTests.cpp`; extend `tests/BridgeTests.cpp`.

**Interfaces:** provider exports exactly `entropy_endpoint_provider_v2(uint32_t,EntropyProviderApiV2*)` using the unchanged published ABI. `BridgeClient::open(const DeviceIdentity&)`, `read(EntropyIngressPacketV2&,void*,uint32_t)`, `cancel()`, `close()` wrap private device I/O. `EndpointIdentity::resolve()` binds to the new interface/PnP instance, returning the associated Windows render endpoint identity. Private read is an ownership transfer after complete successful copy; small buffers do not consume a packet.

- [ ] Write `ProviderAbiExactRaw`: require the frozen ABI hash and all five struct sizes, MUSIC-only role, RAW_PCM capability, exact original container bytes, full format/GUID, nonzero sequence/generation and zero reserved fields.
- [ ] Write `ReadTooSmallDoesNotConsume`, `CloseCancelsPendingRead`, `SecondReaderRefused`, `WrongDeviceRejected` and `AccessModeDoesNotInventWasapiMode`; use a fake bridge with the same wire protocol and deliberately similar foreign endpoint names.
- [ ] Run the Provider/bridge suites; confirm the named failures before the corresponding implementation.
- [ ] Implement IOCTL validation, session ownership and cancel/close rundown; implement the Provider with no sample conversion. Reuse the Core's supported private-transport access convention while keeping packet.mode UNKNOWN unless authoritative information exists.
- [ ] Implement test-only observation of the exact bytes returned to Core, with a bounded capture queue and explicit audit overflow counter; no second bridge reader, production file I/O or DSP change.
- [ ] Run complete tests and load the DLL export/ABI in an isolated harness; verify no Qt/DSP dependency and commit. Do not copy into active Stage1 provider directories or restart Core.

## Task 5: Controlled WASAPI/PCM acceptance harness and stereo diagnostics

**Files:** create `audit/*`, `tests/AuditTests.cpp`, `tools/Build-Audit.ps1`, report schema and an acceptance runner under the planned root.

**Interfaces:** `WasapiClient::run(const RunConfig&) -> RunReport` records requested WASAPI mode/servicing separately from observed WaveRT mode; requires the exact new endpoint identity. `PcmPattern::frame(uint64_t frameIndex,const PcmFormat&,uint8_t* output)` emits distinct L/R values. `PcmVerifier::compare(expected,actual) -> Comparison` accounts for every byte and frame. `StereoMetrics::accumulate(rawPacket)` computes windowed pre-SRC statistics in the audit tool only.

- [ ] Write `VerifierRejectsMissingFirstAndLastFrame`, `VerifierRejectsDuplicateAndReorderedFrame`, `VerifierChecksAllContainerBytes` and `IncompleteAuditCannotPass`; confirm failures with truncated/corrupted synthetic fixtures before verifier implementation.
- [ ] Write `StereoKnownMidSideAndPearson`: mono has zero Side, antiphase has zero Mid, DC-offset fixtures distinguish uncentered correlation from Pearson; zero RMS/variance produce explicit undefined values, not division artifacts.
- [ ] Write `HarnessRejectsPhysicalOrForeignEndpoint` and `InterruptedStopIsNotDrainPass`; test discovery and tail accounting offline before opening a real device.
- [ ] Implement Exclusive Event-Driven and Polling client paths, prefill before Start, exact format negotiation, complete transfer/read drain and configurable buffer periods/durations. Capture expected/emitted/raw-received bytes and actual clock/packet evidence. Unsupported cases and audit overflow cannot become PASS.
- [ ] Run offline harness/model/provider tests; commit the harness. No existing virtual endpoint or physical DAC is opened by default.
- [ ] Define a live runner for 12 formats × 2 servicing modes, initially 10 seconds per case, plus 30-minute continuous stress at PCM24/192 kHz with validated servicing. Record exact duration/frame coverage, not unlimited stability. Add per-format STOP/RUN, format-change, EOS/partial-tail and wrap cases; simulator ULONG wrap remains labeled offline.

## Task 6: Reproducible package and explicit deployment boundary

**Files:** implement `tools/Build-Provider.ps1`, `Package-Driver.ps1`, extend `tests/PackageTests.py`, create package README, SHA256 manifest and deployment checklist.

**Interfaces:** `Package-Driver.ps1 -Output <isolated-directory>` packages SYS/INF/CAT and Provider/harness with complete source/import provenance. It reports signature/catalog validity; it does not change certificate stores or install devices.

- [ ] Write `PackageHasOneNewStereoIdentity`, `AllArtifactsMatchManifest`, `ExistingDriverIdsAbsent` and `NoInstallFromBuild`; confirm failures on incomplete or deliberately altered package fixtures.
- [ ] Implement packaging/signature verification and rollback documentation identifying only the new device/service; do not embed private signing keys or silently change boot security.
- [ ] Build/check all components, run package tests and verify every artifact SHA256; record actual compiler and signing status. Commit the reviewable package/source state.
- [ ] Present the concrete package, hashes, signature status and exact deployment action to the user; obtain explicit installation authorization at this boundary, as required by the approved handoff. Until then, continue only independent offline verification.

## Task 7: Loaded-driver qualification and frozen-Core integration

**Files:** produce validation reports, accepted artifact manifests and README instructions; no Stage1 v0002 source edits.

**Interfaces:** use Task5's runner and Task4's exact Provider in an isolated copy of the original Stage1 runtime. Bind separate test configuration/publish/control endpoints. Provider observation is the payload delivered to this unchanged Core. Never use a second reader or treat legacy converted FP64 as raw verification.

- [ ] After authorized deployment, verify exactly one new render endpoint and correct identity/format negotiation; require valid startup/frame0 before calling any mode qualified.
- [ ] Execute acceptance A: all24 Exclusive cases, byte-for-byte from first to last frame, with the actual Core/Provider boundary observed; retain per-case mismatch/loss/duplicate and audit-overflow totals.
- [ ] Execute acceptance B: live continuity/clock, repeated lifecycle/format transitions, supported period bounds and long stress; record nominal versus committed versus delivered counters, actual WaveRT allocation mode, EOS and packet chronology. Keep injected overload separate from zero-loss results.
- [ ] If A/B fail, fix and rerun only affected plus relevant regression tests; never discard startup/tail data or relabel a failing polling path as supported. Model-only tests cannot substitute for these runs.
- [ ] Execute acceptance C only after A/B qualify: TIDAL Shared/Exclusive stereo diagnostics before SRC, format/mode provenance, common-frame RMS/M/S/correlation windows and transition intervals. Do not assert identical TIDAL internal PCM between modes.
- [ ] Record actual unchanged Core/Stage2/Stage3 hashes and observed listening state. Publish the qualified driver/Provider packages and explicit limitations; then begin the separate AUX investigation with a new versioned Stage1 candidate.

## Self-review and execution handoff

Tasks1–3 cover pin/build, strict formats, QPC and ownership; Tasks2–4 cover raw ring,
metadata, counters, cancellation and no conversion; Task5 and Task7 implement the
three acceptance groups; Task6 owns the deployment boundary. Each Review Focus
item has a named test. No AUX repair, new Stage1 DSP or current-driver modification
is hidden in these tasks.

Plan status2026-10-11: approved and executing inline. Tasks1–6 are implemented.
The user approved signing with the existing DEV certificate and installation of
only the new root device plus isolated tests. Task7 is in progress: installed
kernel0.3.0.10 passed all12 Exclusive Event-Driven raw cases (12,894,000 frames);
all12 Polling cases are explicitly refused at Initialize0x80070032. Continuous
1800s raw stress passed345,600,000 frames, all12 PAUSE/RUN and requested2208-frame
period cases passed raw comparison. Smaller192-frame requests passed at44.1/48k;
the eight higher-rate cases were refused below device minimum. Native192k output
tests reject inserted interior zero blocks despite exact valid FP64 values.
The final audit also rejects frozen publication's ambiguous terminal0. Core is
unchanged; full A/B/C, native OS EOS and real TIDAL remain incomplete.
See validation/music-render-v2/LOADED-DRIVER-0.3.0.10.md and the complete recorded
evidence. No production or unbounded stability claim is made.

Follow-up authorized inline2026-10-11: Stage1v0003-r3 is a separate source/runtime,
not a replacement of the frozen reference. The new Core fixes ingress double wait,
MMCSS scheduling, declared FIFO priming, AUX queue drain, durable publisher terminal,
delayed control receipt and telemetry contention. Kernel0.3.0.11 wakes passive
read-finish on gate release instead of coarse1ms retry. Loaded12-format raw matrix
and native192k 60s/format pass, with exact binary64 and full drain. Real legacy
MUSIC + default AUX Shared300s passes Core counters/metadata; Exclusive AUX/MMCSS120s
passes ingress chronology. Driver12CTest, Core6CTest, terminal9cases and standalone
source closure pass. Current report: validation/music-render-v2/LOADED-DRIVER-0.3.0.11.md.
The original frozen-Core output FAIL and untested TIDAL/effective native EOS paths
are not relabeled complete; older long-stress coverage stays bound to0.3.0.10.
