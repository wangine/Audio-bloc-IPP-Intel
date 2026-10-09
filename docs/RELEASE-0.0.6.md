# Entropy AudioCore 0.0.6 — AUTO ARM integration beta test

Separate local candidate; stable beta 0.0.3 remains the accepted reference.

The host ARM check no longer rejects valid original Headroom AUTO gain ramps.
The native ABI aliases ACTIVE to AUTO: AUTO entry, ATTACK and RELEASE ramps
are accepted while original module fault and finite-state checks remain in
place. Unsettled non-AUTO transitions still require completion. No Headroom
mathematics, gain policy, manual setting, ATTACK/RELEASE timing or DSP source
was changed for this correction. Qt is unchanged from 0.0.5.

MSVC x64 / Intel IPP build PASS. Six focused checks PASS: Stage07 live-worker
fixtures (including AUTO ATTACK, RELEASE and entry), profile preflight,
authorization, Headroom unit/no-allocation checks and Core probe. Fixtures
do not open physical DACs. Evidence: validation/auto-arm-20261009.

Live-Test.cmd launches this folder's Core and Qt with the existing user
profile and assignments. New Core starts DISARMED; physical ARM is explicit
and retains strict profile preflight. If another version owns the endpoint,
close its Qt and use that version's Stop-AudioCore.cmd first. Do not mix files
between release folders. Closing Qt preserves the independent Core state.

Stage1/Stage2 sources and processes are untouched. Full publisher isolation
remains BLOCKED by the legacy StageBus demand/lease contract (see
docs/INPUT-LIFECYCLE-20261009.md). Formal Stage04 FAIL REALTIME and V07 BLOCKED
are unchanged. A manual authorized live test does not replace those gates.
No new listening-quality or hardware acceptance is asserted by fixture tests.
