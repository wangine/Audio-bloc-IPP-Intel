# LiveV1.1 — source-independent MUSIC/CINEMA selection

15 October 2026, Europe/Bucharest. Version reported by Core: `0.2.1-live-v1.1`.
Branch remains `chatgpt/stage3-live-renderer-v1-20261014`.
Baseline: `07bf7582f0ba3ad75ea36fec4d8ccb3a55269d67`, whose audio executable was built from `8e053cc2f34ccf5abd7ed8ca364d3bc956d218b7`.

## Why this revision

The owner reports that LiveV1 removed the persistent audible roughness. Preserve that datapath. The user deliberately placed the frequency cursor over the REW tone's spectral component; the earlier explanation of the cursor missed that intended comparison. No new tone test is requested.

The reproducible control defect is in `ProgramSelection::request`: the old code rejects selecting MUSIC or CINEMA solely when its ingress state is not Receiving. In addition, the old runtime changes source demand before committing the Crossover command; a live transition can require both independent sources. A missing source then prevents Graph processing and therefore the DSP command's real-block acknowledgement. This revision separates command acceptance from source availability rather than repeatedly retrying the UI command or manufacturing silent input blocks.

The regression at `9ab5894bf1618f5400733f770ad4b8cf7645976c` compiled on Windows/MSVC and failed with `CINEMA selection was rejected solely because its publisher is absent`. It uses real, unstarted ProgramIngress objects and does not open pipes or DACs. The final branch additionally tests the exact transaction helper used by CoreRuntime with fake owners: gate-close/join/commit order, exclusive demand, failure handling and preservation/cancellation of ARM intent. This does not claim a full physical runtime qualification.

## New behavior

A MUSIC/CINEMA command validates the requested program without requiring a publisher. For an actual change, the lifecycle closes output publication, joins the Graph, stops the old output session and temporarily marks both inputs inactive during control preparation. It commits the existing saved Crossover scene, selects exactly one required input domain, and clears the previous scene's histories with the existing quiescent reset. These actions occur only at a joined control boundary, never while an audio owner is reading a ring.

The Graph then resumes waiting for real PCM from the newly selected source. No empty source is synthesized, old source is replayed, or source/driver process is restarted. The UI's program uses the committed parameter and therefore can show CINEMA even before any CINEMA PCM exists. DSP audio-applied acknowledgement remains pending until real samples are processed; command acceptance is not mislabeled as audible output.

If playback was already ARMed and the request was not superseded by DISARM, the existing ARM intent is retained in MUTE/WAITING INPUT. The existing source-wait service reopens output only after actual source data, settled original DSP controls and existing format/profile checks. If the user was DISARMED, selecting a program does not ARM playback. A concurrent DISARM cancels future automatic resumption through the existing generation check.

**Tradeoff:** an actual program change now has an explicit muted handover and prefill, even when both sources are available. It is not an uninterrupted two-source crossfade. The already-running steady-state renderer, ASRC, gain, buffers and PCM remain unchanged. This avoids a dual-source wait and stale old-scene histories. Muting individual channels uses the unchanged original DSP control; no general mute fix is claimed.

## Per-domain dropout reporting, not a claimed cure

The lifecycle writes one `LIVE_DOMAIN_FAULT` JSON line to the normal launcher's Core stderr log when a retained domain fault is first observed. It includes the existing reason, driver result, buffer/queue and preparation timing counters. It runs outside the audio threads, creates no recording, and does not change timeouts, buffer sizes, automatic recovery or fault handling. This is for the next naturally occurring incident, without another manual capture task.

This revision does NOT claim to cure the occasional independent DAC stops or the perceived lack of lower-mid richness. No new PCM or physical incident data proves those causes. ASRC ratio law, WDL bank, FIFO dimensions, PCM24, Crossover, Matrix, Headroom modes/levels, processing flags, transport and UI sources are unchanged from LiveV1. Inter-DAC phase alignment remains uncertified. The single-main-DAC listening configuration is preserved without another mapping task.

## Local compile and live use

Follow `RendererLive/CODEX.md`, in the EXISTING worktree:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source`

The same `Build-LiveV1.ps1` entry point now builds/packages LiveV1.1 at the exact final handoff SHA. The incremental Core build cache is reused, UI is reused by its verified hash, and only the established offline `--probe` is run locally. CI contracts are NOT repeated by Codex.

Default profile is the SAVED profile from the previous live package:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1-20261014\profile\listening.entropy`

The user saves current settings there before packaging; unsaved UI edits are not inferable. The script reads/copies this file exactly and does not change the original, its filters/gains or assignments. No fallback to an older Documents profile. An alternate path requires an explicit `-Profile` argument from the owner.

New output, never overwriting the previous package:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1.1-20261015`

After successful packaging, the owner DISARMs/closes the previous UI, stops that Core with its own Stop-AudioCore.cmd and launches the new Live-Test.cmd. Stage1 and Stage2 stay as they are. The usual Qt interface opens DISARMED with the saved crossover. Listen to ordinary music. Selecting CINEMA with no publisher should select it on the first command and wait silently; selecting MUSIC again should not need repeated clicks. No new tones, loopbacks, captures or high-level ultrasonic playback are required.

## Verification boundary

GitHub Actions compile/run the selector and transaction contracts on Windows and retain the original LiveV1 renderer checks on Windows/Linux. These tests use no physical endpoints. Complete IPP/Core linking and Qt deployment are still the local build, and correct behavior on the user's devices remains a live observation. No statement here qualifies the newly built binary before that build or an audible improvement before listening.
