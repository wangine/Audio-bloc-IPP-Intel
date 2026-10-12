# Passive StagePublish V2 transport

`EntropyStagePublishV2` links the caller's `EntropyStageLink` target, reusing its
current-user ACL and overlapped pipe-transfer helpers. Define the StageBus and
StageLink targets before adding this directory. No copy of their security symbols
is built into this library.

The DSP output endpoint is `\\.\pipe\Entropy.StagePublish.V2.<current-user SID>`.
The independent Source output endpoint is
`\\.\pipe\Entropy.StagePublish.Source.V2.<current-user SID>`.
It accepts the original ten-channel MIX_CINEMA10 or eleven-channel LRC_CINEMA11
FP64 contract. Fixed queue storage/ABI remains unchanged; only the session's
declared channel count is copied, never an undeclared eleventh Source plane.
It only creates subscriptions. There are no source-demand, lease, release,
reset, retry, or DSP-control operations. The named-pipe server verifies the actual
client process ID, nonce, version, packet size and reserved fields. Each accepted
subscription gets an unpredictable, current-user-only local mapping. The original
256-byte StageBus session is immutable source metadata, not the V2 queue layout.
The separate V2 wrapper defines and validates queue bounds and mapping identity.

Up to four subscriptions each receive a separate 16-slot SPSC ring allocated
before delivery starts. `publish`, `service`, and `setSession` share one synchronized
owner. Stage1 services subscription changes only while its audio owner is
quiescent; subscriber service does not change its processing calendar/session.
Stop that owner before `stop` or destruction. `publish` performs
metadata checks, lock-free atomics and exact planar FP64 copies only. `service`
handles subscriptions, process checks, heartbeat expiry and resource cleanup.
The pipe worker waits only for control handshakes and owner replies.

Full rings drop delivery to that subscription only. The producer never changes
the reader index or overwrites a full slot. A subscriber reports a terminal local
`Gap` on observed delivery drops, epoch change, or unexpected
sequence/frame progression; it does not replay queued backlog. The first delivered
block establishes the cursor from its actual source sequence and first frame.
Malformed blocks produce a terminal local `Invalid`. UUID, epoch, source sequence,
first frame, QPC, control revision and every other source header field are copied
without renumbering. Source `discontinuity_mask` flags are preserved exactly as
metadata in delivered `Block` headers and reported separately by Core telemetry.
The original producer can derive them from cumulative source telemetry; a repeated
source flag therefore does not imply a missing V2 delivery. V2 does not clear those
flags or invent a delivery-gap interpretation for them.

`Subscriber::read` is bounded and contains no OS calls, waits, allocations or
resource cleanup. A terminal result retires the local subscription with atomics.
The connector must call `heartbeat(qpc)` outside the realtime callback: it also
checks publisher process liveness. Five seconds without a heartbeat retires only
that subscriber on the publisher owner. Call `close` on control after synchronizing
with reads; reconnect always creates a new empty queue. Subscriber connect/close
must not race reads or heartbeat; the Core connector controls that lifetime.

Standalone build/test project: `Tests/StagePublishV2/CMakeLists.txt`.
`Tests/StagePublishV2Tests.cpp` checks exact source metadata/FP64 payload, arbitrary
initial sequences, fresh reconnect, independent full/fast consumers, discontinuity,
malformed headers, actual forged named-pipe client PID, heartbeat expiry and
synthetic cross-process crashes. Compiler-independent size/offset assertions pin
the wire layout for MSVC and MinGW. These tests use synthetic processes only.
On the development machine their current-user ACL requires running tests outside
the restricted sandbox; sandboxed MSVC PDB IPC also fails with C1902.
