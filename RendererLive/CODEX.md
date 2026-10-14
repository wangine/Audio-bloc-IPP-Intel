# Codex — compile/package Stage3 LiveV1, no new diagnosis

The owner has approved this implementation. Work only on the exact final commit supplied in the ChatGPT handoff. This replaces the physical renderer for LIVE MUSIC with the existing Qt UI; do not repeat B1, PCM capture or mapping tasks.

## Isolated source checkout

Start from the existing repository at:
`D:\Projects\Entropy-Stage3-Recovery-20261013\source`

Require clean tracked source files, leaving untracked user work alone. Fetch only `chatgpt/stage3-live-renderer-v1-20261014`. Confirm FETCH_HEAD equals the full expected commit. Do not check out this branch in the recovery or B1 directories.

Create the new worktree `D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source` on local branch `chatgpt/stage3-live-renderer-v1-20261014` at that exact SHA. If that worktree already exists, validate that it is the same repository/branch with clean tracked sources; only a fast-forward to the supplied SHA is authorized. If a conflicting path/branch exists, report and stop. No reset/stash/force/rebase, deletion of user work or unrelated merge.

Open the new worktree and read its AGENTS.md and RendererLive/LIVE-V1.md. Preserve all previous runtime/source folders, main and tags.

## Build command

From the new worktree, using the actual supplied SHA (not this placeholder):

```powershell
& .\RendererLive\Build-LiveV1.ps1 -ExpectedCommit '<EXACT_FULL_SHA>'
```

Default inputs already known:
- Saved profile: `C:\Users\bobi\Documents\entropy.entropy` (read/copy; no edits).
- Existing unchanged UI: `D:\Projects\Entropy-Stage3-Recovery-20261013\source\build\qt-recovery\EntropyUI.exe`.
- Expected UI SHA256: `0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D`.
- Output: `D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1-20261014` (NEW folder only).

The script configures a NEW build/live-v1, BUILD_TESTING=OFF, compiles ONLY EntropyAudioCore with the existing MSVC x64/IPP toolchain, runs the existing offline --probe and packages the unchanged Qt UI/runtime. CMake retrieves Cockos WDL at its exact pinned commit; scoped network permission is authorized for that source fetch. No Rust/Faust, driver, new SDK or global AVX2 installation/change. Do not recopy the tracked old SRC tree.

Request scoped execution/file permission if the sandbox blocks approved local source/profile/build access, using the normal Windows user context, not Administrator. Do not disable the sandbox globally, edit ACLs or remove path validation. The previous B1 Access Denied was an execution-context issue, not a reason to edit ProfileStore.

No CTest, -Verify, contract-fixture rerun, benchmark, new capture, mapping report, test-tone run, B1 replay or 32k variant. No source edits or automatic repair loop. On the first configure/compiler/link/probe/deploy failure, retain full output and report the first actionable error. Never remove INCOMPLETE.txt by hand or relabel an old Core.

## Report and stop

Report only: source SHA and branch; configure/build/probe/deploy result and actual warnings/errors; new Core hash, unchanged UI hash, original/copied profile hashes; final tracked status; full new Live-Test.cmd and Stop-AudioCore.cmd paths. No audio-quality claim.

Do NOT launch the new runtime or UI, ARM, terminate/restart any audio process, change Stage1/2/CINEMA or drivers, change gain/filters/DAC assignments, or push/merge. The owner starts the new visible UI and listens to normal live music manually. Existing HP/LP and other speaker settings stay in force. The secondary DAC does not gate the main DAC; exact A/B phase alignment remains unqualified.
