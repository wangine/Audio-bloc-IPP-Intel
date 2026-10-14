# Stage3 LiveV1 — live music renderer candidate

14 October 2026. Branch: `chatgpt/stage3-live-renderer-v1-20261014`.
Baseline for unchanged DSP, profiles, UI and inputs: `3e985007885c527b5e3bf6580f853b82bf314d59`.

## What this delivery is

This is a live `EntropyAudioCore.exe` for the existing separate Qt UI. It consumes the existing Stage2 MUSIC and CINEMA publications. It is NOT the B1 finite-file player, a diagnostic signal, a recording task or a change to Stage1/Stage2. The user can listen to ordinary music with the existing crossover and controls visible.

The previous reports justify preserving Stage1/r8brain and the measured Stage2 L/R path. They do not prove the cause of the audible roughness, nor establish that this replacement cures it. LiveV1 is a candidate for manual listening, not an accepted stable reference.

## Physical output path implemented

Existing Graph (crossover, matrix and selected Headroom mode) -> per-DAC FP64 source queue -> separate preparation worker (WDL FP64 ASRC + original PCM24) -> bounded ready-packet queue -> independent WASAPI render owner -> exact approved endpoint.

- The old physical `DacPipeline`, `EgressClockEngine`, clock estimator/ratio controller, old sinc bank and `WasapiDacSink` are not invoked by `LiveSinkWorkers` anymore. Their sources remain for offline compatibility and some existing build dependencies; this is not a claim that every old symbol was removed from the complete executable.
- Each DAC has a source queue, preparation owner, ready queue, output owner, cancellation event, fault and rate controller. There is no peer-ready barrier, common presentation replan or instruction to wait for the other DAC to start. Failure of one approved domain does not stop the other functioning domain. Shared graph/policy/source failures remain shared.
- A domain admits its first source frame only after its own endpoint has opened and negotiated its format. This prevents pre-open accumulation from overflowing its queue. Once admitted, source epoch and frame continuity are enforced; active queues are never overwritten or reset to hide loss.
- The sink owner performs bounded prepared-PCM copy/submit/accounting. ASRC and PCM24 run on its separate preparation owner, not inside the hardware event's deadline-critical work. A packet remains owned and the source cursor is committed only after successful submission.
- The nominal source is FP64/192 kHz. Resampling is near unity, with Cockos WDL pinned to `2b260b915f8ca7cb7859b9c0b7ba7e48b166f0af`, double audio samples and double sinc coefficients, 128 taps / 1024 phase interpolation. `Wdl.cmake` creates a plainly marked adaptation in the build directory: constant 0.97 cutoff and no rate-dependent ideal-grid switch. This avoids rebuilding the sinc bank while small drift corrections change rate. It is not unmodified upstream WDL.
- No new Rust/Faust toolchain is required. WDL is the native C++ alternative discussed for this architecture, not a port of CamillaDSP or Rubato.
- Rate control follows ALL published source frames not yet represented by PCM successfully submitted to WASAPI, including source queue, ASRC storage and ready packets. It does not steer by repeatedly filling only an intermediate reservoir, nor interpret callback/QPC arrival jitter as DAC oscillator drift. The configured near-unity ratio range is 0.999..1.001 with slow PI control and a 20 ppm/second output-time-based slew limit.
- Source queue capacity is 65536 frames. Unsubmitted-source target is max(8192, 4 * negotiated buffer frames). The ready queue has four preallocated packets; startup waits for three. Capacity is not nominal latency. With a 1024-frame buffer, the 8192-frame target represents 42.67 ms in this part of the chain; this is a configured accounting target, not an end-to-end latency measurement.
- WASAPI Exclusive negotiates the exact saved 192 kHz PCM24 channel/format contract, with no shared-mode or different-format fallback. A documented buffer-alignment retry retains that same endpoint/format. Negotiated packet sizes up to 4096 fit the preallocated buffers; there is no old `4096 target minus sinc guard` rejection rule.
- Overflow, malformed/discontinuous PCM, failed submission, exhausted ready data and device errors are reported rather than concealed by repeating packets, dropping active blocks, fast resynchronizing the phase or changing gain. A fault requires the existing explicit lifecycle action; do not interpret failure as repaired audio.

