#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  define ENTROPY_ABI_CALL __cdecl
#  if defined(ENTROPY_MODULE_BUILD)
#    define ENTROPY_ABI_EXPORT __declspec(dllexport)
#  else
#    define ENTROPY_ABI_EXPORT
#  endif
#else
#  define ENTROPY_ABI_CALL
#  define ENTROPY_ABI_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_MODULE_ABI_VERSION 0x00010000u
#define ENTROPY_MODULE_GET_API_SYMBOL "entropy_module_get_api"

typedef struct EntropyModuleOpaque* EntropyModuleHandle;

typedef enum EntropyResult {
    ENTROPY_OK = 0,
    ENTROPY_ERROR_INVALID_ARGUMENT = -1,
    ENTROPY_ERROR_ABI_MISMATCH = -2,
    ENTROPY_ERROR_UNSUPPORTED = -3,
    ENTROPY_ERROR_NOT_PREPARED = -4,
    ENTROPY_ERROR_INVALID_STATE = -5,
    ENTROPY_ERROR_PROCESSING = -6
} EntropyResult;

typedef enum EntropyModuleKind {
    ENTROPY_MODULE_UNKNOWN = 0,
    ENTROPY_MODULE_TEST = 1,
    ENTROPY_MODULE_SRC = 2,
    ENTROPY_MODULE_CROSSOVER = 3,
    ENTROPY_MODULE_CENTER_EXTRACTOR = 4,
    ENTROPY_MODULE_LOUDNESS = 5,
    ENTROPY_MODULE_PARAMETRIC_EQ = 6,
    ENTROPY_MODULE_INPUT_MATRIX = 7,
    ENTROPY_MODULE_CLOCK_SYNC = 8,
    ENTROPY_MODULE_DAC_SINK = 9,
    ENTROPY_MODULE_ENDPOINT = 10,
    ENTROPY_MODULE_PCM_CONVERTER = 11,
    ENTROPY_MODULE_OUTPUT_MATRIX = 12,
    ENTROPY_MODULE_HEADROOM_MANAGER = 13,
    ENTROPY_MODULE_FINAL_GUARDIAN = 14,
    ENTROPY_MODULE_DEVICE_MANAGER = 15
} EntropyModuleKind;

typedef enum EntropyProcessFlags {
    ENTROPY_PROCESS_NONE = 0,
    ENTROPY_PROCESS_DISCONTINUITY = 1u << 0,
    ENTROPY_PROCESS_END_OF_STREAM = 1u << 1
} EntropyProcessFlags;

typedef enum EntropyModuleFlags {
    ENTROPY_MODULE_FLAG_NONE = 0,
    ENTROPY_MODULE_FLAG_REALTIME_SAFE_PROCESS = 1u << 0,
    ENTROPY_MODULE_FLAG_IN_PLACE = 1u << 1,
    ENTROPY_MODULE_FLAG_FP64 = 1u << 2,
    ENTROPY_MODULE_FLAG_REPORTS_LATENCY = 1u << 3
} EntropyModuleFlags;

typedef struct EntropyModuleDescriptor {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t module_kind;
    uint32_t module_flags;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t max_input_channels;
    uint32_t max_output_channels;
    uint32_t reserved_u32[7];
    char identifier[64];
    char name[96];
    char vendor[64];
} EntropyModuleDescriptor;

typedef void (ENTROPY_ABI_CALL *EntropyHostLogFn)(void* user_data, int32_t severity, const char* message);

typedef struct EntropyHostServices {
    uint32_t struct_size;
    uint32_t abi_version;
    void* user_data;
    EntropyHostLogFn log_non_realtime;
    uint64_t reserved[4];
} EntropyHostServices;

typedef struct EntropyPrepareInfo {
    uint32_t struct_size;
    uint32_t sample_rate;
    uint32_t maximum_frames;
    uint32_t input_channels;
    uint32_t output_channels;
    uint32_t reserved_u32[3];
} EntropyPrepareInfo;

typedef struct EntropyAudioBlockF64 {
    uint32_t struct_size;
    uint32_t input_channels;
    uint32_t output_channels;
    uint32_t frames;
    const double* const* inputs;
    double* const* outputs;
    uint64_t reserved[2];
} EntropyAudioBlockF64;

typedef struct EntropyProcessContext {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t stream_time_samples;
    uint64_t reserved[3];
} EntropyProcessContext;

typedef struct EntropyConfigBlob {
    uint32_t struct_size;
    uint32_t schema_id;
    uint32_t schema_version;
    uint32_t byte_count;
    const void* bytes;
    uint64_t reserved[2];
} EntropyConfigBlob;

typedef EntropyResult (ENTROPY_ABI_CALL *EntropyModuleQueryExtensionFn)(
    const char* extension_identifier,
    uint32_t requested_extension_version,
    void* extension_api,
    uint32_t extension_api_size);

typedef struct EntropyModuleApiV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    EntropyResult (ENTROPY_ABI_CALL *get_descriptor)(EntropyModuleDescriptor*);
    EntropyResult (ENTROPY_ABI_CALL *create)(const EntropyHostServices*, EntropyModuleHandle*);
    void (ENTROPY_ABI_CALL *destroy)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *prepare)(EntropyModuleHandle, const EntropyPrepareInfo*);
    EntropyResult (ENTROPY_ABI_CALL *reset)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *set_config)(EntropyModuleHandle, const EntropyConfigBlob*);
    EntropyResult (ENTROPY_ABI_CALL *process_f64)(EntropyModuleHandle, const EntropyAudioBlockF64*, const EntropyProcessContext*);
    EntropyResult (ENTROPY_ABI_CALL *latency_samples)(EntropyModuleHandle, uint32_t*);
    EntropyModuleQueryExtensionFn query_extension;
    uint64_t reserved[7];
} EntropyModuleApiV1;

typedef EntropyResult (ENTROPY_ABI_CALL *EntropyModuleGetApiFn)(uint32_t requested_abi_version, EntropyModuleApiV1* api);

ENTROPY_ABI_EXPORT EntropyResult ENTROPY_ABI_CALL entropy_module_get_api(uint32_t requested_abi_version, EntropyModuleApiV1* api);

#ifdef __cplusplus
}
#endif
