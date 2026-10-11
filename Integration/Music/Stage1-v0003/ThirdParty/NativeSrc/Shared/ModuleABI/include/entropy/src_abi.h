#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_SRC_EXTENSION_ID "audio.entropy.sample-rate-converter.v1"
#define ENTROPY_SRC_EXTENSION_VERSION 0x00010000u
#define ENTROPY_SRC_MAX_CHANNELS 16u

typedef enum EntropySrcEngine {
    ENTROPY_SRC_R8BRAIN_REFERENCE = 0,
    ENTROPY_SRC_SOXR_VHQ = 1,
    // Stable ABI reservation. Production DLLs are not required to expose this
    // historical reference engine; legacy DLLs may still advertise ID 2.
    ENTROPY_SRC_SINC_LAB = 2
} EntropySrcEngine;

typedef struct EntropySrcEngineDescriptor {
    uint32_t struct_size;
    uint32_t engine;
    uint32_t flags;
    uint32_t reserved_u32;
    char identifier[48];
    char name[64];
    uint64_t reserved[4];
} EntropySrcEngineDescriptor;

typedef struct EntropySrcConfig {
    uint32_t struct_size;
    uint32_t engine;
    uint32_t source_rate;
    uint32_t destination_rate;
    uint32_t channels;
    uint32_t maximum_input_frames;
    uint32_t required_output_capacity;
    uint32_t reserved_u32;
    uint64_t reserved[4];
} EntropySrcConfig;

typedef struct EntropySrcBlockF64 {
    uint32_t struct_size;
    uint32_t channels;
    uint32_t input_frames;
    uint32_t output_capacity;
    const double* const* inputs;
    double* const* outputs;
    uint32_t consumed_frames;
    uint32_t produced_frames;
    uint32_t flags;
    uint32_t reserved_u32;
    uint64_t reserved[4];
} EntropySrcBlockF64;

typedef struct EntropySrcApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *get_engine_count)(uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *get_engine_descriptor)(uint32_t, EntropySrcEngineDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *configure)(EntropyModuleHandle, EntropySrcConfig*);
    EntropyResult (ENTROPY_ABI_CALL *get_config)(EntropyModuleHandle, EntropySrcConfig*);
    EntropyResult (ENTROPY_ABI_CALL *process)(EntropyModuleHandle, EntropySrcBlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *latency_output_frames)(EntropyModuleHandle, uint32_t*);
    uint64_t reserved[8];
} EntropySrcApiV1;

#ifdef __cplusplus
}
#endif
