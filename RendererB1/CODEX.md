# Codex — B1: compile and prepare the independent output path

## Deliverable and boundaries

This is the first implementation of the agreed finite-file/single-DAC renderer experiment (stage B). It replaces the old delivery path in THIS executable, without modifying the installed live Core. It is not a complete live renderer with elastic buffers/ASRC A/B and is not an audio fix already proved by the reports.

The Stage A result is sufficient to leave Stage1/r8brain and the published Stage2 L/R alone for this interval. The existing full Stage2 capture, including its processed CX channel, is the input here. No new Tidal/RAW acquisition is requested. The saved capture stopped at a Gap; we only replay its complete validated prefix, not pretend that the requested 112 seconds were captured.

## 1. Isolate the checkout

Start from the EXISTING checkout:

D:\Projects\Entropy-Stage3-Recovery-20261013\source

Read the final SHA supplied by ChatGPT in the handoff. Call it EXPECTED_SHA below. Do NOT use a guessed SHA, a moving branch tip, or any placeholder literally.

Require a clean tracked working tree. Do not discard untracked user work. Fetch only:

```powershell
git fetch origin chatgpt/stage3-renderer-b1-20261014
```

Check FETCH_HEAD equals EXPECTED_SHA. Create a NEW worktree only if this destination and local branch do not already exist:

```powershell
git worktree add -b chatgpt/stage3-renderer-b1-20261014 `
  'D:\Projects\Entropy-Stage3-Recovery-20261013\renderer-b1-source' EXPECTED_SHA
```

If the path/branch exists or any Git step fails, STOP and report. Do not delete it, reset, switch the original branch or clone another unrelated repository.

Open only the new worktree. Read its AGENTS.md and RendererB1/DESIGN.md. Validate git rev-parse HEAD and git diff --name-only 3e985007885c527b5e3bf6580f853b82bf314d59 HEAD. Changes must be confined to AGENTS.md and RendererB1/.

## 2. Build and prepare locally

```powershell
Set-Location -LiteralPath 'D:\Projects\Entropy-Stage3-Recovery-20261013\renderer-b1-source'
& .\RendererB1\Build-B1.ps1 -ExpectedCommit 'EXPECTED_SHA'
```

Replace EXPECTED_SHA with the actual full SHA. Defaults:

- Capture: D:\Projects\Entropy-Stage3-Recovery-20261013\pcm-frontier-20261014\stage2-fp64.bin
- Capture SHA256: 737dad1df5d0f2236219caa954c46d501e10254041acd4c51470d2beea89c051
- Profile: C:\Users\bobi\Documents\entropy.entropy (the SAVED file, copied and hashed, never edited).
- New package: D:\Projects\Entropy-Stage3-Recovery-20261013\live\B1-20261014

The script configures an isolated build through RendererB1/CMakeLists.txt and compiles only three executables. EntropyPrepareB1 uses existing native profile/DSP/PCM24 code offline; EntropyPlayB1 links only Windows system libraries. No Qt UI is required. Existing dependencies already in Git are not recopied. Existing local MSVC/IPP installations are used without changing flags or enabling AVX2 globally.

The single packet test checks byte preservation and ordering under different packet sizes, partial final packets and failed/double admission/commit. It is not a new general test campaign. Its tests were authored before the implementation but could not be executed in ChatGPT's failed runtime; local execution is mandatory. A compiler error, failing check, malformed capture, AUTO profile, PCM overrange or deployment error stops the task. Report the original failure, do not edit sources or reduce gain to force success.

Offline preparation reads the entire known Stage2 capture, applies the SAME saved MUSIC profile including crossover/delays/matrix/manual-or-bitperfect gain, and runs the ORIGINAL PCM24 algorithm with a recorded deterministic flat TPDF seed. It then appends one second of declared zero input, rounded to 188 blocks, for normal short DSP tails. This is explicit EOS material, not source-gap concealment. No extra gain, normalization, resampling or limiter. It is not an independent audit of the unchanged DSP's mathematics, and its cold-start state does not match an arbitrary mid-stream live state.

The result contains per-DAC PCM files, original saved physical lane mapping, SHA256 manifest and manual play launchers. A fresh output directory is required. An incomplete run leaves INCOMPLETE.txt. Do not remove that marker manually to bypass a failure.

## 3. Stop and report

Report source SHA, configure/build/test/prepare exit codes and first actual error if present, complete warnings, unchanged tracked status, input capture/profile hashes, player hash and PCM hashes, source/tail frame counts, and full launcher locations. No claim of audible repair. Do NOT launch the player, stop another Core, change Stage1/2, update drivers or send ARM.

## Manual operation (owner only, after successful build/prepare)

DISARM the existing Stage3, close its UI and stop its Core using its own Stop-AudioCore.cmd. Stage1/2 may remain running; B1 does not subscribe to them and does not control Tidal. Playback is the SAVED diagnostic capture, not the current live track.

Start Play-DAC-A.cmd OR Play-DAC-B.cmd in the new B1 folder, never both. The chosen profile mapping/filters are baked into the PCM. Do not change physical speaker wiring or assume all lanes are full-range. Confirm a safe amplifier level, then type ARM in the player's console. Ctrl+C stops only this player. The shared renderer-owner mutex refuses coexistence with another Stage3/B1 instance. No sink opens merely because the build finished.

First use the unchanged driver setting and default requested 1024-frame WASAPI buffer. The player prints the ACTUAL negotiated size. It has no old 4096 target/guard rule, ASRC, ppm servo, FIFO recovery, live input or graph workload during playback. It does not fall back to shared mode or a different PCM format.

This is not the final live two-DAC architecture. If the saved passage becomes clean, the old live delivery path/work scheduling remains implicated but individual removed components are not yet identified. If the same roughness remains, the shared DSP/output hardware must be investigated rather than declaring this new sink a success. Returning to R3 means stopping B1 and starting the preserved R3 package; no installed executable is replaced.
