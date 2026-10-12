#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_OUTPUT_MATRIX_EXTENSION_ID "audio.entropy.output-matrix.v1"
#define ENTROPY_OUTPUT_MATRIX_EXTENSION_VERSION 0x00010000u
#define ENTROPY_OUTPUT_MATRIX_CHANNELS 16u

typedef struct EntropyOutputMatrixChannelDescriptor {
    uint32_t struct_size;
    uint32_t slot_index;
    char identifier[48];
    char label[64];
    uint64_t reserved[4];
} EntropyOutputMatrixChannelDescriptor;

/* Host compiles persistent role->DAC/channel assignments into this RT snapshot.
   Each source mask bit selects a destination lane. No device identity crosses
   the audio ABI. A destination must also be enabled to emit audio. */
typedef struct EntropyOutputMatrixState {
    uint32_t struct_size;
    uint32_t generation;
    uint32_t active_source_mask;
    uint32_t enabled_destination_mask;
    uint32_t source_destination_mask[ENTROPY_OUTPUT_MATRIX_CHANNELS];
    uint64_t reserved[4];
} EntropyOutputMatrixState;

typedef struct EntropyOutputMatrixTelemetry {
    uint32_t struct_size;
    uint32_t applied_generation;
    uint32_t active_destination_mask;
    uint32_t summing_destination_mask;
    uint32_t orphaned_source_mask;
    uint32_t route_count;
    uint32_t host_engine_rate;
    uint32_t reserved_u32;
    double destination_peak[ENTROPY_OUTPUT_MATRIX_CHANNELS];
    uint64_t processed_frames;
    uint64_t reserved[4];
} EntropyOutputMatrixTelemetry;

typedef struct EntropyOutputMatrixApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_input_descriptor)(uint32_t, EntropyOutputMatrixChannelDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *get_output_descriptor)(uint32_t, EntropyOutputMatrixChannelDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *set_state)(EntropyModuleHandle, const EntropyOutputMatrixState*);
    EntropyResult (ENTROPY_ABI_CALL *get_state)(EntropyModuleHandle, EntropyOutputMatrixState*);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyOutputMatrixTelemetry*);
    uint64_t reserved[8];
} EntropyOutputMatrixApiV1;

#ifdef __cplusplus
}
#endif
