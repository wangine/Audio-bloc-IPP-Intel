WDL CINEMA-only SRC dependency

Upstream: https://github.com/justinfrankel/WDL
Pinned commit: d30c30b356b2b7fb1654def8dbda90066f43061a
Files retain the Cockos license notices.

Local alteration: resample.cpp BuildLowPass calls
m_filter_coeffs.ResizeOK(alignedsize,false) instead of ResizeOK(alignedsize).
This retains previously reserved coefficient storage when changing input rate.
No filter arithmetic, coefficients, phase calculation or sinc mode was changed.
CinemaWdlSrc reserves the largest possible table for sinc oversize=32
(64 ideal phases), then resets state before accepting any real audio.
The dependency is compiled only into the Stage 1 CINEMA adapter.
MUSIC/AUX continue to process through the unchanged native r8brain DLL.

Build configuration: WDL_RESAMPLE_FULL_SINC_PRECISION for all translation units
using WDL declarations; both audio and coefficient types statically asserted double.
Final default ENTROPY_CINEMA_SINC_TAPS=128, explicitly selected by the user
after the initial 64-tap tests; only 64 and 128 are accepted.
WDL GetCurrentLatency() returns seconds, not sample frames. CINEMA publishes
ceil(seconds * 192000) output samples and displays samples / milliseconds.
