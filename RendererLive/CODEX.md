# Codex — compile/package LiveV1.1 only

Use the EXISTING worktree `D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source` on branch `chatgpt/stage3-live-renderer-v1-20261014`. Do not create another clone/worktree or switch the recovery/B1 sources.

Verify clean tracked sources (do not delete untracked user work). Fetch this branch once, require FETCH_HEAD to equal the full SHA from the current ChatGPT handoff, and fast-forward only to that SHA. No reset/stash/force/rebase, unrelated merge or source edits. If there is a conflict or the remote differs from the supplied SHA, report and stop.

Read AGENTS.md and RendererLive/LIVE-V1.1.md, then run from this worktree:

```powershell
& .\RendererLive\Build-LiveV1.ps1 -ExpectedCommit '<EXACT_FULL_SHA_FROM_HANDOFF>'
```

This script reuses build/live-v1 for an incremental Release/MSVC x64/IPP Core build with BUILD_TESTING=OFF, runs only the established offline --probe and packages the unchanged verified Qt UI. Do not rerun CI tests, CTest, -Verify, benchmarks, tone sessions, B1, mapping or PCM captures. No 32k variant, dependency substitutions, SDK/driver installs, Rust/Faust or floating-point changes.

Default input profile is the SAVED file from the currently tested LiveV1 package:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1-20261014\profile\listening.entropy`.

It is copied bit-for-bit, with original/copy SHA256 checks. Do not read unsaved UI state, change gain/mode/filter/DAC assignments or silently substitute Documents/another profile. If the owner specified a different profile, pass that explicit path through -Profile. Existing UI input remains `D:\Projects\Entropy-Stage3-Recovery-20261013\source\build\qt-recovery\EntropyUI.exe`, SHA256 `0F7441559F373407D86894A85124F9C230055173F1F12FDE98F597610EB5E13D`.

NEW output package: `D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1.1-20261015`. Preserve LiveV1-20261014 and all recovery/B1 packages. The script refuses an existing output and retains INCOMPLETE.txt on failure. Never remove that marker by hand. No clean build is required.

Request only scoped normal-user execution/file/network permission if necessary. No Administrator, global sandbox disable, ACL edits or removal of path validation. CMake may access the same pinned WDL revision already used by V1; no new resampler is being introduced.

At the first configure/compile/link/probe/deploy error, retain the full log, report the first actionable error and STOP without automatic code fixes. At success report source SHA, build/probe/deploy exit results, Core/UI/profile hashes, final tracked status and the new Live-Test.cmd/Stop-AudioCore.cmd paths.

STOP before playback. Do not start the runtime/UI, send ARM, stop/restart any audio process, touch Stage1/Stage2/driver settings or push/merge. The owner runs the new UI and listens to normal music. This patch addresses program selection with an absent source; it does not certify all sink faults, tonal balance or inter-DAC phase alignment.
