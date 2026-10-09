#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_FINAL_GUARDIAN_EXTENSION_ID "audio.entropy.final-output-guardian.v1"
#define ENTROPY_FINAL_GUARDIAN_EXTENSION_VERSION 0x00010000u
#define ENTROPY_FINAL_GUARDIAN_MAX_CHANNELS 16u
#define ENTROPY_FINAL_GUARDIAN_OVERSAMPLE_FACTOR 16u

typedef enum EntropyFinalGuardianState {
    ENTROPY_GUARDIAN_ARMED = 0,
    ENTROPY_GUARDIAN_FADING_TO_SAFE = 1,
    ENTROPY_GUARDIAN_SAFE_MUTED = 2,
    ENTROPY_GUARDIAN_FAULT_MUTED = 3
} EntropyFinalGuardianState;

typedef enum EntropyFinalGuardianFault {
    ENTROPY_GUARDIAN_FAULT_NONE = 0,
    ENTROPY_GUARDIAN_FAULT_INVALID_CONFIG = 1u << 0,
    ENTROPY_GUARDIAN_FAULT_NONFINITE_AUDIO = 1u << 1
} EntropyFinalGuardianFault;

typedef struct EntropyFinalGuardianConfig {
    uint32_t struct_size;
    uint32_t revision;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t maximum_frames;
    uint32_t oversample_factor;
    double ceiling_dbtp;
    double lookahead_ms;
    double emergency_fade_ms;
    uint64_t reserved[6];
} EntropyFinalGuardianConfig;

typedef struct EntropyFinalGuardianTelemetry {
    uint32_t struct_size;
    uint32_t applied_revision;
    uint32_t state;
    uint32_t fault_flags;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t lookahead_frames;
    uint32_t emergency_fade_frames;
    uint32_t oversample_factor;
    uint32_t trip_channel;
    double ceiling_dbtp;
    double ceiling_linear;
    double maximum_observed_true_peak;
    double trip_true_peak;
    uint64_t trip_input_frame;
    uint64_t processed_input_frames;
    uint64_t produced_output_frames;
    uint64_t intervention_count;
    uint64_t nonfinite_sample_count;
    uint64_t reserved[4];
} EntropyFinalGuardianTelemetry;

typedef struct EntropyFinalGuardianApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *configure)(EntropyModuleHandle, const EntropyFinalGuardianConfig*);
    EntropyResult (ENTROPY_ABI_CALL *get_config)(EntropyModuleHandle, EntropyFinalGuardianConfig*);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyFinalGuardianTelemetry*);
    EntropyResult (ENTROPY_ABI_CALL *rearm)(EntropyModuleHandle);
    uint64_t reserved[8];
} EntropyFinalGuardianApiV1;

#ifdef __cplusplus
}
#endif
