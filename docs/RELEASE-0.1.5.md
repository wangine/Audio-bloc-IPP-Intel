# AudioCore 0.1.5 — stable beta: live MUSIC/CINEMA selection

Fixes the control-plane rejection of every saved scene selection while physical
output is engaged. Core validates the requested saved scene on an independent
offline native graph before publishing the original parameter transaction.
The live graph adopts the immutable snapshot at an audio block boundary;
DAC owners, FIFOs and clocks remain running. Rejected validation leaves the
live scene unchanged. FIR8193 x16 stays unauthorized for live.

Validation includes source-scene warmup, the native 9600-sample crossfade,
target warmup and the existing strict 1000-block preflight. No DSP mathematics,
PCA, Headroom policy/gain, source transport or UI visuals are changed.
Qt remains IPC-only; scene requests use the bounded long-response timeout.
The Core server renews its existing five-second idle lease after the response,
so bounded offline work does not expire an otherwise active connection.

PASS: real offline target-scene preview, all three command spellings, exact
configuration cache, unchanged live scene/Headroom, saved FIR8193 x16 rejection,
native command-server long-handler/idle-lease regression, four Qt timeout
variants, Qt device selection, three existing control/egress tests, MSVC/IPP
and Qt builds. See validation/stage016 for measurements and verification scope.
Historical Stage04 realtime FAIL, formal V07 BLOCKED and residual PCM overrange
from 0.1.4 remain. No full DSP suite/benchmark rerun or new acoustic PASS claim.

Core SHA256: 0EC4B47A9A7F3377D12EE1EC4186949C34F8A0565482426E019610DBF16983EB.
UI SHA256: 5F4D9F90A860F84B6CCA19E8D91DF60B6E0A6EDB5FCBD782CFE3AA2C94643515.

Earlier release folders remain intact. Use this folder's Live-Test.cmd for
the authorized manual live session, retaining your profile and DAC mapping.
New Core starts DISARMED. Select/ARM explicitly; live scene changes validate
before application. A validation failure is reported without changing scene.
Closing Qt leaves the audio engine running. Released as stable beta after the
user confirmed stable live operation and MUSIC/CINEMA switching on 2026-10-10.

Live verification PASS: MUSIC/CINEMA toggled three times by IPC, then in both
directions from the existing Qt selector. Both physical eight-channel DAC
workers stayed ARMED/RUNNING; observed XRUN, deadline-miss, PCM fault, nonfinite
and clipping counters were zero during the recorded IPC toggles. Source1/2
process identities remained unchanged. The UI was left on CINEMA/ARMED with
active input, Crossover, Matrix and Headroom meters. The user subsequently
confirmed stable MUSIC/CINEMA switching and authorized GitHub publication.
This acceptance does not replace the historical Stage04 realtime or V07 gates.

Implementation commit: 9f0db3d8edccacbc68d6241764de698e34cc7dc8.
Runtime binaries are identical to the local 0.1.5 build tested and accepted;
publication changes documentation and release metadata only. Sources, tests,
measurements and reproduction instructions accompany the GitHub release.
