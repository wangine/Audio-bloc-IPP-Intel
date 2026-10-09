# FP64 input lifecycle investigation — 2026-10-09

Required boundaries: independent engine, IPC-only UI, FP64/192 kHz. No changes
to Stage1/Stage2, protected Stage3, DSP mathematics, DAC assignments or profiles.

## Observed live evidence

The running 0.0.4 Core loaded the existing mother profile successfully. Initially
it reported WAITING FOR STAGE2, no bound generation, no received blocks, no
faults. Later, after the user's source restart, it reported 40,064 received and
consumed blocks, generation 1, producer PID 12656, validity mask 5, fresh meters,
zero faults, and nonzero L/R/C peaks (approximately .01027/.01798/.03518).
No Stage1/Stage2 process was restarted or changed by this investigation. No DAC
ARM was performed. This establishes actual signal receipt, not end-to-end
hardware acceptance or proof of total transport independence.

## Defects corrected in AudioCore

- Receiver-local rejection of non-finite samples explicitly set the publisher's
  shared fault flag. It also left a consumed request marked pending. It now
  silences/invalidate locally, clears the completed request, and attempts lease
  release without explicitly poisoning the shared bus.
- Exception cleanup now attempts lease release. Unconfirmed/refused release is
  counted in `stage2.releaseFailures` and logged on the transport worker.
- Live launcher readiness timeout no longer uses Stop-Process on Core. The
  running engine is retained for reconnect or explicit IPC shutdown.
- Live Core detaches from the launcher console before acquiring any input lease.
  Offline console CLOSE handlers wait for cleanup within the OS close budget.
- Stop/restart invalidates the generation after the worker's final write and
  discards queued old-generation blocks before restarting.

All changes are lifecycle/transport/telemetry. Qt and DSP/Egress module sources
are unchanged relative to the already validated Thiran candidate 04881bf.
Stable 0.0.3 remains the reference.

## Regression evidence

The real Win32 fixture reproducer first failed with
`Local rejection poisoned publisher before release`; it passed after the fix.
It verifies FP64 identity/order/signed zero, signal rejection, eight receiver
replacement cycles with queued old audio, fresh signal receipt, starvation,
fresh-session recovery and scoped callback allocation/lock/wait/I/O checks.

Console CLOSE reproducer first failed with
`console close returned before transport cleanup completed`; its completion
barrier now passes. The Core integration process is launched in a hidden new
console: before detach it failed with `Live Core still depends on launcher
console lifetime`; afterwards it reports `consoleAttached=false` and passes.

Focused MSVC x64 build and 10 native/control/IPC tests PASS. See
`validation/input-lifecycle-20261009/`. No benchmark or DSP retuning performed.
The formal Stage04 FAIL REALTIME and V07 BLOCKED statuses remain unchanged.

## Total transport independence: BLOCKED for the existing legacy contract

The frozen StageBus V1 is demand-driven, with one leased consumer, shared fault
state and heartbeat/dead-process checks. It is not a passive publication stream.
The original Stage2 downstream service accepts Attach/Release, rotates upstream
sessions, refuses Release while runtime.pending(), and its runtime can fault
both input/output buses on downstream loss. AudioCore cannot guarantee that
such a producer will remain unaffected by receiver crash, lost handoff reply,
transport corruption or an unconfirmed drain/release.

The fixture PASS proves that AudioCore no longer explicitly poisons a publisher
on local sample rejection. Its simpler publisher does not reproduce Stage2's
entire upstream ownership state machine. PASS must not be described as total
isolation. Abrupt process termination remains outside graceful shutdown.

Removing the legacy handoff requires a separately defined passive publication
contract or an independently owned compatibility adapter outside UI and DSP.
Neither is silently substituted here. Reading reusable private ring slots as an
unregistered observer is not accepted as an extreme-quality transport without
an explicit ownership/coherency contract and validation.

Windows CLOSE semantics: the system terminates the process when the handler
returns or its time limit expires. See
[Microsoft HandlerRoutine documentation](https://learn.microsoft.com/en-us/windows/console/handlerroutine).
