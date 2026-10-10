# Stage019 — audit03 implementation and evidence

Baseline:41f55c4a556ae21e35ee8f711d853f6ceaa2b12c (published v0.1.7). Branch:codex/egress-presentation-rearm. Candidate:0.1.8, two FIFO capacity builds, target4096 in both. User selected capacity-only comparison on2026-10-10.

## Implementation

Normal ARM no longer calls AudioScheduler.stop/start or resets source ingress/Crossover histories. CoreRuntime uses the existing block-boundary control barrier, prepares a separate inactive egress session on the lifecycle owner, then publishes a pointer request. The Graph thread acknowledges attachment/retirement in O(1) at its boundary, even if ingress has no block. A retirement timeout retains resources and refuses replacement. Profile-loading/new/offline commands retain their separate existing quiescent behavior; the normal DAC button path is the changed scope.

Per-DAC cancel handles remain alive until their owner exits. Immediate gate closure is separate from owner join. WASAPI waits audio OR cancel; the WASAPI C ABI is unchanged. Backend Start includes the existing initial SILENT buffer in device-frame presentation accounting. Open/Start/Stop/Close and COM release remain on each DAC owner, not Qt or Graph.

Preallocated sideband identifies every FP64 frame by graph epoch, frame index and graph capture QPC. Capture age is graph-buffer age, not an invented source/ADC latency. Splits/merges/wrap are validated. The real source stream epoch/gaps/sequence discontinuity changes graph epoch; MUSIC/CINEMA parameter changes do not invent transport gaps.

The original frequency estimator and bounded ratio law require real observation time before stable mapped playback. Inactive preparation supplies an actual4096-frame zero prefix to the original clock ring. SILENT hardware observations drive the original estimator/controller until valid estimate plus applied correction within0.1ppm; no fabricated counter/fill, alternative clock law or interpolation is used. Audio is not queued into egress until both enabled domains are calibrated; Graph/ingress continue. Native hardware construction cannot disable calibration; only injected legacy fixtures can explicitly opt out.

Both endpoints open before either Start. Each device reports raw clock frequency, precise native frame position and fresh QPC. Shared future presentation uses each DAC's calibrated rate and accounts for the initial SILENT buffer. Stream latency is exposed with validity; it is not added twice because IAudioClock position already describes playback. Microsoft reference: https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition . Analog residual is unmeasured and stays unclaimed.

Initial graph position is derived from the common presentation contract, with bounded transfer and explicit external callback/producer reserve, retaining4096 internal fill. A nonblocking one-attempt startup CAS serializes the prime decision; the losing callback submits silence, never spins/waits. Initial planning can retry only before mapped output is active. An inability to join an already primed member faults locally instead of silently jumping. Both primed members render mapped silent preroll and cross a future common audible onset, using the largest enabled buffer as lead. Partial onset buffers have only the initial PCM prefix zeroed.

Actual resync count/reset reason, epoch, graph interval, external/internal fill, graph data age, clock validity/rate, common and audible QPC, startup skipped frames and estimated A/B phase are exposed in cached telemetry. Active original recenter/resync produces local silence/fault and explicit re-ARM, rather than a hidden return to LOCKED. CLOCK LOCKED and software mapping verification are separate; analogSkewMeasured always false.

## Targeted evidence

| Gate | 32k | 64k |
|---|---|---|
| Native Core build / --probe | PASS | PASS |
| Original calibrated dual-clock test | PASS | PASS |
|1024/1024 maximum same-time difference|0.015386772 frame|0.015386772 frame|
|512/1024 maximum same-time difference|0.015807012 frame|0.015807012 frame|
|First nonzero onset, both scenarios|0.1728 frame|0.1728 frame|
|PCM ramp oracle within2LSB (excluding discontinuity)|1466+1504 checks PASS|1466+1504 checks PASS|
|FIFO split/merge/wrap identity|PASS|PASS|
|Cancel owners while waiting, bounded boundary ack|PASS|PASS|
|Unsupported driver4096: zero Start/submit, both owners close|PASS rejection|PASS rejection|

Simulation publishes1,536,000 graph frames per case; +100ppm/-70ppm devices,3.7ms clock-start offset and4ms Open delay. Virtual clock time advances through the original4-second estimator; there is no wall-clock soak or hardware test. Positive cases keep the ≤1frame gate unchanged, require actual nonzero PCM and compare numeric samples against the mapped graph timeline.

