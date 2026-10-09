# Entropy AudioCore 0.0.9 — nonnegative channel delay

Each channel's delay is now restricted to **0..800 ms**, in MUSIC and CINEMA,
in both AudioCore and Qt. Fine Adjust can reduce a positive delay to zero;
it cannot request a negative delay. Global Base remains removed.

Core rejects negative delay commands and negative values in imported profiles,
including saved scenes and manual snapshots. The error identifies the invalid
parameter. Edit an old profile in its previous version if it contains negative
delays; this version does not silently reinterpret those values.

Accepted positive delays retain their original FP32 parameter encoding and
FP64 processing results. The compatibility encoding does not add audio delay.
Thiran, smoothing, FIR alignment, filter mathematics, Headroom and the egress
engines are unchanged. Qt remains an IPC client.

This candidate has its own folder. Preserve 0.0.8 and the accepted stable beta
reference; do not mix runtime files between releases. Save your current profile
and close Qt, then use the old version's `Stop-AudioCore.cmd` before switching
Core versions. Start this folder's `Live-Test.cmd`, load/select your profile,
and ARM explicitly. No automatic ARM is performed.

Focused native regression tests: 7/7 PASS. Actual Qt/Core two-process tests:
PASS. Evidence and preserved initial failures are in
`validation/nonnegative-delay-20261009/report.md`.

The user tested 0.0.9 and accepted the nonnegative-delay behavior. This is a
scoped user acceptance of the delay change, not acceptance of source isolation.

**OPEN / FAIL — source independence:** the user reports that Stage3 still
blocks the Stage1 source, previously observed when closing/stopping Stage3,
despite the required independent-consumer architecture. The issue remains
unresolved in 0.0.9. Its cause and the exact transport/lifecycle path have not
been confirmed by a new controlled reproduction. No claim is made that Stage1
or Stage2 code is responsible. Passing offline delay and IPC tests does not
prove source independence. See `docs/KNOWN-ISSUES.md`.

Historical Stage04 FAIL REALTIME and full formal V07 acceptance BLOCKED remain
unchanged. FIR8193 x16 remains unauthorized for live. Stable beta 0.0.3 remains
the preserved reference; 0.0.9 is published as a beta prerelease with this issue
visible, not as a fully qualified independent engine.

No Stage1/Stage2 or stable Stage3 changes; no merge into main. Publication is
limited to `codex/crossover-thiran-delay` and the `v0.0.9` release tag.
