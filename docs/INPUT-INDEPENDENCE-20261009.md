# Exact input boundary and remaining coupling

Required topology: Stage1 -> Stage2 -> AudioCore. AudioCore must only receive
the publisher's FP64/192 kHz L/R/C + eight CINEMA lanes. Qt remains IPC and
telemetry only; it neither receives audio buffers nor owns the audio engine.

AudioCore has no direct Stage1 connection. Its only input link is the publisher
downstream endpoint (`Engine/Input/Stage2Ingress.cpp`, `start`/`run`). The
current compatibility receiver sends Query, Attach, Release and frame demand
to that endpoint. DSP Graph modules do not send those transport commands.

The indirect influence is confirmed in the existing Stage2 service, read only:

- `Host/EntropyDspEngine/Core/DspDownstreamLink.cpp`: downstream Attach/Release
  calls `DspAutoLink::finish`; consumer death can stop its runtime.
- `Host/EntropyDspEngine/Core/DspAutoLink.cpp`: `finish` sends Release to its
  upstream Source lease, then stops its runtime.

These files were inspected in the protected reference under
`D:/Projects/3Stage-3/Stage3/integration/dsp-link/source`; they were not edited.
Thus Stage1 influence is via Stage2's session-management implementation,
not a hidden Stage1 connection in Qt, Crossover or another DSP module.

The exposed legacy bus is demand-driven and uses a single leased consumer,
shared fault state and peer heartbeat/death checks. Removing all downstream
requests from AudioCore alone would stop production of its requested blocks;
unregistered reads from reused ring slots do not provide a coherent passive
stream. Neither option meets the required FP64 correctness or live behavior.

Total independence therefore remains BLOCKED for this particular publisher
interface. Certifying it requires a passive publication interface whose writer
continues independently of receiver presence, whose sequence/epoch makes
copied blocks coherent, and whose receiver cannot fault or control the writer.
The audio algorithms need no change, but the current Stage2 transport exposure
cannot be made passive solely by editing AudioCore. No Stage1/Stage2 source or
process is changed by the 0.0.7 routing cleanup. Graceful receiver handoff is
useful evidence, not proof of independence under arbitrary consumer failure.
