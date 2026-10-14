# B1 independent renderer — local Codex build/prepare only

Read RendererB1/CODEX.md and RendererB1/DESIGN.md. This is a NEW branch/worktree and an offline-file renderer candidate, not another R3 patch and not a completed live dual-DAC Core.

User-authorized scope: compile supplied source, run the one bounded new packet/header test, prepare the supplied verified Stage2 capture with the unchanged saved DSP/profile/PCM24, and package the new single-DAC player. Do not invent architecture changes or repair compiler failures autonomously.

- Branch: chatgpt/stage3-renderer-b1-20261014. Base/report commit: 3e985007885c527b5e3bf6580f853b82bf314d59.
- Use an isolated worktree at D:\Projects\Entropy-Stage3-Recovery-20261013\renderer-b1-source. Do not switch, overwrite or modify the original source checkout or installed R1/R2/R3 packages.
- Pin the exact final SHA from the handoff. Only AGENTS.md and RendererB1/** may differ from the report commit.
- Run RendererB1/Build-B1.ps1 -ExpectedCommit <the supplied SHA>. The script verifies the known Stage2 capture SHA256 and copies the existing saved Documents profile. It builds EntropyPrepareB1, EntropyPlayB1 and EntropyPacketB1Tests, runs that single offline test, then prepares PCM offline. No general CTest, benchmarking, live capture, producer startup, UI, device opening or playback.
- Existing Stage1/2, Crossover, PCA, Matrix, Headroom, PCM24, ClockSync, drivers, compiler floating-point policy, IPP and original profiles remain unmodified. Do not install Faust, Rubato or another dependency for this candidate.
- Stop on the first actionable configure/compiler/link/test/prepare failure and report it. No reset, stash, force, source/test/script edits, alternate binaries or SDK substitution to hide a failure.
- Playback files are private local outputs, not Git artifacts. Never upload PCM/captures/endpoints/profiles automatically.
- Report exact source SHA, exit codes, compiler warnings/errors, player SHA256, capture/profile/prepared-file hashes, source tracked status, and package/Play-DAC-A/B.cmd locations. Explicitly state no live test was run. Do not launch the play commands or send ARM/DISARM; the user controls playback.

The authoring container and Python execution were unavailable (ClientError); the source has not been compiled or tested by ChatGPT. Do not convert source review/Git readback into a PASS claim. Passing the bounded packet test does not prove Windows scheduling or sound quality.
