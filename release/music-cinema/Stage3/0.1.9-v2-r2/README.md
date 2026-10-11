# Stage3 0.1.9-v2-r2 delivery

Both 32k/64k runtimes passed the real local retained-ARM and source-recovery audit.
Extract the desired runtime ZIP and run `Live-Test.cmd` for the authorized live
workflow or `Cold-Start.cmd` for a fresh DISARMED Core. Source absence retains
user ARM with physical DAC owners stopped; validated input resumes the same Core.
Explicit DISARM cancels resumption. Serious faults remain fatal.

[SHA256SUMS.txt](SHA256SUMS.txt) identifies all immutable local archives.
See [validation](../../../../docs/STAGE3-SOURCE-WAIT-r2.md) and the
[publication handoff](../../../../docs/PUBLICATION-STAGE1-r7-STAGE3-r2.md).
The full native suites record 56/57 per variant; inherited Stage04Offline remains
failed. The broader Stage05Ui failure is retained and reproduced on the accepted
Core. These finite live results are not full Stage04/V07 qualification.

The source ZIP preserves the exact 2026-10-11 local snapshot, including the older
Stage1 r6 mirror then present inside this repository. Use the separately
published Stage1 r7 source archive and current `Integration/Music/Stage1-v0003`
for r7. The Git test fixture adds a deterministic lifecycle-teardown barrier
on 2026-10-12; production files and tested runtime binaries are unchanged.
