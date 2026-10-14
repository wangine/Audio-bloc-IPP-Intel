# Stage 3 Recovery R3 — Codex BUILD/PACKAGE ONLY

Read `RECOVERY-R3.md`. R1/R2 and earlier task files are historical; do not resume their diagnostic work.

The owner authorized a targeted clock-observer candidate based on the supplied R2 telemetry. ChatGPT supplies source changes in Git; local Codex compiles/packages only. No autonomous source changes, clock tuning, gain changes, driver changes, or tests against physical outputs.

1. Use the EXISTING checkout `D:\Projects\Entropy-Stage3-Recovery-20261013\source`, branch `chatgpt/stage3-audio-recovery-20261013`. Preserve all existing live/R1-64k and live/R2-64k files.
2. Require a clean tracked tree. Fetch that branch and fast-forward only to the full SHA in the handoff. No reset, stash, force, rebase, cherry-pick, new clone, or unrelated merge. Stop on a mismatch.
3. Run `tools/Build-Recovery-R3.ps1 -ExpectedCommit <FULL_SHA> -Profile 'C:\Users\bobi\Documents\entropy.entropy'`. This is the saved profile path reported by the observed R2 runtime. Read/copy only; no parameter edits or AUTO activation. Unsaved UI state is not inferred.
4. Compile ONLY Core 64k with the established MSVC/IPP wrapper and its offline `--probe`. Reuse the already-built Qt executable by exact hash. Deploy to a NEW `live\R3-64k`; never overwrite an existing runtime. Do not recopy the tracked SRC dependencies.
5. No CTest, -Verify, benchmarks, portable fixtures, live captures, 32k builds, or repair loops. On configure/compiler/link/deploy error, report the first actionable error and STOP. Do not edit code, scripts, build flags, SDK paths, dependencies, or profiles to force success.
6. Report the full source SHA, configure/build/probe/deploy result, new Core and unchanged UI hashes, profile source/copy hashes, final tracked status, and full Live-Test.cmd path. STOP without launching or stopping anything.

The owner performs manual ARM and listening. Do not access DACs, terminate/restart processes, change Stage1/Stage2/CINEMA/virtual drivers, or push/merge to other branches. Preserve FP64/192 kHz, original floating-point compiler policy and IPP dispatch; no global AVX2. A build success is not an audio-quality verdict. R3 is a candidate, not a confirmed fix of the audible roughness.
