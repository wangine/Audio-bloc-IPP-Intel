# Stage 3 audio recovery — local Codex is BUILD-ONLY

This branch was requested by the owner on 2026-10-13 to separate source analysis and changes from local Windows compilation. These instructions replace the historical Stage01/Stage02 baseline workflow in this branch.

## Current status

- Working branch: `chatgpt/stage3-audio-recovery-20261013`.
- Source starting point: `c2b1bc5060fc0c5efeaa7b1dacc84dcba4938ebb`.
- Initial recovery handoff changes documentation only. It is NOT an audio fix and is NOT approved as a better-sounding version.
- Read `STAGE3-RECOVERY.md` before any commands. Other release notes are historical evidence, not instructions to resume old tasks.

## Your authorized job

1. Work only in a NEW clone at `D:\Projects\Entropy-Stage3-Recovery-20261013`. Never repurpose or overwrite an existing nonempty directory.
2. Record branch, full HEAD SHA and clean tracked-file status. Stay on that exact revision for this build. Do not pull, merge, rebase or cherry-pick during the task.
3. Compile the checked-in Stage 3 Core in Release, MSVC x64, with the existing Intel IPP installation. Compile the separate Qt UI using the existing UI script. Start with the 65536-frame FIFO variant only.
4. Use the precise build-only commands in `STAGE3-RECOVERY.md`. No general CTest run, benchmarks, prolonged live captures, automatic repair loops or `-Verify`. The existing Core `--probe` is the only permitted runtime check; it is offline and does not authorize DAC playback.
5. If compilation fails, report the first actionable configure/compiler/linker error and STOP. Do not edit C++, headers, CMake, scripts, tests, dependency implementations or profiles to make a build pass. Report missing SDK/dependency paths rather than silently substituting another version.
6. Report full source SHA, exact build commands, exit codes, executable paths and SHA256, dependency provenance, and final tracked-file status. Do not claim audio quality or a corrected regression from a successful build.

## Boundaries

- No edits to DSP, graph, FIFO, scheduler, ClockSync, PCM24, Headroom, Guardian, UI or audio parameters. Source changes will be supplied in reviewed commits, not invented during local compilation.
- Do not install, rebuild, restart or alter Stage1, Stage2, CINEMA or the virtual driver. Do not alter live profile files, DAC assignments or the existing stable/runtime folders.
- Never add gain, compression, limiting, normalization, hidden SRC, buffering changes, automatic phase/delay correction or drop/duplicate audio blocks to suppress a symptom.
- Preserve FP64/192 kHz processing, existing current input contracts, independent per-DAC egress and separate IPC-only Qt UI. Do not infer the active ABI from old release notes.
- Do not enable global AVX2 flags or change the floating-point policy. Preserve the repository's compiler flags and IPP dispatch.
- Do not run old packaging scripts into the original release folder. Do not copy old executable binaries or old PASS reports into a new recovery result.
- No automatic ARM, DAC access, process termination, session switching, listening tests, push or merge. The owner controls playback and authorizes any later expansion of scope.

No additional diagnosis, test campaign or source patch is part of this local build-only task.
