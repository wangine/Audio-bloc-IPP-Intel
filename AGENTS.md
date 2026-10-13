# Stage 3 Recovery R1 — local Codex is BUILD/PACKAGE-ONLY

Read `RECOVERY-R1.md`. It supersedes the initial documentation-only `STAGE3-RECOVERY.md` handoff and historical release/task instructions.

## Authorized current task

The owner requested the R1 Core delivery corrections and a separate runtime for a MANUAL listening test. Source changes were supplied in this Git branch. Codex must not diagnose or invent additional fixes.

1. Use the existing checkout `D:\Projects\Entropy-Stage3-Recovery-20261013\source`. Do not clone again or work in its parent directory.
2. Confirm `chatgpt/stage3-audio-recovery-20261013` and no tracked modifications. One fetch and fast-forward to the exact final commit supplied by the owner/ChatGPT handoff is authorized before building. Stop if the local tree/branch does not match; never reset, stash, discard work or merge unrelated changes.
3. Run `tools/Build-Recovery-R1.ps1 -ExpectedCommit <the supplied full commit>`. It builds ONLY Core 64k and runs the established offline --probe. It verifies and reuses the Qt executable from the initial local build report, deploys its runtime libraries and creates a NEW isolated R1 package.
4. The saved profile may be READ and COPIED to the package. Default is Documents/entropy.entropy; use -Profile only for an exact existing local profile path. Do not invent a profile, alter parameters or request its upload for diagnosis.
5. No general CTest, -Verify, benchmarks, new captures, diagnostic campaigns, 32k build or automatic repair loops. The portable R1 fixture is retained for reproduction, not part of this local build task.
6. On configure/compile/link/deployment error, report the first actionable error and STOP. No C++, header, CMake, script, dependency or profile edits to force success.
7. Report source SHA, build/probe status, Core/UI SHA256, final tracked git status and full path to the new Live-Test.cmd. Stop before running it.

## Immutable boundaries

- Never modify the installed Stage3 runtime, original release folders, main/tags, Stage1, Stage2, CINEMA, drivers, DAC assignments or original saved profiles.
- No push, merge into other branches, automatic process termination, source restart, DAC access, ARM or playback. The owner starts the new package and performs the listening test.
- No gain, normalization, limiter, hidden SRC, latency/FIFO/clock/interpolator changes, protection bypass or sample drop/duplicate to hide an issue.
- Keep MSVC x64 / FP64 / 192 kHz / original IPP dispatch and compiler floating-point flags. Do not introduce global AVX2. Qt remains a separate IPC-only process.
- Preserve the existing R1 package if already present; use a new explicitly recorded OutputRoot rather than overwriting it.
- Successful compilation is not a sound-quality verdict. R1 is a candidate, not an accepted stable reference.

Dependencies: the initial local report verified that the 198 SRC files are already tracked and match the indicated local reference. Do not recopy or replace them based on the older handoff's mistaken assumption that they were absent.
