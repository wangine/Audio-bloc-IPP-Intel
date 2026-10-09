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

Historical Stage04 FAIL REALTIME and full formal V07 acceptance BLOCKED remain
unchanged. FIR8193 x16 remains unauthorized for live. This change does not
establish a new listening reference without the user's audio test.

No Stage1/Stage2 or stable Stage3 changes, no merge or push.