## Preserved controls and loudspeaker processing

The complete existing `DSP/` tree, PCM24 implementation, input transport, UI sources and saved profile format are unchanged. The saved MUSIC/CINEMA role switch, HP/LP filters, delays, matrix assignments, polarity, output gain and protection settings remain under the original UI control. No full-range route is added to a band-specific speaker. No automatic Headroom mode, reserve, gain or limiter is inserted by this release. Headroom remains whatever the saved profile says; MANUAL/BITEXACT is not changed to AUTO.

The launcher copies the user's current SAVED profile at build time, with its hash. Unsaved UI settings cannot be inferred. Subsequent user saves to the package copy are allowed; they do not change the original Documents profile. Stage1, Stage2, source drivers and physical cabling are not modified.

The user currently listens through one main DAC; the other carries only the unused/acoustically disconnected Scoop route. No extra mapping campaign is required. LiveV1 retains approved endpoint IDs and lane assignments and does not swap A/B based on names or device enumeration order.

**Independent DAC playback is implemented; precise inter-DAC presentation/phase alignment is NOT certified by this first live candidate.** The old software-alignment claim is explicitly false/unavailable. Do not use this release as proof that two independently clocked DACs are acoustically aligned. The current single-DAC listening setup avoids making that unverified claim necessary for this comparison.

## Build/package and manual operation

Local instructions: `RendererLive/CODEX.md`. The isolated script `RendererLive/Build-LiveV1.ps1 -ExpectedCommit <exact handoff SHA>` builds ONLY the new Core in Release/MSVC with the existing IPP SDK, runs the established offline `--probe`, checks the existing Qt UI hash, deploys Qt runtime files and creates a new `live/LiveV1-20261014` package. It does not execute the CI contract tests again locally or create any recording. It refuses existing output folders and leaves `INCOMPLETE.txt` on failure.

The compiler configure step needs network access for the exact pinned WDL source. Request scoped permission when required; do not disable security or change ACLs. The source/SDK/profile access used by B1 required the normal user token, not Administrator. Use the same scoped permission approach for this build. On a compiler/linker/probe/deploy error, report and stop without editing source or replacing dependencies.

After the package succeeds, the OWNER DISARMs the old Stage3, closes its UI and stops that Core with its own `Stop-AudioCore.cmd`. Closing only the UI does not stop the independent Core. Leave Stage1 and Stage2 running as usual. Then start:

`D:\Projects\Entropy-Stage3-Recovery-20261013\live\LiveV1-20261014\Live-Test.cmd`

This opens the existing visible Qt UI, initially DISARMED, with the copied saved profile. Check that the familiar crossover/profile is loaded and select ARM manually. Listen to the ordinary music passages on which the roughness was recognizable, not the B1 diagnostic PCM. No bypass of speaker filters or change of physical wiring is required. Keep upstream settings and the driver buffer unchanged for the first comparison.

The launcher refuses coexistence with another Core/UI, checks binary and launcher hashes, and unsets the optional inherited chain-audit environment variable only for this launched session. It does not silently run a capture. To roll back, DISARM and stop LiveV1 with its own stop command, then restart the preserved R3 package. No previous runtime is overwritten.

## Verification boundaries

The branch's GitHub Actions workflow builds and runs bounded contracts on Ubuntu and Windows/MSVC. On Windows the real new domain workers, new WASAPI implementation, WDL adaptation and original PCM24 are compiled; test playback uses injected fake sinks, never a physical DAC. Queue ordering, packetization independence, commit-after-submit accounting, bounded control, either surviving endpoint and pre-open admission are checked. The strengthened pre-open test reproduced a full 65536-frame queue before any sink opened in commit `41d7fbf3e3a33d8268536790fca105443ba6c3bd`; the subsequent production admission guard addresses that exact case, not an alleged proof of the user's distortion cause.

The PowerShell entry points are syntax-checked separately in Windows CI. Passing these checks does not mean the complete local IPP Graph/Core/Qt package has been built, that all Windows/driver scheduling cases are qualified, or that the audible roughness is gone. The full application link/probe/deploy belongs to the requested local Codex build; the live listening verdict belongs to the user. No physical playback, new Tidal test, loopback capture or profile audit is requested from Codex.
