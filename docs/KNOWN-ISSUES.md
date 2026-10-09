# Known issues — AudioCore 0.1.0 local candidate

## Source lifecycle: phase 01 PASS with Stage2 v0007

The user subsequently authorized an isolated Stage2 modification. The compatible
Stage2 v0007/Core 0.1.0 pair replaces Stage3 demand/leases with passive V2
publication. The actual stop/restart/forced-kill scenario retains the same Stage1
UUID and advancing Stage1/2 counters. See [evidence](STAGE010-SOURCE-ISOLATION.md).
The preserved 0.0.9 binary still has the historical defect below. This does not
certify independence of Stage1 from Stage2: stopping Stage2 can still release its
upstream lease. Stage04 FAIL REALTIME and formal V07 BLOCKED remain unchanged.

## Historical 0.0.9 source independence: OPEN / FAIL (user-reported)

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
