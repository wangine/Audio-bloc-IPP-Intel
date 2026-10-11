#pragma once

#include <entropy/endpoint_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID "audio.entropy.cinema-endpoint.v1"
#define ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION 0x00010000u
#define ENTROPY_CINEMA_ENDPOINT_MAX_CHANNELS 16u

typedef struct EntropyCinemaEndpointStatus {
    uint32_t struct_size;
    uint32_t state;
    uint32_t source_rate;
    uint32_t channels;
    uint32_t sample_bits;
    uint32_t available_frames;
    uint64_t stream_generation;
    uint64_t callback_count;
    uint64_t write_frame;
    uint64_t read_frame;
    uint64_t reserved[4];
} EntropyCinemaEndpointStatus;

typedef struct EntropyCinemaEndpointBlockF64 {
    uint32_t struct_size;
    uint32_t capacity_frames;
    uint32_t output_capacity_channels;
    uint32_t frame_count;
    double* const* outputs;
    EntropyCinemaEndpointStatus status;
    uint64_t reserved[4];
} EntropyCinemaEndpointBlockF64;

typedef struct EntropyCinemaEndpointApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    uint32_t minimum_buffer_frames;
    uint32_t maximum_channels;
    EntropyResult (ENTROPY_ABI_CALL *connect)(EntropyModuleHandle);
    void (ENTROPY_ABI_CALL *disconnect)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *read_source_f64)(
        EntropyModuleHandle, EntropyCinemaEndpointBlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *get_status)(
        EntropyModuleHandle, EntropyCinemaEndpointStatus*);
    uint64_t reserved[8];
} EntropyCinemaEndpointApiV1;

#ifdef __cplusplus
}
#endif
