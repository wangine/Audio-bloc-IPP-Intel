# Stage05 Control Plane and Qt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Run native AudioCore and the existing Qt UI as separate IPC-connected executables, preserving every existing user control and DSP calculation.
**Architecture:** Native control owner and DSP/sink owners communicate through preallocated block-boundary snapshots. A framed Windows named pipe carries commands/replies/telemetry to a presentation-only Qt client.
**Tech Stack:** C++20/MSVC/IPP 2026.0 for Core; Qt 6.11.2/GCC 13.1 for UI; native pipe; UTF-8 structured payloads with round-trip numeric encoding and explicit LE header.
**Spec:** ../specs/2026-10-09-stage05-control-plane-qt-design.md

## Global Constraints

- Baseline 1fb726bc103cbb630ea55ca27647e85d897c637d; branch codex/stage05-control-plane-qt.
- Preserve all protected DSP/Egress algorithms/ABI and stable Stage3 files.
- Preserve the original 24 UI files' visual layout and presentation behavior.
- Physical ARM stays BLOCKED on Stage04 FAIL; never execute physical playback.
- FP64/192 input order L/R/C plus eight CINEMA lanes; no input SRC.
- No callback allocations/deallocations, locks, waits, filesystem, IPP initialization.
- No push/merge; no Intel SDK/build/binaries in Git.
- User approved design and inline execution without additional planning approval stops.

## Review Focus

- Cross-compiler serialization and uint64 revisions above 2^53: codec tests preserve identifiers/revisions exactly.
- Fast gestures and Save barriers: control tests ensure accepted/applied distinction and no edits leaking past save cutoffs.
- Stale sessions/reconnect: transport tests prohibit command replay and automatic ARM.
- Invalid Headroom/device/profile request: candidate validation leaves current state intact; corruption tests verify rollback.
- UI backend contamination: link/source audit and two-process tests reject DLL/audio ownership and confirm core-owned persistence.

### Task 1: Protocol, strict structured data and native pipe

**Files:** IPC/Protocol.h, IPC/Codec.h, IPC/Json.h, IPC/PipeServer.h/.cpp; Tests/Stage05ProtocolTests.cpp; cmake/Stage05.cmake; CMakeLists.txt.
**Interfaces:** Frame encode/decode retain 16-byte LE header and 1MiB maximum; JSON value parser rejects malformed/nonfinite data, supports exact revision strings. PipeServer serves Handler callbacks on non-RT thread and owns same-user local endpoint. Subsequent tasks consume Frame and Json Value APIs.
- [ ] Write protocol/parser tests: fragmented frame, version/size limits, unknown/nonfinite structured values, escaped UTF-8, exact uint64 strings, signed numeric round-trip.
- [ ] Run RED before implementation; retain logs in local/stage05.
- [ ] Implement strict codec/parser and same-user, remote-rejecting framed pipe transport.
- [ ] Build Stage05 test targets with MSVC; run protocol tests GREEN; test independent pipe clients and teardown.
- [ ] Commit task with verification recorded in local/stage05/progress.md.

### Task 2: Native graph control, authoritative state and telemetry

**Files:** Engine/Control/GraphControl.h/.cpp, Engine/Control/CrossoverAccess.h/.cpp, Engine/Control/CoreState.h/.cpp; Engine/AudioGraph.h/.cpp integration access only; Tests/Stage05ControlTests.cpp.
**Interfaces:** GraphControl owns desired revisions and publishes native Crossover command tags plus Matrix/Headroom snapshots; processBoundary/apply and ownerSnapshot provide actual applied revisions. CoreState handles typed actions and core-side validation using Task1 Json. No DSP object appears in wire.
- [ ] Write RED tests for catalog coverage, invalid parameter nonmutation, actual boundary adoption, gain/mute/program transitions, Matrix routing/Headroom policy and state/telemetry consistency.
- [ ] Reuse original Crossover compatibility/control/display code with storage-only adapter binding to the same Graph processor; preserve original mathematical functions and state export/import.
- [ ] Add Engine accessors and bounded block-boundary command/state exchange; prepare FIR/configuration work outside callback, keep pending/adoption semantics.
- [ ] Populate module telemetry, meter data, two clock/FIFO domain metadata and exact fault/rejection outcomes.
- [ ] Run controls/differential tests GREEN and existing configured regressions; record protected-source integrity.
- [ ] Commit task and ledger commands/output.

