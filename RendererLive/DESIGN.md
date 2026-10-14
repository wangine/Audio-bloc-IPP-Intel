# Stage3 Live Renderer V1

Owner-authorized implementation, 14 October 2026. Base: 3e985007885c527b5e3bf6580f853b82bf314d59. This is a live candidate, not another finite-file B1 player. Keep the existing Qt UI, source selection, Crossover, OutputMatrix, manual/bitperfect Headroom and PCM24. No Stage1/Stage2 changes or new diagnostic acquisition is requested.

## Signal ownership

Graph (existing owner) -> selected FP64 planes -> one preallocated SPSC input queue per DAC -> dedicated preparation worker (ASRC and original PCM24) -> four preallocated ready packets -> independent WASAPI owner (copy/submit only).

No new live code calls DacPipeline, EgressClockEngine, EgressClockEstimator, EgressRatioController, or WasapiDacSink. Some old libraries remain linked for existing offline/profile/ABI support, not in the new physical path. No shared presentation gate or peer-ready barrier. A failed secondary endpoint cannot fault a healthy primary. Exact saved endpoint and lane mapping are retained; no name/order fallback.

The C++ WDL resampler is pinned to Cockos WDL commit 2b260b915f8ca7cb7859b9c0b7ba7e48b166f0af. Both samples and coefficients are double. This selects the already-discussed native C++ alternative instead of installing Rust/Faust on the user's workstation. A documented generated-source adaptation fixes the sinc cutoff and disables rate-dependent ideal-table selection for the limited drift range. It leaves Cockos' interpolation, history and buffering algorithm intact. A constant bank is built before playback, avoiding table reconstruction in any active worker. No linear audio interpolator, slip/drop/duplicate mode or old sinc table is used.

## Rate/queue accounting

The controlled level is source frames published minus source frames represented by successfully submitted PCM. This includes the elastic input queue, WDL input/history accounting and every READY packet; it excludes the packet whose ownership has already passed to WASAPI. The endpoint buffer is separately declared and fixed after negotiation. Moving frames between internal queues cannot change this end-to-end unsubmitted count. The controller samples that count once per successful submission, averages over source-independent output-frame time, and changes the near-unity input/output ratio slowly. QPC callback arrival intervals are not used as oscillator frequency measurements.

Rate control is a bounded slow PI loop; ratios remain in [0.999,1.001]. Output samples are generated at the continuously retained WDL phase. Changes in packet size do not reset filter history. Capacity is 65536 source frames; target is an explicit 8192 frames (or larger for a large negotiated quantum), not a latency inferred from an intermediate reservoir. Four prepared output packets isolate the sink deadline from ASRC/quantization work. Neither capacity nor control gain is automatically increased to hide an error.

## Lifecycle and limits

All memory, ASRC tables and PCM24 state are prepared before PCM publication to each session. The sink negotiates its quantum before its preparation worker starts. Start waits only for that domain's prefill and ready packets; DISARM cancels that wait. Existing group ARM UI remains a request to start configured domains independently. No sound-producing operation happens during build or --probe.

Input starvation is not fabricated input: no partial WDL flush, replay of last data, or timeline jump. A depleted active queue fails the affected domain and retains the reason. Source loss is exposed to the existing waiting/resume lifecycle; retirement/reset occurs only after workers stop. No automatic gain, limiter, normalization, raw full-range bypass, changes to speaker filters or parameter migration.

This initial live candidate does not claim physical/sample-accurate A/B phase synchronization. It retains a shared source-frame origin and publishes per-domain positions; two independent start times must not be called verified alignment. The user currently listens through one DAC; the other has only an unconnected Scoop role. The primary must work independently, without removing the stored secondary configuration.

## Verification boundaries

Focused portable queue/rate/resampler checks and Windows compilation are used to catch implementation errors, not to claim audible repair. The complete IPP/Qt/Core Windows build remains a local Codex task. No DAC playback is automated. The owner compares normal live music with existing safe crossover/mapping and the UI visible. A new isolated package and exact commit are required; R1/R2/R3/B1 remain untouched.
