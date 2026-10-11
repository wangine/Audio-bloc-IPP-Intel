# Stage3 0.1.9-v2 — MUSIC / CINEMA

Source baseline: exact 0.1.8 archive, commit `2f3c45aa71cbf1bc352aece848060dc7523b0649`. DSP, Headroom, physical egress and device algorithms remain unchanged.

Stage1 → Stage2 MUSIC DSP → **MUSIC L/R/C, FP64 planar / 192000 Hz / 1024 frames** → Stage3 Crossover → Matrix → DAC A/B.

Future Decoder → **CINEMA 5.1 (6ch) or 7.1 (8ch), FP64 planar / 192000 Hz / 1024 frames**, through its own passive endpoint → the same Stage3 Crossover. Decoder/SRC implementation is pending. Height lanes are reserved and zero.

Default local pipes append the current Windows SID:

- `Entropy.StagePublish.Music.Dsp.V2.<SID>` — distributed Stage2 v0009 MUSIC3 layout 4, unchanged MUSIC hashes.
- `Entropy.StagePublish.Cinema.Decoder.V2.<SID>` — Cinema layouts 5/6; channel order FL, FR, FC, LFE, SL, SR, then BL, BR for 8ch. Contract hashes are documented in `Transport/StageBusSDK/CinemaStageBusContract.json` in the source archive.

The existing MUSIC/CINEMA control selects the native Crossover program. Inactive sources are drained independently. Missing inactive Cinema does not block Music parameter edits. A request to select an absent source is refused. A saved Cinema profile stays on Cinema and reports WAITING CINEMA when its decoder is absent. Metadata and source chronology remain separate. Independent source identity tokens reset only the corresponding Crossover histories. They never reset the continuous Graph-to-DAC clock on valid source format/generation changes or program selection; no upstream header is fabricated.

Selected gap/nonfinite/overflow/disconnect faults stop processing and fault any engaged physical output. No backlog replay, upstream control or automatic re-ARM. Explicitly selecting an available source after loss uses that source with unavailable old planes zero and the original native fade. For an active source fault, reload the profile or stop/restart this Core; physical output automatically DISARMs on a serious fault, retains the cause, and then accepts an explicit new ARM directly once its source is ready. No automatic re-ARM.

Profiles add `stage3_inputs` version 1 containing `music` and `cinema` local pipe names. Older profiles use defaults; DSP scenes and DAC bindings are preserved. Explicit CLI endpoint overrides take precedence over profile endpoints. Invalid input profiles are rejected before mutation.

## Launch

- `Cold-Start.cmd` starts DISARMED. The existing Stage04 gate remains in force.
- `Live-Test.cmd` grants the same manual live-test authorization as 0.1.8; starts DISARMED and still requires explicit ARM in UI and validated profile/devices.
- `Cold-Start.cmd -Profile "D:\path\private-copy.entropy"` uses an explicit profile. Without it, the existing mother-profile selection behavior is retained.
- `Cold-Start.cmd -SmokeTest` tests packaged Core → IPC → Qt offscreen, with synthetic signal and no source or DAC.
- `Stop-AudioCore.cmd` stops only this package's Core through IPC. It refuses another version's process. Closing/reopening Qt retains Core state.

Another Core at the default endpoint must be stopped explicitly by its own launcher before switching versions. New launchers never stop it automatically. Both 32k/64k variants are Windows **x64**; the number is FIFO capacity, with target 4096 frames.

Core validation flags: `--input-only` blocks physical ARM, permits isolated coexistence, and uses a prepared offline graph if no profile is provided. `--music-endpoint` / `--cinema-endpoint` accept local named pipes only. `--stage2-input` selects this new production input path; `--stage2-endpoint` is a Music endpoint alias. Legacy11 is retained only in old in-process fixtures and is rejected by production input.

## Acceptance limits

See `VALIDATION-MUSIC-CINEMA-v2.json` for measured results and SHA256 manifests. This repair adds real dual-DAC MUSIC listening evidence, high-priority passive input scheduling and a continuous output timeline. See the validation report for the exact listening interval and observed source changes. The original upstream 0.1.9 build was not changed or qualified. Real Cinema and analog alignment remain pending. Original Stage04 offline failure, V07 BLOCKED and any inherited Qt failure are reported explicitly. Author self-review was used as requested; no independent reviewer.

## Fault repair

The 0.1.8-v2 adapter could pause its ordinary-priority receiver long enough to starve ASRC; it now uses the original host approach of MMCSS Pro Audio and a high-resolution wait. The FIFO target/capacity, DSP and egress algorithms are unchanged. Valid source identity changes no longer invalidate the downstream clock. Serious sink faults finish DISARM on the lifecycle owner and preserve their reason; manual ARM needs no extra DISARM.

The input panel displays MAX sample peaks in dBFS measured across every valid received block since this Core started. These are Sample Peak holds, not True Peak or SRC Startup Peak. Instantaneous meters remain separate.
