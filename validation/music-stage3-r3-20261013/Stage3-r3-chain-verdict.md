# Stage 3 r3 chain verdict — 2026-10-13

## Scope

This is a read-only verification of the installed local reference
`0.1.9-v2-r3` using the user's active Stage 3 profile and one 34.3-second live
capture. No audio source, Stage 1/2 processing, Stage 3 DSP, clock, FIFO, gain,
profile, or DAC assignment was changed. This does not qualify other profiles,
sources, or longer playback.

The captured profile had **L way 1 solo**. That path was configured with an
80.1 Hz high-pass and a 1692.6 Hz low-pass, so it is intentionally band-limited
and is not a full-range listening reference.

## Evidence

- The graph recorded 6,435 consecutive 1,024-frame blocks at 192 kHz: zero
  graph-frame gaps and zero crossover input-token transitions.
- All propagated source metadata remained constant: MUSIC generation 505,
  AUX/CINEMA generation 0, MUSIC session revision 1, validity mask 1,
  discontinuity mask 0, and source rate 44.1 kHz. The Stage 2 code's
  generation/revision reset condition therefore had **zero triggers** in this
  interval. This is derived from published headers; the live Stage 2 reset
  function itself has no separate per-reason counter in this capture.
- Offline replay through the same Stage 3 profile matched the captured
  crossover, matrix, and headroom output after the first 1,000 replay blocks
  (10.67 seconds, needed because the recording began mid-stream). Maximum
  absolute differences were 5.83e-13, 5.83e-13, and 4.63e-13 respectively;
  all other channels were bit-identical in that steady interval.
- The independent ClockSync reference checked 5,742,592 sample points per DAC
  at stride 8: zero failures. Maximum error was 1.33e-10 for DAC A and zero for
  DAC B.
- PCM24 output stayed within the existing TPDF quantizer bound of 1.5 LSB for
  all checked audible samples after accounting for the intentional startup mute
  prefix (192 frames on DAC A and 571 on DAC B). There were no interior silent
  blocks. The live trace also recorded zero DAC frame gaps, clock resets,
  resyncs, deadline misses, or rejected submissions; minimum FIFO depth was
  4,321 frames.
- Only DAC A channel 1 carried signal in this capture; the remaining output
  channels were silent as configured by the user's solo routing.

## Verdict

No Stage 3 processing or egress defect was demonstrated after the Stage 3 input
boundary in this interval. The earliest boundary that remains unverified is the
Stage 3 input, which carries Stage 2's MUSIC L/R/C output: the capture has no
independent Stage 2 output reference to compare against. Do not create an r4
audio change from this evidence alone. The existing r3 live command remains the
versioned test entry point; evaluate perceived full-range sound with the full
profile, not the L way 1 solo path.

## Reproduction

Run the packaged manual live test:

```cmd
D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.9-v2-r3\EntropyAudioCore-0.1.9-v2-r3-64k-win-x64\Live-Test.cmd
```

The package requires the user's validated profile and explicit ARM. Start
Stage 1 and Stage 2 independently if they are not already running.
