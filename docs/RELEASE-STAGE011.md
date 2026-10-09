# Stage1 v0002 / Stage2 v0008 — passive publication beta reference

The user accepted this pair after listening on 2026-10-09: it behaves correctly,
sounds good and the highs are clean, with no perceived artifacts. This is a
listening observation, not a claim of laboratory acoustic validation or a change
to historical realtime qualification.

Two separate downloadable releases in `wangine/Audio-bloc-IPP-Intel`:

- `stage1-passive-v0002`: Stage1 continuous producer, original capture/SRC/native
  DLLs and UI, passive ten-channel publication. Folder `Stage1/runtime/stage1`.
- `stage2-v0008-passive-source`: Stage2 passive source subscriber, original native
  DSP, eleven-channel publication for unchanged AudioCore 0.1.0.

Each ZIP contains its executable, required runtime/native libraries, manifest,
launcher, source snapshot and report. User profiles/local state, build trees and
Intel SDK are excluded. The native executables were verified against the final
package manifests before publication. Code provenance:
`4e6af4f9fa91d3b8d6e61b3bc684384fc756843d`.

Sources are separated in `Integration/Stage1` and `Integration/Stage2`; the shared
wire transport lives in `Transport/StagePublishV2`. No merge into main. Original
release folders remain untouched. New Core starts DISARMED; physical ARM remains
an explicit user action.

Actual lifecycle independence PASS: fixed Stage1 UUID, zero source faults during
Stage2 clean exit/termination/reconnect and Core shutdown/reconnect. Core final
31/31 functional tests PASS. Stage1 33/37 and Stage2 21/44: retained legacy V1/title
failures are fully named in `docs/STAGE011-COMPLETE-INDEPENDENCE.md` and included
logs. Do not interpret the release as an all-tests-green declaration.

This accepted pair is the reference for rectification 3. Changes to Headroom will
be delivered in a new version and do not overwrite these releases.
