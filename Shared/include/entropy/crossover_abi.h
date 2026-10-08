#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_CROSSOVER_EXTENSION_ID "audio.entropy.crossover.v1"
#define ENTROPY_CROSSOVER_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_STATE_SCHEMA 0x43524f53u
#define ENTROPY_CROSSOVER_STATE_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_SCHEMA_EXTENSION_ID "audio.entropy.crossover-schema.v1"
#define ENTROPY_CROSSOVER_SCHEMA_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_TELEMETRY_EXTENSION_ID "audio.entropy.crossover-telemetry.v1"
#define ENTROPY_CROSSOVER_TELEMETRY_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_GESTURE_EXTENSION_ID "audio.entropy.crossover-control-gestures.v1"
#define ENTROPY_CROSSOVER_GESTURE_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_VIEW_TELEMETRY_EXTENSION_ID "audio.entropy.crossover-view-telemetry.v1"
#define ENTROPY_CROSSOVER_VIEW_TELEMETRY_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_PROTECTION_EXTENSION_ID "audio.entropy.crossover-protection.v1"
#define ENTROPY_CROSSOVER_PROTECTION_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_ID "audio.entropy.crossover-control-status.v1"
#define ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CROSSOVER_RESPONSE_POINTS 256u
#define ENTROPY_CROSSOVER_SPECTRUM_POINTS 256u

typedef enum EntropyCrossoverProgram {
    ENTROPY_CROSSOVER_PROGRAM_MUSIC = 0,
    ENTROPY_CROSSOVER_PROGRAM_CINEMA = 1
} EntropyCrossoverProgram;

typedef enum EntropyCrossoverInputSemantic {
    ENTROPY_CROSSOVER_INPUT_MUSIC_L = 1,
    ENTROPY_CROSSOVER_INPUT_MUSIC_R = 2,
    ENTROPY_CROSSOVER_INPUT_MUSIC_C = 3,
    ENTROPY_CROSSOVER_INPUT_CINEMA_L = 4,
    ENTROPY_CROSSOVER_INPUT_CINEMA_R = 5,
    ENTROPY_CROSSOVER_INPUT_CINEMA_C = 6,
    ENTROPY_CROSSOVER_INPUT_CINEMA_LFE = 7,
    ENTROPY_CROSSOVER_INPUT_CINEMA_SL = 8,
    ENTROPY_CROSSOVER_INPUT_CINEMA_SR = 9,
    ENTROPY_CROSSOVER_INPUT_CINEMA_BL = 10,
    ENTROPY_CROSSOVER_INPUT_CINEMA_BR = 11,
    ENTROPY_CROSSOVER_INPUT_CINEMA_TSL = 12,
    ENTROPY_CROSSOVER_INPUT_CINEMA_TSR = 13
} EntropyCrossoverInputSemantic;

typedef enum EntropyCrossoverOutputSemantic {
    ENTROPY_CROSSOVER_OUTPUT_OFF = 0,
    ENTROPY_CROSSOVER_OUTPUT_LEFT = 1,
    ENTROPY_CROSSOVER_OUTPUT_RIGHT = 2,
    ENTROPY_CROSSOVER_OUTPUT_CENTER = 3,
    ENTROPY_CROSSOVER_OUTPUT_SUB = 4,
    ENTROPY_CROSSOVER_OUTPUT_SCOOP = 5,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_SL = 6,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_SR = 7,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_BL = 8,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_BR = 9,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_TSL = 10,
    ENTROPY_CROSSOVER_OUTPUT_CINEMA_TSR = 11
} EntropyCrossoverOutputSemantic;

typedef struct EntropyCrossoverInputDescriptor {
    uint32_t struct_size;
    uint32_t slot_index;
    uint32_t semantic_id;
    uint32_t program;
    uint32_t program_channel_index;
    uint32_t reserved_u32[3];
    char identifier[48];
    char label[64];
} EntropyCrossoverInputDescriptor;

typedef struct EntropyCrossoverOutputDescriptor {
    uint32_t struct_size;
    uint32_t slot_index;
    uint32_t active;
    uint32_t semantic_role;
    uint32_t way_number;
    uint32_t engine_role_id;
    uint32_t program;
    uint32_t reserved_u32;
    char identifier[48];
    char label[96];
} EntropyCrossoverOutputDescriptor;

