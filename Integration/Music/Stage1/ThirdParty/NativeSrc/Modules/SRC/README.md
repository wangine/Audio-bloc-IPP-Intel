# Entropy Sample Rate Converter FP64 — Stage 1 trusted reference
Single engine: r8brain-reference, linear phase, 2 percent transition and 217 dB attenuation settings, unchanged from accepted reference.
Stage1 conversion uses planar FP64 to 192000 Hz, coherent 1–16 channels and independent instances for MUSIC/AUX/CINEMA clock domains. SRC input256 and source block1024 remain fixed. Native 192k bypass copies FP64 bits exactly. Engine1 is rejected; legacy enum/layout constants remain ABI-compatible.
Build output is build/Modules/SRC/EntropySampleRateConverter.dll relative to native-src, with automatic copies to shared source folders disabled. No SOXR implementation or link dependency. Upstream source copyright/licenses are retained with r8brain.
