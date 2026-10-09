# Stage011 Complete Source Independence Implementation Plan

Goal: Stage1 continues its existing capture/SRC processing when Stage2 or Stage3 closes, crashes, or reconnects. Stage2 accepts a passive FP64 publication and publishes its unchanged DSP output. Each version remains in its own ordered release directory.

Spec: `local/stage011/user-spec.md`, supplied by the user as `02_INDEPENDENTA_COMPLETA_STAGE1_STAGE2.md`. The user now explicitly authorizes Stage1 integration changes in an isolated new release. Previous instruction permits implementation here without further planning approval.

Baseline: Stage010 commit 7222030a9473a8e141d55313f785c06c5a336553; branch `codex/stage011-complete-independence`. Reuse the existing isolated worktree. Stage1 source snapshot copied from the specified 20261005 release into `Integration/Stage1`; original release is untouched.

Constraints: no capture, SRC, Stage2 math, Graph, Qt Stage3, or egress changes. No push/merge. New instances start DISARMED. Preserve real sequence, frame, epoch, validity and discontinuity metadata. Subscriber failure/full affects that subscriber only. Never overwrite a reader's slot or write its cursor. No V1 fallback or upstream fault writes.

## Task 1: shared passive transport (root implementer)

- [x] Test MIX_CINEMA10_V1 exact payload and subscription loss with the existing V2 implementation: expect rejection before the change.
- [x] Generalize existing V2 session validation/copies to 10 or 11 channels, with unchanged wire queue ABI and storage. Add `sourceEndpointName()` (Stage1 output) alongside existing `endpointName()` (Stage2 output). Control metadata remains in each original block header.
- [x] Run V2 MSVC and MinGW tests including existing eleven-channel cases.

## Task 2: Stage1 continuous publisher (isolated implementer)

- [x] Regression test real offline SourceController processing continues with absent/full/closed/crashed subscriptions and unchanged session identity.
- [x] Keep sole local SourceDemandHarness cadence for all source blocks. Keep original executor/program/native SRC functions byte-identical.
- [x] Publish completed 10-channel blocks using V2; only local configuration changes may rotate the source generation/session. Service subscriber connections outside audio callback, synchronized with publication via existing pause boundary if necessary; never race queue lifetimes.
- [x] Retire V1 external demand/lease takeover; Query may report compatibility but Attach/Release cannot control source. No subscriber faults enter global source fault.
- [x] Build ordered Stage1 isolated package with original DLLs and configuration, production transport initialization and explicit incompatible-version diagnostics.

## Task 3: Stage2 passive consumer (isolated implementer)

- [x] Test real V2 10-channel publisher → Dsp runtime → V2 11-channel subscriber, exact bypass result and identity; source survives runtime stop/fault.
- [x] Replace actual DspAutoLink/controller driver with passive subscription, no DspLocalDemand or V1 output-demand driver in production. Execute once per accepted source block using original executor/module order.
- [x] Gap or changed epoch: explicit local history reset/reconnect plus honest metadata. Never reset/Release/fault upstream. No reuse of old samples or renumbering.
- [x] Preserve local controls/profiles and source program semantics carried by headers. No external lifecycle takeover. Create Stage2 v0008 in separate directory.

## Task 4: validation and ordered delivery (root implementer)

- [x] Build/test affected projects and run existing functional regression suite; retain/report incompatible legacy tests explicitly, don't hide failures.
- [x] Integrity SHA256 checks show capture/SRC/native module DLLs/Stage2 algorithm sources/Core DSP/UI unchanged.
- [x] Actual sequence: Stage1 alone → Stage2 → clean stop → forced termination → reconnect Stage2 → reconnect Stage3 once. Stage1 blocks increase and UUID stays fixed without Source Retry.
- [x] Fresh independent code review of transport ownership and runtime lifecycle before delivery.
- [x] Deliver Stage1 new sibling release (same Stage1/runtime/stage1 structure), Stage2 v0008 sibling, Core 0.1.0 compatible launchers/documentation. Preserve private user configuration locally, no bundled secrets/profiles in Git.

Shared interfaces: Publisher and Subscriber accept either 10 or 11 channels; queue stays eleven-plane allocation and only session.channels planes are touched. Publisher `service`, `setSession`, `publish` require one synchronized owner. Stage1 endpoint uses `sourceEndpointName()`; Stage2 output retains `endpointName()`.
