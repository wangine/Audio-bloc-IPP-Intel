# B1: independent memory renderer, first bounded delivery

Base: 3e985007885c527b5e3bf6580f853b82bf314d59. Implements the previously proposed single-DAC, file-fed stage B, NOT the final live dual-DAC renderer.

The Stage A report establishes numeric equality against its r8brain reference at the Stage1 publication and exact delayed L/R equality at Stage2's publication for the measured interval. It does not establish what the running Stage3 consumed or which downstream operation causes the reported sound. Its recommendation of an internal-input tap remains a separate diagnostic option. B1 is an independent comparison path, not a claim that the report proved all Stage3 components defective.

## Path

Saved Stage2 capture -> unchanged existing AudioGraph/Crossover/OutputMatrix/manual-or-bitperfect Headroom -> unchanged PCM24 arithmetic -> immutable per-DAC PCM files -> newly written WASAPI exclusive event sink.

Prepare is an OFFLINE process. It links the existing control/profile libraries solely to validate and instantiate the same profile. It opens no IPC, source or DAC and starts no AudioScheduler/LiveSinkWorkers. All DSP, fractional delay, PCA, FIR and quantization work finishes before playback. Legacy egress objects may be present transitively in the prepare binary; none are the playback backend.

Play is a DIFFERENT executable. Its target links only Windows system libraries, not entropy_graph, entropy_control_runtime, entropy_egress, ClockSync, SRC, PCM24 or Qt. It loads the already quantized file into RAM before opening one exact endpoint. Its render loop only stages the next bytes, ReleaseBuffers the complete negotiated packet and commits the cursor after success. There is no rate estimator, fractional reader, elastic FIFO, ASRC, driver-rate correction, live producer or second clock. This is valid for an offline finite file, not for indefinitely running independent live sources.

## Invariants

- One DAC at a time. Mapping/channels/container come from the saved validated profile. Never infer lanes from friendly device names.
- Existing render-owner mutex excludes other Stage3/B1 renderers; no automatic process termination.
- No extra gain, limiter, normalization or AUTO. A profile selecting AUTO is refused; MANUAL and BITPERFECT remain unchanged. Offline PCM overrange fails instead of creating a purportedly good clipped file.
- Full source headers are validated for session/epoch/sequence/first_frame consistency. All three source planes are copied without arithmetic. No block-by-block realignment.
- The capture's terminal Gap is retained as a limitation of the supplied prefix; only its complete saved records are replayed. EOS appends one second of explicitly declared zero input to drain normal short DSP tails; it does not claim to drain arbitrary profile delays exceeding this duration.
- Profile, capture, executable and prepared-file SHA256s are checked locally. FNV in the PCM header is only a fast consistency check, not a security primitive.
- Reference quantization uses the original flat TPDF algorithm with an explicit deterministic seed, to make prepared files repeatable. This is not a claim of bit identity with production's independently randomized dither or old ClockSync output.
- Playback offers no arbitrary raw-LR-to-all-outputs bypass: saved band filters and routes remain active, protecting multiway systems. The owner controls amplifier level and explicitly confirms ARM.
- No PCM, full captures, or local endpoint identities are uploaded by B1.

## Verification boundary

Container and Python execution returned ClientError in the authoring session. No attached binary hash, new compilation, portable test, Windows API execution or DAC playback was executed here. Source review and Git readback are distinct from compilation. The local build script must compile, run ONLY the bounded cursor/header test, prepare the supplied capture offline, and validate every prepared-file hash. Stop on any failure; no autonomous repairs.

B1 has NOT completed stages A-internal-input/C-live-ASRC/dual-clock alignment, replaced the installed Core, or demonstrated an audible repair. It is the new independently clocked-by-device output baseline needed before those steps.

## Primary API references

https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiorenderclient-getbuffer
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiorenderclient-releasebuffer
https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition

The existing report remains at validation/pcm-frontier-20261014/FRONTIER-REPORT.md. A clean B1 result would isolate a large portion of the old live delivery path; it would not distinguish every removed component. Persistent roughness in B1 would require looking at the still-common DSP/output hardware, without blaming the already-verified Stage2 publication from this file alone.
