# Candidate0.3.0.1 — source review and qualification boundary

2026-10-11. One independent fresh-context review covered architecture baseline
`62c69b7` through candidate `b8139b1`. Its verdict was **not ready for development
installation**: nine Important findings, two deferred Minor findings and four
areas requiring loaded-driver evidence. A single source fix pass followed; no
second review was requested. This report is a source/offline record, not a
loaded-driver approval or zero-loss transport certificate.

## Important findings and changes

| Finding | Correction | Evidence and limit |
| --- | --- | --- |
| Provider idle notification preceded reader-mutex destruction | Unlock reader before decrementing active reads; no mutex access after close may destroy the session | `CloseWaitsUntilReaderMutexReleased` failed first, then passed including actual heap destruction while the exit barrier remains blocked |
| Publication contention rejected valid SetWritePacket commits | Separate bounded SPSC commit inbox; snapshot without publication gate; chronological loss markers or explicit terminal capacity fault | Inbox contention/concurrent immutable-copy fixtures; baseline source contract FAIL, corrected contract PASS; kernel compiled |
| Skipped OS packet number blocked recovery; one-notification callback was treated as late at packet start | Extend actual modulo packet index, record unwritten gaps, reject only a completely elapsed packet | `RecoversAfterUncommittedUnderrun` failed first, then passed; delayed one-notification callback test passes |
| QPC elapsed time certified unproven position-only DMA | Production refuses zero-notification allocations before DMA allocation; offline position model is research only | Production source contract FAIL on baseline, PASS on candidate; allocation policy test passes; polling remains unqualified |
| Position capture failure prevented PAUSE from freezing clock | Always pause clock; latch terminal capture fault until STOP/new generation | `PositionCaptureFailureStillFreezesClock` failed first, then passed |
| lostFrames was a per-gap delta | Publish cumulative generation loss, preserving chronology of pending/ready overflow; audit records maximum cumulative value | Overrun-followed-by-contiguous-packet regression failed first, then passed; cumulative audit fixture passes |
| Lifetime/ended loss disappeared after Free/format change | Device-lifetime ledger; archive once after writer/read teardown and before next owner becomes eligible; failed unregistered allocations do not erase evidence | Lifetime/free source contract FAIL on baseline, PASS on candidate; independent allocation/duplicate-archive fixture passes; actual PnP lifetime still needs live testing |
| Verifier learned expected format from first captured packet | Requested rate/container and reported generation supplied independently; validate complete format/original bits/QPC frequency | Missing expected-generation interface failed before implementation; wrong-rate/wrong-generation fixtures and baseline source contract confirm rejection |
| Stale package and mutable checkout binaries could be qualified | Fresh package includes its runner, manifest validator and user binaries; every package hash, installed SYS and frozen Core hash checked before audio; requested format/generation bound per case | Old package FAIL in two delivery tests, including acceptance of altered DLL; fresh package tests are recorded separately in validation report |

Six source-integration checks fail on `b8139b1` and pass on corrected sources.
They guard actual kernel/runner bindings and complement executable model,
Provider and verifier regressions. They do not execute PortCls or certify kernel
lifecycle safety. x64 Release kernel and user-mode components compile with
warnings treated as errors. Complete suite/package outcomes and hashes are in
`validation/music-render-v2/` and the delivery manifest.

## Decisions that constrain qualification

- The current branch/directory is used as requested; stable source/runtime
  snapshots remain protected. The cost is directory/branch isolation rather
  than another checkout.
- Missing matching SDK tools were supplied by pinned workspace-only NuGet
  extraction. The cost is an explicit local build dependency, with provenance.
- Private PCM IOCTLs use a registered PortCls reference interface and immediate
  data/no-data completion. User-mode cancellation owns waits; the cost is that
  actual dispatch/IRP/PnP safety still requires loaded-driver evidence.
- Finite Exclusive Event test output rounds to complete buffers and compares
  the actual emitted frame count in full. WASAPI has no client EOS flag; native
  partial EOS remains a separate live requirement. A finite unwritten interval
  after the requested tail is not labeled loss; interior gaps fail full frame
  coverage. The cost is no live EOS guarantee before its own trace.
- Source/tool/INF reproduction is recorded; byte-identical PE/PDB rebuilds are
  not promised. The cost is different rebuild hashes, each freshly manifested.
- Review ran before kernel deployment, and all nine Important findings entered
  one source fix pass. The cost is a predeployment source review, not a second
  independent verdict on the corrected binary.
- Position-only allocation is refused because OS ownership remains unproven.
  The cost is explicit failure for a polling/shared path using that allocation;
  it cannot be advertised as supported merely because the model passes.

## Reviewer declined to judge — rulings

1. Loaded PortCls dispatch, PnP/power/removal and unsupported private IRPs:
   not qualified by source inspection; test only after explicit installation
   approval. Cost if wrong: a kernel lifecycle fault could still occur.
2. Real OS DMA ownership and WASAPI servicing: only packet-commit ownership is
   implemented in production; actual modes must be traced. Cost if wrong:
   a requested polling/shared case fails and remains unsupported.
3. Live partial EOS, TIDAL and listening: offline results do not stand in for
   acceptance A/B/C. Cost if wrong: first/tail continuity or TIDAL behavior is
   still unknown until the corresponding complete trace.
4. Signing/deployment: an existing trusted development certificate is an
   available route, not code validation or installation authorization. Cost if
   wrong: signature/policy may reject loading; no trust/boot changes are implicit.

## Deferred minors

- Controlled patterns lack an explicit full-scale-extrema set. Existing
  comparisons are byte exact over their emitted pattern range, but do not yet
  probe clipping behavior above that range.
- Runtime Info does not expose capacity/max-packet/write-lead limits. Fixed
  bounds are documented/tested; those extra telemetry fields remain deferred.

## Next authorized boundary

Present the unsigned manifested package first. After **explicit approval**, use
the already trusted `CN=Entropy Audio DEV Driver Signing` certificate
`89EE6700145A65ED29B0B129052933D17560AFB3`, if still available, to create a separate
signed package and updated manifest. Register only `ROOT\EntropyMusicRenderV2`
with the installed Microsoft x64 devcon, verify its exact service/private GUID/
container/render identity, then run isolated frozen-Core qualification. Do not
change system default, active providers, old drivers, boot/trust policy or DAC.
Stage1 v0002 remains unchanged; AUX investigation follows driver qualification.
