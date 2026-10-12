#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_INPUT_MATRIX_EXTENSION_ID "audio.entropy.input-matrix.v1"
#define ENTROPY_INPUT_MATRIX_EXTENSION_VERSION 0x00010001u

typedef enum EntropyInputMatrixInput {
    ENTROPY_INPUT_MATRIX_TIDAL_L = 0,
    ENTROPY_INPUT_MATRIX_TIDAL_R = 1,
    ENTROPY_INPUT_MATRIX_AUX_L = 2,
    ENTROPY_INPUT_MATRIX_AUX_R = 3,
    ENTROPY_INPUT_MATRIX_CINEMA_L = 4,
    ENTROPY_INPUT_MATRIX_CINEMA_R = 5,
    ENTROPY_INPUT_MATRIX_CINEMA_C = 6,
    ENTROPY_INPUT_MATRIX_CINEMA_LFE = 7,
    ENTROPY_INPUT_MATRIX_CINEMA_SL = 8,
    ENTROPY_INPUT_MATRIX_CINEMA_SR = 9,
    ENTROPY_INPUT_MATRIX_CINEMA_BL = 10,
    ENTROPY_INPUT_MATRIX_CINEMA_BR = 11,
    ENTROPY_INPUT_MATRIX_INPUT_COUNT = 12
} EntropyInputMatrixInput;

typedef enum EntropyInputMatrixOutput {
    ENTROPY_INPUT_MATRIX_MUSIC_L = 0,
    ENTROPY_INPUT_MATRIX_MUSIC_R = 1,
    ENTROPY_INPUT_MATRIX_AUX_OUT_L = 2,
    ENTROPY_INPUT_MATRIX_AUX_OUT_R = 3,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_L = 4,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_R = 5,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_C = 6,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_LFE = 7,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_SL = 8,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_SR = 9,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_BL = 10,
    ENTROPY_INPUT_MATRIX_OUT_CINEMA_BR = 11,
    ENTROPY_INPUT_MATRIX_OUTPUT_COUNT = 12
} EntropyInputMatrixOutput;

typedef enum EntropyInputMatrixSource {
    ENTROPY_INPUT_MATRIX_SOURCE_TIDAL = 0,
    ENTROPY_INPUT_MATRIX_SOURCE_AUX = 1
} EntropyInputMatrixSource;

typedef enum EntropyInputMatrixSourceMask {
    ENTROPY_INPUT_MATRIX_SOURCE_MASK_NONE = 0,
    ENTROPY_INPUT_MATRIX_SOURCE_MASK_TIDAL = 1u << 0,
    ENTROPY_INPUT_MATRIX_SOURCE_MASK_AUX = 1u << 1,
    ENTROPY_INPUT_MATRIX_SOURCE_MASK_CINEMA = 1u << 2
} EntropyInputMatrixSourceMask;

typedef struct EntropyInputMatrixChannelDescriptor {
    uint32_t struct_size;
    uint32_t slot_index;
    uint32_t semantic_id;
    uint32_t reserved_u32;
    char identifier[48];
    char label[64];
    uint64_t reserved[4];
} EntropyInputMatrixChannelDescriptor;

typedef struct EntropyInputMatrixState {
    uint32_t struct_size;
    uint32_t mute_tidal;
    uint32_t mute_aux;
    uint32_t mute_cinema;
    uint64_t reserved[4];
} EntropyInputMatrixState;

typedef struct EntropyInputMatrixTelemetry {
    uint32_t struct_size;
    uint32_t active_source_mask;
    uint32_t music_summing_active; /* deprecated: always 0 since v1.1 */
    uint32_t overload_mask;
    uint32_t host_engine_rate;
    uint32_t mute_ramp_samples;
    double music_peak_left;
    double music_peak_right;
    double tidal_contribution_gain;
    double aux_contribution_gain;
    uint64_t processed_frames;
    uint64_t reserved[3];
} EntropyInputMatrixTelemetry;

typedef struct EntropyInputMatrixApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_input_count)(uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *get_input_descriptor)(uint32_t, EntropyInputMatrixChannelDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *get_output_count)(uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *get_output_descriptor)(uint32_t, EntropyInputMatrixChannelDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *set_state)(EntropyModuleHandle, const EntropyInputMatrixState*);
    EntropyResult (ENTROPY_ABI_CALL *get_state)(EntropyModuleHandle, EntropyInputMatrixState*);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyInputMatrixTelemetry*);
    uint64_t reserved[8];
} EntropyInputMatrixApiV1;

#ifdef __cplusplus
}
#endif