typedef struct EntropyCrossoverApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_input_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_input_descriptor)(uint32_t index, EntropyCrossoverInputDescriptor* descriptor);
    EntropyResult (ENTROPY_ABI_CALL *get_output_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_output_descriptor)(EntropyModuleHandle, uint32_t index, EntropyCrossoverOutputDescriptor* descriptor);
    EntropyResult (ENTROPY_ABI_CALL *get_program)(EntropyModuleHandle, uint32_t* program);
    EntropyResult (ENTROPY_ABI_CALL *set_program)(EntropyModuleHandle, uint32_t program);
    EntropyResult (ENTROPY_ABI_CALL *get_parameter)(EntropyModuleHandle, const char* identifier, double* value);
    EntropyResult (ENTROPY_ABI_CALL *set_parameter)(EntropyModuleHandle, const char* identifier, double value);
    EntropyResult (ENTROPY_ABI_CALL *export_state)(EntropyModuleHandle, void* bytes, uint32_t* in_out_byte_count);
    EntropyResult (ENTROPY_ABI_CALL *import_state)(EntropyModuleHandle, const void* bytes, uint32_t byte_count);
    uint64_t reserved[8];
} EntropyCrossoverApiV1;

typedef enum EntropyCrossoverParameterKind {
    ENTROPY_CROSSOVER_PARAMETER_CONTINUOUS = 0,
    ENTROPY_CROSSOVER_PARAMETER_BOOLEAN = 1,
    ENTROPY_CROSSOVER_PARAMETER_CHOICE = 2
} EntropyCrossoverParameterKind;

typedef struct EntropyCrossoverParameterDescriptor {
    uint32_t struct_size;
    uint32_t index;
    uint32_t kind;
    uint32_t choice_count;
    double minimum_value;
    double maximum_value;
    double default_value;
    char identifier[64];
    char label[96];
    uint64_t reserved[4];
} EntropyCrossoverParameterDescriptor;

typedef struct EntropyCrossoverSchemaApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_parameter_count)(uint32_t* count);
    EntropyResult (ENTROPY_ABI_CALL *get_parameter_descriptor)(EntropyModuleHandle, uint32_t index, EntropyCrossoverParameterDescriptor* descriptor);
    uint64_t reserved[8];
} EntropyCrossoverSchemaApiV1;

typedef struct EntropyCrossoverTelemetry {
    uint32_t struct_size;
    uint32_t latency_samples;
    uint32_t block_size;
    uint32_t cinema_program_active;
    double sample_rate;
    double music_input_peaks[3];
    double cinema_input_peaks[10];
    double output_peaks[16];
    double protection_gain_reduction_db[16];
    double protection_raw_peaks[16];
    double protection_below_db[16];
    double protection_above_db[16];
    uint32_t protection_state[16];
    uint64_t reserved[8];
} EntropyCrossoverTelemetry;

typedef struct EntropyCrossoverTelemetryApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyCrossoverTelemetry* telemetry);
    uint64_t reserved[8];
} EntropyCrossoverTelemetryApiV1;

typedef struct EntropyCrossoverGestureApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *begin_parameter_gesture)(
        EntropyModuleHandle, const char* identifier);
    EntropyResult (ENTROPY_ABI_CALL *end_parameter_gesture)(
        EntropyModuleHandle, const char* identifier);
    uint64_t reserved[8];
} EntropyCrossoverGestureApiV1;

typedef struct EntropyCrossoverViewTelemetry {
    uint32_t struct_size;
    uint32_t response_point_count;
    uint32_t spectrum_point_count;
    int32_t monitored_channel;
    double sample_rate;
    double minimum_frequency_hz;
    double maximum_frequency_hz;
    uint32_t spectrum_available;
    uint32_t reserved_u32;
    float response_db[16][ENTROPY_CROSSOVER_RESPONSE_POINTS];
    float spectrum_frequency_hz[ENTROPY_CROSSOVER_SPECTRUM_POINTS];
    float spectrum_db[ENTROPY_CROSSOVER_SPECTRUM_POINTS];
    uint64_t reserved[8];
} EntropyCrossoverViewTelemetry;

typedef struct EntropyCrossoverViewTelemetryApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *request_spectrum_channel)(
        EntropyModuleHandle, int32_t channel);
    EntropyResult (ENTROPY_ABI_CALL *get_view_telemetry)(
        EntropyModuleHandle, EntropyCrossoverViewTelemetry* telemetry);
    uint64_t reserved[8];
} EntropyCrossoverViewTelemetryApiV1;

typedef enum EntropyCrossoverProtectionState {
    ENTROPY_CROSSOVER_PROTECTION_ARMED_SAFE = 0,
    ENTROPY_CROSSOVER_PROTECTION_DISABLED_SAFE = 1,
    ENTROPY_CROSSOVER_PROTECTION_ARMED_WARNING = 2,
    ENTROPY_CROSSOVER_PROTECTION_ARMED_TRIPPED = 3,
    ENTROPY_CROSSOVER_PROTECTION_DISABLED_WARNING = 4
} EntropyCrossoverProtectionState;

typedef struct EntropyCrossoverProtectionConfig {
    uint32_t struct_size;
    uint32_t output_index;
    uint32_t enabled;
    uint32_t reserved_u32;
    double safety_min_hz;
    double safety_max_hz;
    double trip_hold_ms;
    uint64_t reserved[4];
} EntropyCrossoverProtectionConfig;

