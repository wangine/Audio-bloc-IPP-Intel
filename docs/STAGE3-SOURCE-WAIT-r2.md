# Stage3 0.1.9-v2-r2: local source-wait result

2026-10-11. User requirement: keep ARM indefinitely when Stage1/Stage2 stop or restart, mute until validated input returns, and do not fabricate source/Graph PCM. No Git upload in this session.

The required-input worker previously latched faults permanently. It now retires the subscription and reconnects with the existing strict session, channel, sequence and finite-PCM validation. Source faults and gaps remain recorded.

User ARM intent is separate from physical playback. In `WAITING_INPUT`, native DAC owners stop and close; Core, input workers and controls stay alive. The Graph processes no missing-input block. Fresh validated PCM resumes within retained user ARM, using the original native preparation, clock calibration and preroll. There is no waiting timeout. Explicit DISARM cancels pending resumption. An explicit ARM issued while input is absent prepares a fresh inactive session and waits.

Graph/module faults retain fatal provenance even when followed by source loss. Clock, PCM and device faults override source hold. Final faults are rechecked after teardown; fatal teardown advances the lifecycle generation to cancel older queued ARM commands. A new explicit ARM remains possible without Core restart.

## Recorded verification

- MSVC 19.51.36257.0 x64, native FP64, static Intel IPP 2026.0. Qt 6.11.2/GNU 13.1.0 remains a separate IPC client.
- Full native suite: **56/57 per variant**, 32k and 64k. The inherited `EntropyStage04Offline` failure (`unsafe PCM rejected before converter`) remains recorded; no Stage04/V07 qualification is claimed.
- Source wait, new UUID reconnection, no fabricated Graph/sink activity, explicit DISARM, fatal provenance in both orderings, late fatal teardown, stale-session re-ARM and queued-ARM cancellation regressions pass. RED and final GREEN logs are retained under `validation/music-cinema-v2`.
- Typed-source Qt presentation, lifecycle completion/disconnect and device-selection regressions pass. Both packaged Core/Qt offscreen startup smokes pass. The broader `EntropyStage05Ui` fixture still fails; the same failure was reproduced with the unchanged accepted Core in an isolated offline session. Its output is retained separately.
- Protected inventory: 72 files checked, 70 unchanged. The two intentional modifications are local `Engine/Egress/LiveSinkWorkers.*` lifecycle adapters. Pinned DSP, SRC, clock-controller and PCM conversion algorithms are unchanged.

## Real local MUSIC tests

`source-wait-live-64k-complete.summary.json` records PASS with Core **11412** unchanged throughout that audit. `source-wait-live-32k-complete.summary.json` records PASS with Core **19544** unchanged throughout its audit.

Each audit includes Stage1 stop/restart, Stage2 stop/restart, explicit DISARM while waiting, input return without automatic re-ARM, explicit ARM without input followed by resumption, and rejection of unavailable Cinema. Stage2's 64k stop was graceful; the 32k audit also tested abrupt termination of its identified owner when no closeable window was available. Stage1 shutdowns used its guarded launcher and were graceful.

Four waiting intervals per audit were sampled with retained ARM, closed DAC owners, unchanged Graph block count, and unchanged submitted-frame, callback, processed-PCM and nonzero-PCM counters. Valid input resumed the same Core after upstream process/session changes. The final continuous intervals were 60 seconds (64k) and 30 seconds (32k). Both audits ended RUNNING with no output fault, zero MUSIC transport gaps and zero clipped PCM blocks in their final egress sessions. Input fault history from intentional interruptions remains visible: 5 (64k, including the first helper-only attempt) and 4 (32k).

The initial 64k helper attempt retained ARM correctly but stalled in Python captured-output collection after launching a child. Its partial evidence is preserved. The corrected helper uses files for child output; both complete audits passed. This was a test-helper problem, not a Core restart or an audio-binary change.

The final listening session returns to the 64k candidate, Core **21488**, with explicit ARM and a separate 20-second RUNNING snapshot. Original mother-profile SHA256 remained `4D924408BDC06C5BF1DAB4306BC1D87BF0B4700A74F1D65D1D0BB472DB93C8C7`. Its active settings were saved into a new local profile copy for candidate tests; DAC assignments were preserved.

## Scope and limits

No ten-hour wall-clock test was performed; indefinite wait follows the absence of a wait timeout and the exercised wait/recovery path. The real Cinema decoder remains pending; this revision tests its passive contract and absent-input rejection, not real Cinema playback. Physical analog skew, every machine load and original TIDAL PCM identity are not certified by telemetry.

TIDAL kept producing while Stage1 was deliberately stopped. The finite driver ring can lose frames during that consumer absence; Stage1's later raw metadata retains those losses. These restart tests do not establish zero driver-frame loss over intentional Stage1 downtime. Stage1 r7's separate pause/play test and known-pattern transport tools remain available with their own exact binary scope. Kernel 0.3.0.11 and raw Provider are unchanged; actual WASAPI access metadata remains unverified.

Runtime candidates, source archives and test tools are local. Accepted 0.1.9-v2/r6/v0002 artifacts remain preserved. Git HEAD remains `34556e782e47a67417aadbf7b06e12c330f7da10`; nothing was committed or pushed in this session.
