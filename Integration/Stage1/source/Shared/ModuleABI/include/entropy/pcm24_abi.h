#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_PCM24_EXTENSION_ID "audio.entropy.pcm24.v1"
#define ENTROPY_PCM24_EXTENSION_VERSION 0x00010000u
#define ENTROPY_PCM24_MAX_CHANNELS 16u

typedef enum EntropyPcm24DitherMode {
    ENTROPY_PCM24_DITHER_NONE = 0,
    ENTROPY_PCM24_DITHER_TPDF_FLAT = 1,
    ENTROPY_PCM24_DITHER_RESERVED_FUTURE = 2
} EntropyPcm24DitherMode;

typedef enum EntropyPcm24SeedMode {
    ENTROPY_PCM24_SEED_DETERMINISTIC_TEST = 0,
    ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT = 1
} EntropyPcm24SeedMode;

typedef enum EntropyPcm24OutputFormat {
    ENTROPY_PCM24_PACKED_LE = 0,
    ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED = 1
} EntropyPcm24OutputFormat;

typedef enum EntropyPcm24Flags {
    ENTROPY_PCM24_FLAG_NONE = 0,
    ENTROPY_PCM24_FLAG_DISCONTINUITY = 1u << 0
} EntropyPcm24Flags;

typedef enum EntropyPcm24Fault {
    ENTROPY_PCM24_FAULT_NONE = 0,
    ENTROPY_PCM24_FAULT_INVALID_CONFIG = 1u << 0,
    ENTROPY_PCM24_FAULT_NONFINITE_AUDIO = 1u << 1
} EntropyPcm24Fault;

typedef struct EntropyPcm24Config {
    uint32_t struct_size;
    uint32_t dither_mode;
    uint32_t seed_mode;
    uint32_t channels;
    uint64_t seed;
    uint32_t output_format;
    uint32_t maximum_frames;
    uint32_t revision;
    uint32_t reserved_u32;
    uint64_t reserved[3];
} EntropyPcm24Config;

typedef struct EntropyPcm24BlockF64 {
    uint32_t struct_size;
    uint32_t channels;
    uint32_t frames;
    uint32_t flags;
    const double* const* inputs;
    uint8_t* output_interleaved;
    uint32_t output_capacity_bytes;
    uint32_t produced_bytes;
    uint64_t reserved[4];
} EntropyPcm24BlockF64;

typedef struct EntropyPcm24Telemetry {
    uint32_t struct_size;
    uint32_t applied_revision;
    uint32_t fault_flags;
    uint32_t dither_mode;
    uint32_t seed_mode;
    uint32_t output_format;
    uint32_t channels;
    uint32_t bytes_per_sample;
    uint64_t processed_frames;
    uint64_t overrange_sample_count;
    uint64_t nonfinite_sample_count;
    uint64_t reserved[4];
} EntropyPcm24Telemetry;

typedef struct EntropyPcm24ApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *configure)(EntropyModuleHandle, const EntropyPcm24Config*);
    EntropyResult (ENTROPY_ABI_CALL *get_config)(EntropyModuleHandle, EntropyPcm24Config*);
    EntropyResult (ENTROPY_ABI_CALL *process)(EntropyModuleHandle, EntropyPcm24BlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyPcm24Telemetry*);
    EntropyResult (ENTROPY_ABI_CALL *clear_fault)(EntropyModuleHandle);
    uint64_t reserved[8];
} EntropyPcm24ApiV1;

#ifdef __cplusplus
}
#endif
