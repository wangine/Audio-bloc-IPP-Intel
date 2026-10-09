# AudioCore 0.1.2 — beta test: Crossover exact-zero delay

Separate local candidate for rectification 04, based on the published 0.1.1.
Only the native millisecond parameter adapter changes: requesting exactly zero
now produces exactly zero, preserving Thiran bypass. Previously the offset
quantizer invented a tiny positive delay. Positive stored delay values keep the
legacy FP32 control conversion. No filter, PCA, Thiran, Headroom, ClockSync,
PCM24, Matrix, Guardian, sink or Qt mathematics changed.

Bounded offline probes and Core `--probe`: PASS. CLASSIC/AVERAGED keep their
legacy `(L+R)/2` semantics. PCA is active in the captured MUSIC Bass/Scoop and
is not a transparent sum: measured rank-one gain is +8.53 to +11.04 dB.
PHASE SAFE is inactive and is unchanged. See `docs/STAGE013-CROSSOVER.md` and
`validation/stage013` for the numerical evidence and actual lane configuration.

Historical Stage04 realtime FAIL, retained Stage04 finite-overrange-policy test
FAIL and formal V07 acceptance BLOCKED remain visible. No broad suite or CPU
benchmark was rerun for this bounded correction. Listening acceptance pending.
The accepted 0.1.0 reference and the published 0.1.1 remain available unchanged.

Save the current profile, close Qt and stop the previous Core using its
`Stop-AudioCore.cmd`; leave Stage1/Stage2 running. Start this folder's
`Live-Test.cmd`, load your profile and explicitly ARM. Device assignments come
from your profile/UI. No automatic ARM, source restart or device hardcoding.
`Cold-Start.cmd` starts the normal session without live-test authorization.
Closing Qt leaves AudioCore running with its current state.

Existing saved tiny positive delays are intentionally not guessed to be zero:
request 0 ms explicitly where zero is intended, then save your profile.
Positive accepted delays remain unchanged.

The private CLASSIC comparison profile changes only the active extractor and
retains all filters, gains and delays. The captured Scoop CH12 has HP/LP/AP off;
fixed mono therefore contains broadband signal there. This comparison copy was
not loaded or armed on hardware. Configure the intended Scoop filtering before
auditioning that copy; never audition the synthetic bypass fixtures on speakers.
