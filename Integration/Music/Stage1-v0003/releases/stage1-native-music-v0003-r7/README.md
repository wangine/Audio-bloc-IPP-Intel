# Stage1 v0003-r7 delivery

Locally tested runtime and complete standalone source archives, published after
user approval on 2026-10-12. Extract the runtime ZIP, then run
`Start-Stage1-Music.cmd` inside its folder. The guarded ColdStart/Stop launchers,
diagnostic runner and known-pattern PCM/FP64 audit tools are retained.

See [generation correction and live evidence](../../docs/V0003-R7-GENERATION-RESULT.md),
[runtime manifest](runtime-manifest.json), [source manifest](source-manifest.json)
and [archive hashes](archives.json). Core tests pass 6/6. Real TIDAL pause/play
validation records zero MUSIC underrun/fault/lost/overrun, publisher drops and
clock-late counters over its bounded capture. This does not certify original
TIDAL PCM identity or automatic WASAPI access detection.

Kernel 0.3.0.11 and raw Provider r3 are unchanged. Pair with the existing
[driver/Provider delivery](../../../../Drivers/EntropyMusicRenderV2/0.3.0.11-signed-dev-r3/README.md).
Stage2 MUSIC v0009 remains unchanged. Frozen v0002 and earlier archives remain
available with their original Provider pairs. The runtime carries local endpoint
selections; another machine must select its own endpoints.

The original archive manifests record the modified local tree based on commit
`34556e782e47a67417aadbf7b06e12c330f7da10`. Their immutable hashes bind the exact
tested payloads; publication does not replace that build provenance.
