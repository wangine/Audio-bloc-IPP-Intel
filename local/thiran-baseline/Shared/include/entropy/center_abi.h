#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_CENTER_EXTENSION_ID "audio.entropy.center-extractor.v1"
#define ENTROPY_CENTER_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CENTER_V2_EXTENSION_ID "audio.entropy.center-extractor.v2"
#define ENTROPY_CENTER_V2_EXTENSION_VERSION 0x00020000u
#define ENTROPY_CENTER_STATE_SCHEMA 0x43454e54u
#define ENTROPY_CENTER_STATE_VERSION 0x00010000u
#define ENTROPY_CENTER_V2_STATE_VERSION 0x00020000u
#define ENTROPY_CENTER_TELEMETRY_EXTENSION_ID "audio.entropy.center-telemetry.v1"
#define ENTROPY_CENTER_TELEMETRY_EXTENSION_VERSION 0x00010000u

typedef enum EntropyCenterInputSemantic {
    ENTROPY_CENTER_INPUT_LEFT = 1,
    ENTROPY_CENTER_INPUT_RIGHT = 2
} EntropyCenterInputSemantic;

typedef enum EntropyCenterOutputSemantic {
    ENTROPY_CENTER_OUTPUT_LEFT = 1,
    ENTROPY_CENTER_OUTPUT_RIGHT = 2,
    ENTROPY_CENTER_OUTPUT_EXTRACTED_CENTER = 3
} EntropyCenterOutputSemantic;

typedef enum EntropyCenterParameterKind {
    ENTROPY_CENTER_PARAMETER_CONTINUOUS = 0,
    ENTROPY_CENTER_PARAMETER_BOOLEAN = 1,
    ENTROPY_CENTER_PARAMETER_CHOICE = 2
} EntropyCenterParameterKind;

typedef struct EntropyCenterChannelDescriptor {
    uint32_t struct_size;
    uint32_t slot_index;
    uint32_t semantic_id;
    uint32_t reserved_u32[5];
    char identifier[48];
    char label[64];
} EntropyCenterChannelDescriptor;

typedef struct EntropyCenterParameterDescriptor {
    uint32_t struct_size;
    uint32_t index;
    uint32_t kind;
    uint32_t choice_count;
    double minimum_value;
    double maximum_value;
    double default_value;
    uint64_t reserved[2];
    char identifier[48];
    char label[64];
} EntropyCenterParameterDescriptor;

typedef struct EntropyCenterApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_input_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_input_descriptor)(uint32_t index, EntropyCenterChannelDescriptor* descriptor);
    EntropyResult (ENTROPY_ABI_CALL *get_output_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_output_descriptor)(uint32_t index, EntropyCenterChannelDescriptor* descriptor);
    EntropyResult (ENTROPY_ABI_CALL *get_parameter_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_parameter_descriptor)(uint32_t index, EntropyCenterParameterDescriptor* descriptor);
    EntropyResult (ENTROPY_ABI_CALL *get_parameter)(EntropyModuleHandle, const char* identifier, double* value);
    EntropyResult (ENTROPY_ABI_CALL *set_parameter)(EntropyModuleHandle, const char* identifier, double value);
    EntropyResult (ENTROPY_ABI_CALL *export_state)(EntropyModuleHandle, void* bytes, uint32_t* in_out_byte_count);
    EntropyResult (ENTROPY_ABI_CALL *import_state)(EntropyModuleHandle, const void* bytes, uint32_t byte_count);
    EntropyResult (ENTROPY_ABI_CALL *get_fft_backend_name)(EntropyModuleHandle, char* utf8, uint32_t* in_out_byte_count);
    uint64_t reserved[7];
} EntropyCenterApiV1;

typedef struct EntropyCenterTelemetry {
    uint32_t struct_size;
    uint32_t latency_samples;
    uint32_t block_size;
    uint32_t reserved_u32;
    double center_peak_db;
    double center_rms_db;
    double mid_activity;
    double level_activity;
    double phase_activity;
    double coherence_activity;
    double stereo_correlation;
    double detector_confidence;
    double dsp_load_percent;
    double process_time_us;
    uint64_t reserved[4];
} EntropyCenterTelemetry;

typedef struct EntropyCenterTelemetryApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyCenterTelemetry*);
    uint64_t reserved[7];
} EntropyCenterTelemetryApiV1;

#ifdef __cplusplus
}
#endif
