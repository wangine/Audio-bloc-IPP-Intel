# Exclusive real chain investigation plan

**Goal:** Find and correct only the first demonstrated deviation in live r9 → Stage2 → Stage3 → DAC A/B with the user's listening profile.

**Architecture:** Preserve reference binaries and configuration. Observe passive StagePublish boundaries without another kernel reader; use bounded diagnostic memory/queues for graph and sink buffers, and disk workers outside audio threads. Replay captured inputs offline through the identical profile and algorithms, with state/reset chronology retained.

**Tech Stack:** Native C++20, MSVC Stage3, MinGW Stage2, Python analysis, existing StagePublish V2 and telemetry IPC.

**Spec:** User request in this chat, 2026-10-12: live Exclusive diagnosis at Stage2 input, Crossover outputs, final PCM for each DAC; distinguish genuine stream changes from transient AUX absence; no bypass, gain/filter/buffer modifications or general suites; publish complete tested local references in Git afterwards.

## Constraints and review focus

- Keep real profile, output mapping, crossover and speaker protections active.
- Do not equate invalid/missing source blocks with a new generation unless identity truly changes.
- Count every observed Stage2 reset with the exact old/new header and attach reason; do not infer unobserved events from coarse telemetry.
- Compare tweeter/low-mid signals with their own expected filtered output, never full range.
- Retain capture drops, transport gaps, FIFO state, sink deadlines and ClockSync reset reasons; report missing evidence honestly.
- Handle repeat-loop boundaries distinctly from continuous playback and AUX starvation.
- Publish only after targeted regression and live validation; retain source/executable provenance.

## Steps (execute directly as already authorized)

- [x] Preserve Stage1 config, Stage2 current live state and Stage3 actual listening profile, binary hashes, endpoint mappings.
- [x] Read r9 publisher, Stage2 reset sites and Stage3 epoch handling; capture consecutive passive headers and PCM. Count resets exhaustively from the exact source condition, without adding an executable Stage2 counter. Report this distinction.
- [x] Add observation-only bounded taps at native Crossover output and final sink PCM, including reset/timing/FIFO metadata; build only affected targets separately, without DSP changes.
- [x] Replay the full qualified capture and identical profile offline, retaining epochs, silence, loop boundaries and sink timing. No audible numerical degradation was demonstrated up to DAC submission.
- [x] Correction decision: no audio correction or audio regression added because no causal mechanism was demonstrated in the audible passage. Diagnostic-reader integrity and restoration checks passed (9 targeted checks); original production Core was restored, and user confirmed correct playback. Broader music listening and physical A/B alignment remain open.
- [ ] Package reference sources, complete runtimes, launchers, documentation and evidence; verify hashes, commit and push the architecture branch. Packaging includes a distinct live-qualified observation Core and explicitly identifies later source-only hardening.
