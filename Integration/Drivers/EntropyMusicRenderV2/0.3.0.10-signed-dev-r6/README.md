# Entropy Music Render V2 — signed development package0.3.0.10 / audit r6

This archive preserves the installed MUSIC-only kernel, production/audit Provider
DLLs, permanent PCM/FP64 audit executable, packaged runner, all source/tests,
frozen Stage1 transport dependencies and SHA256 inventories. No private key is
included. Source `1322849`; kernel source `d3ba51e`. Existing approved DEV signing
was used; Microsoft production signing is not claimed.

**Acceptance is partial.** Recorded Exclusive Event raw tests pass, including
345,600,000 frames in30minutes and all12 PAUSE/RUN cases. Polling is unsupported.
Native192k output captures expose inserted MUSIC zero clock blocks in unchanged
Stage1v0002. Valid values compare exactly; output continuity does not pass.
The final audit also rejects the frozen publisher's ambiguous shutdown state.

Read [full results and limitations](../../../../validation/music-render-v2/LOADED-DRIVER-0.3.0.10.md)
and [tool instructions](../../../../Drivers/EntropyMusicRenderV2/audit/TEST-TOOL.md).
`delivery.json` binds installed SYS, archive and package manifest. All25 manifested
archive artifacts were verified; the source ZIP built independently and all12
CTest suites passed. Large captures remain local with hashes in the evidence.

Expanded local package: `release/EntropyMusicRenderV2/0.3.0.10-signed-dev-r6`.
Use its manifested `tools/Run-Live-Acceptance.ps1`, an absolute `-Package`,
`-Repository` and a fresh `-Output`; the default invocation opens no audio.
The runner verifies its own hash, all binaries, installed driver and frozen Core,
and stops only its isolated test processes. Do not install or copy Providers over
the active [Stage1](../../../Music/Stage1) or [Stage2](../../../Music/Stage2) snapshots.
They stay reference deliveries; the future Stage1 fixes require a new version.
