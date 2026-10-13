# Entropy Dynamic Recovery v0.1

Standalone x64 C ABI DLL for Stage 2 MUSIC v0010's internal planar FP64 L/R slot. The published Stage 2 bus remains 2 inputs → 3 outputs. No SRC, look-ahead, limiter, clipping, saturation, EQ or stereo decorrelation is performed here.

## Build

Run `tools/Build-DynamicRecovery.cmd` from the candidate repository. It uses local MSVC x64 and CMake/Ninja, with `/fp:precise`, and runs `DynamicRecoveryContract`. The release source is checked in at `Dsp/EntropyDynamicRecovery.dsp`; the generated C++ is checked in at `Generated/EntropyDynamicRecoveryFaust.cpp`. To regenerate it with Faust 2.88.0:

```powershell
faust -lang cpp -double -scal -cn EntropyDynamicRecoveryFaust -o Generated/EntropyDynamicRecoveryFaust.cpp Dsp/EntropyDynamicRecovery.dsp
```

The wrapper defines `FAUSTFLOAT` as `double` and compiles the generated class directly. It uses no `libfaust` runtime or Faust standard library imports.

## Contract

The DLL exports `entropy_module_get_api` for `ENTROPY_MODULE_ABI_VERSION=0x00010000`. Its descriptor reports two inputs, two outputs, FP64, real-time processing and zero samples of transport latency. Audio is planar IEEE-754 `double`, with one linked gain applied to L/R. `prepare` accepts 2→2, sample rates 8–192 kHz and a caller-specified maximum block size up to 65536 frames. The Stage 2 MUSIC host uses 192 kHz and 1024 frames. Active processing requires distinct input and output planes; bypass supports exact same-plane in-place use. Cross-channel overlap is rejected.

Configuration is the versioned POD `EntropyDynamicRecoveryConfigV1` passed through `set_config` with `ENTROPY_DYNAMIC_RECOVERY_SCHEMA_ID` and schema version 1. `enabled` defaults to false; expansion also defaults to false. The config must be submitted by the worker that owns the module, between blocks. There is no internal cross-thread mutable pointer. `enabled=0`, `wet_mix=0`, and zero transient strength with expansion disabled take the bit-exact copy path and never call Faust `compute`. Bypass preserves FP64 bit patterns, including signed zero, subnormals and NaN payloads. Active processing rejects nonfinite input before modifying DSP state or output. A discontinuity flag or explicit `reset` clears all envelopes; ordinary blocks preserve history.

The active detector uses the linked maximum absolute sample, 0.3/15 ms fast and 20/180 ms slow envelopes, a −55 dBFS activity gate, bounded transient and optional upward expansion gain, then a 0.5/20 ms gain envelope. The output may exceed 1.0 FP64. Parameter defaults and ranges are in the ABI header and the attached project plan. The processing path has no allocation, lock, logging or filesystem access.

## Current verification

`DynamicRecoveryContract` exercises ABI/prepare/latency, bit-exact default and wet-zero bypass, a stereo-linked transient burst, stationary release, optional upward expansion, invalid parameters, oversized frames, active in-place rejection and nonfinite input containment. Physical listening and downstream Headroom verification remain separate candidate-runtime checks.