### Task 3: Process runtime, devices and profile persistence

**Files:** Engine/Control/CoreRuntime.h/.cpp, Engine/Control/ProfileStore.h/.cpp, Engine/Control/DeviceService.h/.cpp; main.cpp; Tests/Stage05RuntimeTests.cpp; Tests/Stage05IpcIntegration.py; tools/Run-Stage05.ps1.
**Interfaces:** CoreRuntime::serve(endpoint, offlineOptions) runs native pipe + one graph + two independent offline sink owners. DeviceService exposes immutable inventory/binding/configuration replies. ProfileStore loads/saves EntropyProject v1 with original serialized module states/checksums and private core-owned settings. Default launch listens DISARMED; --probe retains behavior.
- [ ] Write RED two-process tests for command/reply correlation, queue pressure, session loss, close-DISARM, profile barriers/roundtrip/corruption, device binding/configuration and physical ARM rejection.
- [ ] Implement off-RT device inventory/configuration validation and per-owner snapshots, native core session ownership and safe stop/reconfiguration.
- [ ] Implement atomic profile Save/Load preserving legacy template/opaque fields and state checksum, with candidate validation and rollback; never write original Stage3 profiles.
- [ ] Start/stop actual Core process in tests with fake sinks and explicit offline signal fixtures; preserve every FAIL gate and current physical prohibition.
- [ ] Run new integration/control tests and audit under control/telemetry load, then commit with logs.

### Task 4: Existing Qt pages with IPC client

**Files:** UI/Legacy/* copied from original 24-file UI, UI/Contracts/* presentation DTOs, UI/Client/IpcClient.h/.cpp, UI/Client/EngineFacade.h/.cpp, UI/main.cpp, UI/CMakeLists.txt; Tests/Stage05UiTests.cpp; tools/Build-Stage05-UI.ps1.
**Interfaces:** EngineFacade implements original writeEngine*/readEngineTelemetry presentation signatures using Task1 protocol and Task3 Core requests. IpcClient is asynchronous QLocalSocket, caches confirmed metadata/snapshots and request outcomes. Device page reads core inventory/bindings; no ModuleRegistry/ModuleInstance or authoritative QSettings persistence.
- [ ] Write RED Qt integration tests: original object names/tabs/control handlers, two-process round trips and errors, inventory/binding rendering, authoritative Save/Load and offline/reconnect behavior.
- [ ] Copy original presentation files and hashes; replace only backend wiring, remove direct controller/DLL/registry dependency and engine state persistence.
- [ ] Map all original page handlers including bulk/gestures/protection/monitor, Matrix, Headroom timing/policies, device configs and profiles to server actions; retain confirmed/applied feedback.
- [ ] Build MinGW EntropyUI against only Qt Core/Gui/Widgets/Network and DTO/client sources; run offscreen two-process tests and baseline visual comparisons.
- [ ] Run source/link audit for absent DSP/IPP/WASAPI/ClockSync/RenderController/StageBus/audio-buffer ownership and commit verified integration.

### Task 5: Complete verification and report

**Files:** VALIDATION-STAGE05.md, validation/stage05/*, tools/Run-Stage05.ps1, source/UI integrity and SHA256 manifests.
**Interfaces:** Complete Stage01-04 + Stage05 + Qt suite and actual two-executable launch evidence; no hardware.
- [ ] Run every configured CTest, Stage04 numeric/clock checks and benchmark under IPC load, plus complete Qt/control IPC integration.
- [ ] Verify source integrity, native module math identity, original UI baseline/visual behavior, private state files and all executable SHA256.
- [ ] Obtain one fresh whole-branch reviewer; fix critical/important findings with RED/GREEN tests, rerun affected suites.
- [ ] Preserve raw local archive and compact reported measurements; explicitly report Stage04 FAIL REALTIME/BLOCKED HARDWARE and V07 BLOCKED.
- [ ] Commit locally; report PASS/FAIL/BLOCKED, tested executable paths and launch steps; verify clean tree, no push/merge.
