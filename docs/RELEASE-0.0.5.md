# Entropy AudioCore 0.0.5 — input lifecycle beta test

Local correction candidate based on Thiran beta 0.0.4. Stable beta 0.0.3 remains
the accepted reference; this candidate is not declared fully independent of
the legacy input publisher. See `docs/INPUT-LIFECYCLE-20261009.md`.

10 focused offline native/control/IPC checks PASS. Local signal rejection no
longer explicitly poisons the publisher's shared bus. Core detaches from the
launcher console; a startup timeout does not kill the live engine. Receiver
replacement rejects queued old-generation audio. Refused/unconfirmed lease
release is visible in telemetry/logs.

**Full upstream transport independence: BLOCKED by the legacy StageBus V1
demand/lease contract.** Downstream crash or heartbeat loss can still propagate
in that contract. No claim of a passive, isolated transport is made.

DSP, FP64/192 kHz, Thiran options, Headroom V2 and Qt are unchanged relative to
0.0.4. No Stage1/Stage2 source or running process was modified for this build.
Historical Thiran numeric PASS, formal Stage04 FAIL REALTIME and V07 BLOCKED
remain unchanged. No new listening or hardware acceptance is claimed.

This folder contains its own runtime, launchers and manifest. Do not mix files
with 0.0.3/0.0.4. Save settings, close Qt, then use the old version's
Stop-AudioCore.cmd for graceful Core shutdown before switching. Live-Test.cmd
starts the new Core DISARMED; ARM remains explicit and uses your own profile
and DAC assignments. Closing Qt preserves Core state. Stop-AudioCore.cmd is
the supported shutdown; do not use forced process termination to switch builds.

If the legacy publisher is already faulted or its lease cannot be released,
this build reports that limitation and does not issue source restart commands.
