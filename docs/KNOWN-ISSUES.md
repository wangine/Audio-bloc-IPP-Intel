# Known issues — AudioCore 0.0.9

## Source independence: OPEN / FAIL (user-reported)

After accepting the nonnegative-delay change in 0.0.9, the user explicitly
confirmed that Stage3 still blocks the Stage1 source. Earlier reports describe
this occurring when closing/stopping Stage3; restarting the source restores
normal operation. The exact failing lifecycle sequence in 0.0.9 has not been
captured in a new controlled reproduction.

Expected behavior: AudioCore consumes compatible published FP64 input without
affecting the publisher or upstream stages when Core/Qt starts, stops, closes,
or faults. Qt owns only IPC and telemetry. Stage1 and Stage2 must remain
unaffected; fixing this issue must not require changing those stages.

Actual status: the user's observation violates that independence requirement.
The defect is unresolved. Earlier ingress/recovery changes and passing isolated
offline tests do not close it. No root cause is asserted and no upstream blame
is assigned without evidence.

This publication records the defect only; it does not change audio processing,
restart upstream applications, or attempt a lifecycle fix. Keep 0.0.9 marked as
a beta prerelease. Stable beta 0.0.3 remains the preserved reference.

Historical Stage04 qualification remains FAIL REALTIME; full formal V07
acceptance remains BLOCKED. These are separate from the source-independence
defect and are not cleared by user acceptance of the delay control.
