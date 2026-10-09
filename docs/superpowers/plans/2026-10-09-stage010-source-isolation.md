# Stage010 Source Isolation Implementation Plan

> Agentic workers: use the subagent development workflow for independent transport and Stage2 integration tasks, with a final branch review. Keep edits in the assigned files.

Goal: Stage3 may stop, crash or restart while Stage1 UUID and Stage2 processing remain continuous.

Architecture: Stage2 keeps its existing V1 upstream and permanent local nominal demand. A separate StagePublish V2 endpoint publishes already processed FP64 blocks into independent bounded SPSC subscriptions. Core reads passively. Qt remains IPC-only; no DSP mathematics changes.

Spec: user-provided `01_IZOLEAZA_STAGE3_DE_SURSA.md`, F02/F03 in `AUDIT_DOVEZI.md`; copied locally under `local/stage010`. User explicitly authorized Stage2 transport changes and separate v0007 based on installed v0006.

Constraints: preserve Stage1, Stage2 v0006, stable Stage3 and release folders; change only Stage2 transport/lifecycle/control plumbing. No DSP coefficients, gain, filters, SRC law or PCM changes. No push. Do not stop upstream for cleanup. Stage1 may still depend on Stage2 after this phase.

Baseline: installed Stage2 v0006, candidate-stage2-eq runtime and candidate build executable are byte-identical, SHA256 82CA4821DF5651BED38D9841F3BD1CA8794574B0F75EE1E03439FAE2012FE1BE. Copied source is at Integration/Stage2/source. No verified original git commit is asserted for this source snapshot.

## Task 1: shared transport

- [ ] Add `Transport/StagePublishV2/StagePublishV2.h/.cpp` and CMake target `EntropyStagePublishV2`.
- [ ] Namespace `entropy::publish`; API `endpointName()`, `Publisher::start(endpoint,error)`, `stop()`, `setSession(StageBusSessionHeader)`, `service()`, `publish(StageBusBlockHeader, const double* const*)`, `dropped()`; `Subscriber::connect(endpoint,error)`, `close()`, `read(StageBusBlockHeader&, double* const*)`, `heartbeat(qpc)`, `session()`.
- [ ] `ReadResult` values Block, Empty, Gap, Invalid, Disconnected. A Gap never silently replays a backlog. First block establishes the subscriber cursor using real source sequence; later gaps are explicit.
- [ ] Preserve original session ID/epoch/first_frame/block_sequence/QPC/control metadata and 11 planar binary64 channels at 192000 Hz, 1024 frames. V2 validates independently; no V1 contiguous-demand validation is reused for subscriptions.
- [ ] Separate preallocated queue per subscriber. Full drops delivery only; producer never writes consumer read-index. No overwriting a slot under read. Producer publish performs no allocation, blocking, process liveness query or filesystem.
- [ ] Test exact FP64 payload, absent/full consumer, gaps, unsubscribe/crash and fresh subscription. Compile with both MSVC and MinGW.

## Task 2: Stage2

- [ ] Keep DspLocalDemand continuously attached and sole driver; keep existing nominal calendar. Disable V1 external takeover without finish/release to upstream.
- [ ] Publish each completed Stage2 block once to V2. Subscriber death/Release/Full never calls input fault or runtime stop, never marks Source UUID failed.
- [ ] Test control lifecycle and byte-preserved DSP; keep legacy test failures visible where the contract is intentionally replaced.
- [ ] Build isolated Stage2 v0007; preserve original modules and profile configuration. Include transport source/manifest/reproduction instructions.

## Task 3: Core

- [ ] Replace upstream requests/leases and DAC-dependent input pacing with passive V2 reads. Do not run legacy fallback.
- [ ] Accepted controls progress at logical boundaries during starvation without processing invented samples. Preserve audible status until actual DSP audio arrives.
- [ ] Gap/invalid input causes an explicit local fault; no source reset or hidden backlog replay. Quiescent restart/profile/ARM operations must rotate only the subscription and restart native histories deterministically.
- [ ] Validate new transport fixture, IPC control during no input, startup/stop/crash/restart.

## Task 4: delivery

- [ ] One real end-to-end lifecycle scenario: upstream runs, Core start/DISARM/stop/restart then kill only Core; confirm original Source UUID, advancing Stage1/Stage2 counters while Core absent, new Core subscription without Source Retry.
- [ ] Preserve source/DSP integrity hashes and report limitations honestly.
- [ ] Separate Stage2 v0007 and AudioCore 0.1.0 folders, compatible version manifest, launch instructions and rollback. No automatic physical ARM, no push.

Ruling: native worktree tool targeted another repository and failed invalid reference; reuse the already isolated existing worktree on a new `codex/stage010-source-isolation` branch. Original branch/tag v0.0.9 preserved.
