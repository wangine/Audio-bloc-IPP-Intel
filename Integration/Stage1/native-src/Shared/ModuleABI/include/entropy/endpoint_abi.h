#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_ENDPOINT_EXTENSION_ID "audio.entropy.endpoint-source.v1"
#define ENTROPY_ENDPOINT_EXTENSION_VERSION 0x00010000u

/* Provider truth flags share the discontinuity_flags word without changing ABI layout. */
#define ENTROPY_ENDPOINT_FLAG_SHARED_MIX_RATE (1u << 16)
#define ENTROPY_ENDPOINT_FLAG_EXCLUSIVE_NATIVE_RATE (1u << 17)
#define ENTROPY_ENDPOINT_FLAG_MODE_UNKNOWN (1u << 18)

typedef enum EntropyEndpointState {
    ENTROPY_ENDPOINT_DISCONNECTED = 0,
    ENTROPY_ENDPOINT_IDLE = 1,
    ENTROPY_ENDPOINT_ACTIVE = 2
} EntropyEndpointState;

typedef struct EntropyEndpointStatus {
    uint32_t struct_size;
    uint32_t state;
    uint32_t source_rate;
    uint32_t channels;
    uint32_t container_bits;
    uint32_t valid_bits;
    uint32_t discontinuity_flags;
    uint32_t available_frames;
    uint64_t stream_generation;
    uint64_t total_frames_written;
    uint64_t total_frames_read;
    uint64_t lost_frames;
    uint64_t reserved[4];
} EntropyEndpointStatus;

typedef struct EntropyEndpointBlockF64 {
    uint32_t struct_size;
    uint32_t capacity_frames;
    uint32_t frame_count;
    uint32_t reserved_u32;
    double* left;
    double* right;
    EntropyEndpointStatus status;
    uint64_t reserved[4];
} EntropyEndpointBlockF64;

typedef struct EntropyEndpointApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    uint32_t minimum_buffer_frames;
    uint32_t reserved_u32;
    EntropyResult (ENTROPY_ABI_CALL *connect)(EntropyModuleHandle);
    void (ENTROPY_ABI_CALL *disconnect)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *read_source_f64)(EntropyModuleHandle, EntropyEndpointBlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *get_status)(EntropyModuleHandle, EntropyEndpointStatus*);
    uint64_t reserved[8];
} EntropyEndpointApiV1;

#ifdef __cplusplus
}
#endif