typedef struct EntropyCrossoverProtectionSummary {
    uint32_t struct_size;
    uint32_t output_count;
    uint32_t enabled_mask;
    uint32_t warning_mask;
    uint32_t tripped_mask;
    uint32_t global_tripped;
    uint32_t reserved_u32[2];
    uint64_t reserved[4];
} EntropyCrossoverProtectionSummary;

typedef struct EntropyCrossoverProtectionApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_output_config)(
        EntropyModuleHandle, uint32_t output_index, EntropyCrossoverProtectionConfig* config);
    EntropyResult (ENTROPY_ABI_CALL *set_output_config)(
        EntropyModuleHandle, const EntropyCrossoverProtectionConfig* config);
    EntropyResult (ENTROPY_ABI_CALL *reset_trip)(
        EntropyModuleHandle, uint32_t output_index);
    EntropyResult (ENTROPY_ABI_CALL *reset_all_trips)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *get_summary)(
        EntropyModuleHandle, EntropyCrossoverProtectionSummary* summary);
    uint64_t reserved[8];
} EntropyCrossoverProtectionApiV1;

typedef enum EntropyCrossoverCommandKindV1 {
    ENTROPY_CROSSOVER_COMMAND_PARAMETER = 1,
    ENTROPY_CROSSOVER_COMMAND_BULK = 2,
    ENTROPY_CROSSOVER_COMMAND_IMPORT = 3,
    ENTROPY_CROSSOVER_COMMAND_GESTURE_BEGIN = 4,
    ENTROPY_CROSSOVER_COMMAND_GESTURE_END = 5,
    ENTROPY_CROSSOVER_COMMAND_CONTROL_ONLY = 6
} EntropyCrossoverCommandKindV1;

typedef enum EntropyCrossoverCommandStateV1 {
    ENTROPY_CROSSOVER_COMMAND_IDLE = 0,
    ENTROPY_CROSSOVER_COMMAND_REQUESTED = 1,
    ENTROPY_CROSSOVER_COMMAND_ACCEPTED = 2,
    ENTROPY_CROSSOVER_COMMAND_APPLIED = 3,
    ENTROPY_CROSSOVER_COMMAND_COMMITTED_STOPPED = 4,
    ENTROPY_CROSSOVER_COMMAND_APPLICATION_UNCONFIRMED = 5,
    ENTROPY_CROSSOVER_COMMAND_FAILED = 6
} EntropyCrossoverCommandStateV1;

typedef enum EntropyCrossoverCommandReasonV1 {
    ENTROPY_CROSSOVER_COMMAND_REASON_NONE = 0,
    ENTROPY_CROSSOVER_COMMAND_REASON_INVALID_PARAMETER = 1,
    ENTROPY_CROSSOVER_COMMAND_REASON_AUTHORITY_REJECTED = 2,
    ENTROPY_CROSSOVER_COMMAND_REASON_QUEUE_FULL = 3,
    ENTROPY_CROSSOVER_COMMAND_REASON_SUPERSEDED = 4,
    ENTROPY_CROSSOVER_COMMAND_REASON_SESSION_MISMATCH = 5,
    ENTROPY_CROSSOVER_COMMAND_REASON_DUPLICATE_PAYLOAD_MISMATCH = 6,
    ENTROPY_CROSSOVER_COMMAND_REASON_EXTENSION_UNAVAILABLE = 7,
    ENTROPY_CROSSOVER_COMMAND_REASON_TIMEOUT = 8,
    ENTROPY_CROSSOVER_COMMAND_REASON_INTERNAL_ERROR = 9
} EntropyCrossoverCommandReasonV1;

typedef struct EntropyCrossoverCommandIdV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    uint64_t session;
    uint64_t sequence;
    uint64_t reserved[2];
} EntropyCrossoverCommandIdV1;

typedef struct EntropyCrossoverControlStatusV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    uint32_t state;
    uint32_t reason;
    uint32_t kind;
    uint32_t reserved_u32[3];
    EntropyCrossoverCommandIdV1 accepted;
    EntropyCrossoverCommandIdV1 applied;
    EntropyCrossoverCommandIdV1 failed;
    uint64_t reserved[4];
} EntropyCrossoverControlStatusV1;

typedef struct EntropyCrossoverControlStatusApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *begin_command)(
        EntropyModuleHandle, const EntropyCrossoverCommandIdV1*, uint32_t kind);
    EntropyResult (ENTROPY_ABI_CALL *commit_command)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *cancel_command)(
        EntropyModuleHandle, uint32_t reason);
    EntropyResult (ENTROPY_ABI_CALL *get_command_status)(
        EntropyModuleHandle, EntropyCrossoverControlStatusV1* status);
    uint64_t reserved[8];
} EntropyCrossoverControlStatusApiV1;

#ifdef __cplusplus
}
#endif
