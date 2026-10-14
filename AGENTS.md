# Stage 3 Recovery R2 — local Codex is BUILD/PACKAGE-ONLY

Read `RECOVERY-R2.md`. R1 and earlier notes are historical, not instructions to resume diagnosis.

The owner authorized R2's input concurrency correction and a separate manual listening candidate. They already tested MANUAL at -10 dB and BITEXACT at 0 dB: do not turn this task into a clipping lesson or an AUTO Headroom adjustment. Do not change gain, profile, DSP, clock, egress or FIFO sizes.

1. Use the EXISTING checkout `D:\Projects\Entropy-Stage3-Recovery-20261013\source` on `chatgpt/stage3-audio-recovery-20261013`. No new clone. Preserve `live\R1-64k` unchanged.
2. Confirm clean tracked files. Fetch once and fast-forward only to the exact full R2 SHA in the handoff. No reset, stash, force, rebase, cherry-pick or unrelated merge. Stop on a different branch, changed tracked files or unexpected history.
3. Run `tools/Build-Recovery-R2.ps1 -ExpectedCommit <the supplied full SHA>`. Compile only Core 64k Release/MSVC/IPP with the existing offline --probe, reuse the already-built Qt UI by hash, deploy into a NEW `live\R2-64k` folder. Never overwrite an existing package.
4. Default runtime profile is a read-only copy of R1's SAVED `live\R1-64k\profile\listening.entropy`. An exact existing profile can be supplied with -Profile. Do not infer unsaved UI changes, edit values, force AUTO/MANUAL, or upload the profile for diagnosis.
5. No general CTest, -Verify, benchmarks, portable fixtures, captures, 32k builds, source repairs or diagnostic loops. Fixtures in Tests are for reproducibility, not the local task.
6. On configure/compile/link/deploy error, report the first actionable error and STOP. Do not modify source, tests, scripts, SDK paths or dependencies to force success.
7. Report full source SHA, build/probe/deploy result, Core/UI hashes, copied-profile hash, final tracked git status and the full new Live-Test.cmd path. Stop before launching it.

No push/merge to other branches, process termination/restart, Stage1/Stage2/CINEMA/driver changes, DAC access or ARM. Playback belongs to the owner. Qt remains IPC-only. Retain FP64/192 kHz, original compiler floating-point settings and IPP dispatch; no global AVX2. The already-tracked SRC tree is not recopied or replaced.

R2 is not an accepted audio reference. A successful build does not establish that the remaining audible roughness is fixed. Do not edit any R1 package to make R2 or use old binaries with a new label.
