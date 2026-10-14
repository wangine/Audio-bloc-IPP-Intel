# Stage3 LiveV1 — local Codex is BUILD/PACKAGE ONLY

Read RendererLive/CODEX.md and RendererLive/LIVE-V1.md. These supersede R1/R2/R3/B1 diagnostic/build tasks in this branch.

The owner authorized a new live renderer with the existing UI, crossover, MUSIC/CINEMA switch, matrix assignments, PCM24 and saved gain policy. ChatGPT supplied the implementation in Git. Local Codex only compiles/packages the exact supplied revision; do not invent additional changes.

Use the isolated worktree D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source on chatgpt/stage3-live-renderer-v1-20261014. Create it from the existing repository as described in RendererLive/CODEX.md; do not switch or overwrite the recovery/B1 checkouts. Validate the full SHA, branch and clean tracked files. One fetch and an exact fast-forward before building are permitted; no reset, stash, force, unrelated merge or source repair.

Run RendererLive/Build-LiveV1.ps1 -ExpectedCommit <supplied full SHA>. Only the new Core Release/MSVC/IPP is compiled, with the existing offline --probe; the verified Qt UI is reused. The exact pinned WDL source may be fetched by CMake. No dependency substitution, SDK installation, Rust/Faust, global AVX2 flags or floating-point-policy changes.

Profile is read/copied, not edited. Keep all saved HP/LP filters, gain/Headroom mode, routing and endpoint assignments. Do not force AUTO or lower gain to hide a failure. Do not create another mapping report; the owner uses one main DAC and the secondary Scoop output has no current acoustic contribution.

No general tests, CTest, -Verify, benchmarks, tone sessions, live captures, B1 repetitions, 32k builds or diagnostic loops. CI fixtures are for implementation evidence, not the local task. Report the first actionable error and STOP without editing sources/scripts/tests/profiles/SDK paths or bypassing INCOMPLETE.txt.

Scoped normal-user execution/network/file permission may be requested when needed. Do not use Administrator, disable sandboxing globally or change NTFS permissions. Do not treat an access failure as a DSP defect.

Return the exact revision, build/probe/deploy outcome, executable/profile hashes, final tracked status and new Live-Test.cmd path. STOP before playback. No DAC access, ARM, process termination/restart, Stage1/2/CINEMA/driver edits, push or merge. Existing runtime folders remain untouched. Successful compilation is not an audio-quality verdict; inter-DAC acoustic alignment is not certified by LiveV1.