Targeted regressions: Stage018View, Stage017Lifecycle, Stage07Live, Stage05Boundary, Stage05Control, Stage05Protocol and Stage05Transport PASS. The lifecycle test uses a legacy nominal scripted sink (calibration disabled only for injected fixture) and verifies Graph block count continues after ARM and DISARM. Full calibrated-clock verification is the separate Stage019 model. Allocation assertions in the existing Stage07 producer test remain active. No new full CPU qualification was run.

Integrity: Tests/Stage019Integrity.py compares101 protected files to v0.1.7 and checks original ClockEngine.configure/push/writeNominal/interpolate bodies plus render math, excluding added telemetry copies only. SHA256 list is validation/stage019/source-integrity.json. No edits to Crossover, Matrix, Headroom, PCM24 conversion, Guardian, original estimator/controller/sinc, Qt, Stage1/2, stable references or profiles.

## Failures and limits kept visible

- Before ARM integration fix, structural lifecycle test failed because ARM stopped/restarted Graph: red-lifecycle.txt; current lifecycle-green.txt PASS.
- Development chronology failures and legacy failures remain in validation/stage019; no tolerance was widened.
- The full-target driver4096 experiment failed with5.807994074-frame difference, while onset0.4416frame passed: chronology-large-buffer.txt. Required sinc guard makes steady fill4196; original backlog controller then adds17.3611ppm. This driver size is rejected before group Start in this fixed-target variant (buffer+100 >4096). The same restriction applies to both FIFO capacities. No success is claimed for that unsupported configuration.
- Initial32k startup could overflow after repeated incomplete primes under concurrent compiler load. External startup reserve now covers one callback phase and one producer/peer boundary; raw development failures describe incomplete prime data. Final32/64 software cases pass. Real scheduling qualification still requires later evidence.
- Legacy lifecycle fixture initially failed its2-second RUNNING check when implicitly enabling new real4-second calibration; it now explicitly models nominal lifecycle timing, with no production timeout/estimate gate relaxed. It also no longer restarts Graph around ARM.
- Stage05Transport initially failed local client connection in restricted sandbox; identical test passed outside sandbox, recorded in transport-unsandboxed.txt. No IPC production code was changed for this.
- Fresh MSVC CMake Debug probe encountered C1902 PDB manager mismatch in sandbox; Release try-compile avoids unnecessary debug PDB tooling. Local pinned SRC dependency path must be supplied for fresh worktree builds. PowerShell logging no longer treats vendor CMake stderr warnings as successful-build failures; process exit status remains authoritative.
- Historical Stage04 FAIL REALTIME and formal V07 BLOCKED remain. No new median/p99/maximum benchmark or analog skew measurement. Live audible acceptance of both0.1.8 variants PASS USER CONFIRMED on2026-10-10; user could not distinguish their sound. Software/analog equivalence is not claimed. A later single read-only live snapshot showed estimated A/B=-82.84245571565629frames, softwareAlignmentVerified=false, resyncCount=0, faults=0 and deadlineMisses=0 on bothDACs. Preserve this unresolved software alignment limit; analog skew remains unmeasured. Contract software verificat; skew analogic nemasurat.

## Reproduce / live test

1. From this source tree: powershell -File tools/Build-Stage019.ps1 -FifoFrames 32768 -Verify (insert a space between -FifoFrames and32768). Repeat with65536. Installed local MSVC/IPP/Qt paths are in the script; -ReferenceDirectory selects the pinned original SRC dependency checkout, extracted with tools/Stage04-References.ps1 when absent.
2. Source guards: Python Tests/Stage019Integrity.py and Tests/Stage019LifecycleSource.py. Restricted named-pipe sandbox can require ordinary host execution for the IPC fixture.
3. Package via tools/Package-Stage019.ps1. Identical UI is used in both. Manifest includes separate capacity/target and SHA256. No SDK, build folders or private profile is packaged.
4. Save profile, close old UI, stop old Core with its own stop launcher. Run exactly one candidate's Live-Test.cmd. Explicit ARM; wait for the original silent calibration/preroll. Compare32k/64k with the same profile/4096target. Stop one Core before switching variants. Closing UI alone preserves Core.
5. Verify re-ARM/MUSIC/CINEMA and ordinary stop without source changes. Any fault requires explicit local recovery; do not interpret LOCKED as measured analog alignment. No automatic DAC launch was performed during packaging.
