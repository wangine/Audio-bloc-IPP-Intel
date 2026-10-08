#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_LOUDNESS_DISPLAY_EXTENSION_ID "audio.entropy.loudness.display.v1"
#define ENTROPY_LOUDNESS_DISPLAY_EXTENSION_VERSION 0x00010000u
#define ENTROPY_LOUDNESS_DISPLAY_POINTS 320u
typedef struct EntropyLoudnessDisplay {
    uint32_t struct_size, valid, stopped, fault;
    uint64_t applied_sequence, control_epoch, processed_frames;
    double sample_rate, fit_error_db, static_attenuation_db;
    double bass_comp_db, mic_peak_spl, air_effective_mid, air_effective_high;
    double listening_phon, reference_phon, adaptive_amount, max_comp_db;
    double marker_hz[3], marker_db[3], marker_shape[3];
    double target_db[320], realised_db[320];
} EntropyLoudnessDisplay;
typedef struct EntropyLoudnessDisplayApiV1 {
    uint32_t struct_size, extension_version;
    EntropyResult (ENTROPY_ABI_CALL *read_display)(EntropyModuleHandle,EntropyLoudnessDisplay*);
} EntropyLoudnessDisplayApiV1;
#ifdef __cplusplus
}
#endif
