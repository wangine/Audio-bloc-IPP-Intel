# Entropy AudioCore 0.0.7 — routing cleanup beta test

Separate candidate folder. Stable beta 0.0.3 remains the accepted reference.

Removed the Precise Relative Delay editor and Global Base slider from Qt.
Fine Adjust, its ratio selector, coarse per-channel delay and selectable Thiran
remain. Removed all three common delay parameters from Core metadata,
snapshots and processing; channel delay is used directly, with the existing
FIR compensation and delay transition behavior. Compile-time named indices
preserve the identity of every remaining parameter without callback lookups.

Old profiles still load. Retired common delay fields are discarded when loaded
and are absent from resaved module state, including manual snapshots. They are
not added to individual channel delays. Channel values, gain, routing, device
assignments and unrelated metadata are preserved. Common delay removal
intentionally changes latency in profiles that used a nonzero Global Base.
The only old-ID strings in product code are the import cleanup; there is no
active common delay control, storage slot, setter or processing branch.

MSVC x64/Intel IPP and Qt builds PASS. Twelve focused native checks PASS;
additional nested metadata preservation check PASS; actual Qt-to-Core offline
IPC check PASS. Linear comparison to stable 0.0.3 with its common base explicitly
disabled: bit mismatches 0, maximum absolute error 0. Thiran numeric checks
PASS. Scoped processing audit reports no allocations/deallocations, locks,
waits or I/O. Evidence: validation/routing-cleanup-20261009.

Headroom, Output Matrix, ClockSync, Guardian, PCM24, WASAPI and fractional-delay
filter mathematics are unchanged. No hidden gain or delay redistribution.
No Stage1/Stage2 edits, no stable Stage3 edits, no push/merge.

Use Live-Test.cmd from this folder; the new Core starts DISARMED. If another
version owns the endpoint, close its UI and stop it gracefully through its
Stop-AudioCore.cmd. Keep profiles private and do not mix runtime files.

Publisher isolation is not certified: see docs/INPUT-INDEPENDENCE-20261009.md.
Formal Stage04 FAIL REALTIME and V07 BLOCKED remain visible. Authorized manual
live startup/ARM checks are separate from those historical acceptance gates.
