# Entropy AudioCore 0.0.8 — V07 host integration candidate

Stable beta 0.0.3 remains the accepted listening reference. This candidate has
its own release folder; do not mix runtime files between versions.

Restored the exact V07 Headroom page and its original global 16-channel host
ledger, complete IPC telemetry, native timing controls and live input meters.
Matrix source activity follows the captured Crossover topology; dormant routes
are retained and transition contributors remain available. No DSP coefficients,
Headroom engine, gain policy, PCA or filter mathematics were changed.

The input worker uses V07's bounded recovery calendar and heartbeat sampling
order. A successful strict profile preflight is reused for exactly the same
settings and saved scenes; viewing a channel does not discard that result.
Loading/creating a profile clears it, changed settings are compared exactly,
and a failed validation cannot reuse an older PASS. No gate was relaxed.

Start `Live-Test.cmd` for the explicitly authorized listening test. Core starts
DISARMED; select/load your profile and press ARM once. The first validation is
still deliberate; subsequent ARM of the unchanged profile reuses its result.
Use `Cold-Start.cmd` for a session without manual physical-test authorization.
Qt communicates only through IPC. Closing Qt preserves Core state; use
`Stop-AudioCore.cmd` for a graceful Core shutdown instead of terminating it.

Detailed measurements, numerical differences, regression failures and limits:
`validation/v07-host-integration-20261009/report.md`.

The Crossover oracle compares the actual V07 DLL in legacy linear-delay mode,
with retired Global Base values zeroed explicitly. It is a scoped comparison;
it does not certify Thiran against a reference that has no Thiran implementation.
Historical Stage04 FAIL REALTIME and full formal V07 acceptance BLOCKED remain
visible. FIR8193 x16 remains unauthorized for live. Listening acceptance is
pending the user's test; short live checks do not certify long-term stability.

No Stage1/Stage2 changes or restarts, no stable Stage3 changes, no merge or push.
