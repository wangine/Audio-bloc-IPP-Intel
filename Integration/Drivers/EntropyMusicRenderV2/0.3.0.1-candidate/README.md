# Entropy Music Render V2 — unsigned candidate0.3.0.1

Kernel source/build revision: `0e93fb35b220f2f8af2c2668659ea6f9f979b833`.
11/11 offline suites and9/9 package checks PASS. Archive CRC and every archived
file were compared with the manifested local delivery. **NOT INSTALLED; live
acceptance NOT_RUN; SYS/CAT NotSigned.**

`EntropyMusicRenderV2-0.3.0.1-candidate-win-x64.zip` contains SYS/INF/unsigned CAT,
production and audit Provider DLLs, WASAPI audit executable, packaged qualification
runner, source.zip, frozen Provider ABI, licenses, hashes and provenance. See
`delivery.json`, `manifest.json` and `SHA256SUMS.txt` for exact bytes/hashes.

Local expanded version: `release/EntropyMusicRenderV2/0.3.0.1-candidate`.
Sources and review: [Drivers/EntropyMusicRenderV2](../../../../Drivers/EntropyMusicRenderV2).
The new driver is separate from the two installed legacy drivers.

Architecture references remain [Stage1 v0002](../../../Music/Stage1) and
[Stage2 MUSIC v0009](../../../Music/Stage2); neither snapshot changed. Stage3
continues passive MUSIC L/R/C and separate future Cinema ingress.

After explicit installation approval, create a separate signed package with a
fresh matching manifest and register only `ROOT\EntropyMusicRenderV2`; do not
change system default, existing drivers or active Stage1/2/3 providers. Run the
packaged `tools/Run-Qualification.ps1 -Repository <checkout> -RunLive` only on that
approved installed version. Package/installed SYS/frozen Core hashes are checked
before audio. A source-model PASS is not live PCM or PortCls qualification.

Position-only allocations are refused. Exclusive Event and Polling must each
prove the actual OS packet-commit path with full first-to-last byte coverage.
The30-minute stress, lifecycle/partial EOS and real TIDAL diagnostics follow
loaded-driver approval. AUX correction remains deferred to the next Stage1 version.
