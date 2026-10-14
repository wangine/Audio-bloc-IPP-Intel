# Stage3 LiveV1.1 — local Codex is BUILD/PACKAGE ONLY

Read RendererLive/CODEX.md and RendererLive/LIVE-V1.1.md. These replace earlier R1/R2/R3/B1/LiveV1 handoff tasks. ChatGPT supplies reviewed source changes; local Codex only updates to the exact supplied SHA and compiles/packages.

Use the EXISTING D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source worktree on chatgpt/stage3-live-renderer-v1-20261014. Require clean tracked sources, fetch the branch, verify the requested full SHA and fast-forward only. No new clone/worktree, reset/stash/force/rebase, unrelated merge, source repairs or deletion of user files.

Run RendererLive/Build-LiveV1.ps1 -ExpectedCommit <supplied full SHA>. Reuse the incremental Core build and verified Qt UI. This revision outputs live\LiveV1.1-20261015 and copies the SAVED profile from live\LiveV1-20261014\profile\listening.entropy. Preserve the original profile and every earlier package. No silent substitution of profiles.

Only Core Release/MSVC/IPP, the established offline --probe and Qt deployment are authorized locally. No CTest, -Verify, CI-fixture rerun, new capture, mapping report, benchmark, test tones, B1 replay or 32k variant. No gain, AUTO mode, filters, matrices, DAC assignments, drivers, Stage1/Stage2, compiler policy or SDK changes.

Scoped normal-user execution/network/file permission may be requested when necessary; no Administrator, global sandbox disable or ACL edits. At the first error, keep logs and INCOMPLETE.txt, report and STOP without editing sources/scripts or replacing dependencies. Never package an old executable as the new version.

Report exact SHA, build/probe/deploy outcome, executable/profile hashes, tracked status and launcher paths. Do NOT start playback/UI, ARM, terminate/restart audio processes, push or merge. The owner performs live listening. New-source selection waits silently until real data is available; general DAC stability, perceived tonal balance and A/B phase alignment are not declared solved.
