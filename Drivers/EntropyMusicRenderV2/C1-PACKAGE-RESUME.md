# C1 packaging correction — reuse the already compiled SYS

15 October 2026. This is a packaging-only correction; no C++ source, header, driver project, INF template, PCM/clock code or audio runtime is changed.

## What happened

The local report states that source `3faccc678242f2815cc61eeb8f49a2f05c0c85b5` passed the offline contracts and compiled kernel 0.3.1.0. Build-C1.ps1 then recursively found two byte-identical stamped INF files and incorrectly required a count of exactly one. The report supplies:

- SYS SHA256: `F7703F8A004535031EF9F96795E0AEDF1779F75EC1050EF8E48F808ACA6A6764`.
- Both INF copies SHA256: `5834F5F6BB05741D341000F74B69EF919FDDD4CABF32D8F552B4F25D4ABC5370`.

The updated selection verifies every INF copy's SHA256 and length. Different copies, a missing INF or multiple SYS files still fail. Identical INF copies select the root output file when present, otherwise the first sorted path. All copies and hashes are recorded. No copy is deleted or edited.

The packaging regression runs the actual selection block on Windows PowerShell using temporary fixture files. The test-only commit fails with the original `Expected exactly one new SYS and stamped INF` error. These checks do not build or load a driver.

## Codex: finish the existing candidate without a rebuild

Use the EXISTING worktree:
`D:\Projects\Entropy-ClockFollow-C1-20261015\source`

Branch: `chatgpt/music-clockfollow-c1-20261015`.
Fetch this branch and fast-forward only to the exact packaging commit given in the current handoff, after confirming clean tracked sources. Do not create a worktree or touch the original driver project. No reset, stash, force or deletion of user work.

Read AGENTS.md and this file. Execute from the worktree root:

```powershell
& .\Drivers\EntropyMusicRenderV2\tools\Build-C1.ps1 -ExpectedCommit '<EXACT_PACKAGING_SHA_FROM_HANDOFF>' -PackageOnly
```

Do NOT omit `-PackageOnly`: this continuation must not run CMake, MSBuild, the model executable or any tests. Do not clean the build directory, choose an arbitrary INF manually or remove the original INCOMPLETE marker.

PackageOnly requires the preserved initial candidate and its kernel-build.json, logs and recorded model PASS. It rejects any compiled source/project changes since the reported build and checks the actual SYS/INF against the reported hashes above. Signature status is checked now; if it is not NotSigned the script stops without signing or changing anything. The model binary is only hashed/copied, not executed again. The manifest records binary source `3faccc...` separately from the new packaging commit and marks prior build/model results as recorded, not newly executed.

The existing incomplete candidate and all build outputs are read only. New destination:
`D:\Projects\Entropy-ClockFollow-C1-20261015\candidate\package-retry\ClockFollow-C1-0.3.1.0`

A preexisting new destination is refused. Any failure leaves the new INCOMPLETE marker and original evidence. Successful packaging removes only the new marker; NOT-FOR-INSTALLATION.txt remains.

Report only package outcome, signature status, manifest/source/packaging identity and SYS/INF/model hashes. STOP before signing, catalog generation, PnP/INF operations, loading the driver, reboot, audio playback or any Stage1/2/3/AUX/CINEMA change. No new test/capture task. Installation and the full ClockBridge/Stage1/direct-primary integration are still separate and not authorized by this packaging task.
