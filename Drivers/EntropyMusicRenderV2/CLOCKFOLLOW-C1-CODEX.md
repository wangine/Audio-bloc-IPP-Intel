# Codex — compile MUSIC ClockFollow C1, no deployment

Use only the exact full SHA supplied in the current ChatGPT handoff, on `chatgpt/music-clockfollow-c1-20261015`. ChatGPT has implemented C1 sources. Your task is compilation and reporting, not autonomous driver design or repair.

## New isolated worktree

Existing repository used only to fetch/create the worktree:
`D:\Projects\3Stage-3\Audio-bloc-IPP-Intel`

New worktree:
`D:\Projects\Entropy-ClockFollow-C1-20261015\source`

The original checkout remains on its current branch and revision; no reset/stash/force, source cleanup or checkout there. Fetch the ClockFollow branch once and require FETCH_HEAD to equal the expected SHA. Create the new worktree on the matching local branch at that exact SHA. If the directory or local branch already exists, verify identity and clean tracked files before a fast-forward only; stop on any conflict. Preserve untracked user work and every recovery/B1/LiveV1 package.

This branch starts from `fa9e126d4bcbde2aaaf033de78509c6f601f8dbb`, the driver's inspected build-source baseline. Do not merge it into the current Stage3 or build old Stage1/Stage3 code merely because it is present in the full repository.

## One build command

Read AGENTS.md and `Drivers/EntropyMusicRenderV2/CLOCKFOLLOW-C1.md`. From the NEW worktree:

```powershell
& .\Drivers\EntropyMusicRenderV2\tools\Build-C1.ps1 -ExpectedCommit '<EXACT_FULL_SHA_FROM_HANDOFF>'
```

The script checks source identity, scope and clean tracked state. It builds/runs only `EntropyClockFollowContracts.exe`, a bounded offline clock/packet model with no device handles. It then builds the actual SYS through the existing `Build-Kernel.ps1`, with signing and Inf2Cat disabled. No general CTest or historical acceptance suites are requested locally; CI has separate regression evidence.

Existing extracted SDK defaults to:
`D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\Drivers\EntropyMusicRenderV2\build\kits\sdk`

Its sibling `sdk-provenance.json` is required and copied only as provenance. Existing WDK headers must include `10.0.28000.0/km/portcls.h`. No new compiler/WDK/SDK installation, download or version substitution is authorized. If an existing required path differs, report it and stop rather than editing build scripts. `-SdkDirectory` accepts an explicitly approved alternative path.

Normal-user scoped file/execution/network permission may be requested for the fetch/build. Do not use Administrator, change ACLs or turn off the sandbox globally. This task does not need installation privileges.

Expected NEW compile-candidate destination:
`D:\Projects\Entropy-ClockFollow-C1-20261015\candidate\ClockFollow-C1-0.3.1.0`

An existing destination is refused; retain its logs and use a newly approved OutputRoot. Do not delete incomplete evidence or remove INCOMPLETE.txt by hand. No profile, installed binary, source provider or runtime folder is overwritten.

## Report, then stop

Report full branch/SHA, model and kernel command exit results, first actionable error if any, actual warnings, SYS and INF version, SYS/INF/model hashes, signature state, candidate path and final tracked source status. Keep full logs in the package. Do not call a successful build a live-clock or audio-quality PASS.

At the first failed configure/compiler/link/model/version/package check: STOP without editing source, changing tests, suppressing warnings or repairing dependencies. No retry loop.

NO signing, catalog installation, PnP/INF registration, SYS replacement, service/endpoint restart, reboot, certificate import, boot-policy/test-signing/Secure Boot changes, registry edits, driver query/control tool, live IOCTL writes, audio signals/captures, DAC/ARM access, Stage1/2/3/AUX/CINEMA edits, push or merge. Do not execute any old deployment or Run-Live-Acceptance instructions. The existing .11 driver and current listening setup remain untouched.
