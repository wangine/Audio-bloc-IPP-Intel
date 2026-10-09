# Native live egress execution

User authorized complete auditory testing and physical ARM on 2026-10-09,
with external hardware mute under the user's control. Baseline Stage06
5fa4c636b47b37ede5a18ebbce80e7a35ac2abcc; branch codex/stage07-live-egress.
Existing Stage04/05 architecture and user approval to implement inline without
further planning approvals apply. No DSP mathematics, stable Stage3, Stage2,
Qt audio ownership, mandatory AVX2, clipping, or alternative SRC changes.

Execution: test separate native live DAC owners with injected fake sinks;
wire explicit IPC ARM/DISARM to validated, configured endpoints/lanes and
existing FIFO/ClockSync/PCM24/WASAPI; publish honest state/fault/clock telemetry
to original Qt; preserve Core and live audio when UI closes; regression and
actual-profile offline preflight; fresh numeric/clock/realtime gate and hashes;
then an explicitly authorized hardware check, observe counters and DISARM.
Final fresh-context review and local report; no push.

Each configured DAC owns its COM and sink calls, event waits and original clock.
Preparation, reconfiguration and joins occur after the graph owner is stopped.
Graph publish remains allocation/lock/wait/I/O free. Any member fault requests
silence and closes the armed group. Recovery requires explicit ARM. No endpoint,
profile, gain or physical pin mapping is selected by a hardwired product default.
Guardian remains exact bypass. Loaded profiles never ARM automatically.

Headroom mode and gain are user-owned. AUTO, MANUAL SAFE (including 0 dB),
BIT PERFECT and native-valid active masks retain the original module behavior.
Ledger telemetry is diagnostic; the host imposes no minimum attenuation or
ledger coverage requirement. Original faults/nonfinite output still fail closed.
FIR8193 x16 remains forbidden. Actual profile measurements retain failures and
acceptance thresholds. User-supplied stage 3 2.entropy is a read-only example,
not a fixed product configuration. Raw evidence stays in ignored local/stage07.

Cold-Start.cmd starts Core then the existing Qt UI, enforcing the formal offline
gate for a newly started Core. The latest formal Stage04 deadline verdict is
FAIL; it must not be rewritten as PASS.
For the manual live test explicitly authorized by the user, launch Live-Test.cmd
(optionally -Profile "absolute\\path.entropy"). This passes --authorized-live-test
for this Core process only. It never automatically ARMs, is not saved to profiles,
and reports authorizedLiveTest=true separately from offlineGatePassed=false.
Actual input, profile, device and strict current-profile preflight remain required.
Closing/reopening Qt retains this authorization and the engine state until Core
stops. A fresh ordinary Cold-Start uses the formal gate again.
For an explicitly chosen profile use Cold-Start.cmd -Profile "absolute\\path.entropy".
Core loads the mother Documents/entropy.entropy when no explicit profile is
given, or waits for Create/Load on a new PC. DAC assignment, exact driver PCM24
format and destination pins are chosen explicitly in Devices; no DAC is hardwired.
Select any original native-valid Headroom policy; the host does not change gain
or require a minimum reduction. Allow the original mode transition to settle.
Loaded profiles never ARM; the user must explicitly press ARM. Guardian is exact
bypass. DISARM before replacing devices/profile or saved MUSIC/CINEMA scenes;
select the scene then ARM performs its strict preflight. UI closure/reconnection
does not disarm the native engine. Stop-AudioCore.cmd is an explicit engine stop.

The offline gate is local evidence tied to the exact executable SHA256, not an
authentication credential. Rebuild invalidates it. Reproduce with
tools/Run-Stage07.ps1; it never opens hardware or alters acceptance thresholds.
WASAPI event waits occur outside the measured cycle; wall-minus-wait and thread
cycles are reported separately. Wall-minus-wait can still include descheduling,
so it is not labelled pure CPU execution. Original Stage04 dispatch profiling
provides measured scheduling evidence for the fixed reference configurations.

Native receiver and graph scheduling use owner-thread high-resolution waitable
timers instead of Sleep-based sub-millisecond polling. Real ingress owns source
pacing; the graph does not apply a second period wait. These waits remain outside
DSP processing. A fresh-time retry addresses the immutable StageBus heartbeat
snapshot race without changing its process, fault or one-second timeout checks.
The Core receiver also registers MMCSS Pro Audio / Critical, with scoped cleanup,
matching the graph/sink scheduling class. No external source thread is changed.
IPC shutdown preserves the reply until client EOF or the fixed five-second lease;
subsequent inbound bytes do not count as reply acknowledgment.
