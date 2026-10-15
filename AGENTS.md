# MUSIC ClockFollow C1 — current continuation is PACKAGE-ONLY

Use the existing worktree D:\Projects\Entropy-ClockFollow-C1-20261015\source on chatgpt/music-clockfollow-c1-20261015. The owner reported successful model contracts and kernel build from 3faccc678242f2815cc61eeb8f49a2f05c0c85b5, followed by a packaging failure because two identical stamped INF copies were found.

Read Drivers/EntropyMusicRenderV2/C1-PACKAGE-RESUME.md. Fetch and fast-forward to the exact supplied packaging SHA only, preserving clean tracked sources and all user work. Do not recreate/switch other checkouts or merge this branch into Stage3.

Run Drivers/EntropyMusicRenderV2/tools/Build-C1.ps1 -ExpectedCommit <handoff SHA> -PackageOnly. Preserve the initial incomplete candidate. The script verifies all duplicate INF contents, the reported SYS/INF hashes, previous logs and signature state, then creates a new package. It does NOT compile or execute the model again. Binary source and packaging-source revisions are recorded separately.

No source or profile edits, CMake/MSBuild, model rerun, CTest, historical acceptance suites, captures or diagnostic loops. No deleting duplicate INF files, arbitrary first-file workaround, manual INCOMPLETE removal or bypass of hash/version/signature checks. On error report and STOP.

No signing, catalog generation, certificate import, test-signing/Secure Boot changes, Administrator, PnP/INF installation, SYS replacement, endpoint/service restart, reboot, live driver IOCTL, DAC/ARM access, Stage1/2/3/AUX/CINEMA changes, push or unrelated merge. Request only scoped normal-user file/network permission where required. Existing installed .11 and listening configuration remain untouched. Report package result and hashes, not an audio-quality verdict.
