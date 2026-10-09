#pragma once

#include <entropy/module_abi.h>
#include <entropy/pcm24_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_DAC_SINK_EXTENSION_ID "audio.entropy.dac-sink.v1"
#define ENTROPY_DAC_SINK_EXTENSION_VERSION 0x00010000u
#define ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES 768u
#define ENTROPY_DAC_SINK_NAME_BYTES 256u

typedef enum EntropyDacSinkBackend {
    ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE = 1,
    ENTROPY_DAC_SINK_BACKEND_ASIO_RESERVED = 2
} EntropyDacSinkBackend;

typedef enum EntropyDacSinkState {
    ENTROPY_DAC_SINK_CLOSED = 0,
    ENTROPY_DAC_SINK_OPEN = 1,
    ENTROPY_DAC_SINK_RUNNING = 2,
    ENTROPY_DAC_SINK_DEVICE_LOST = 3,
    ENTROPY_DAC_SINK_FAULTED = 4
} EntropyDacSinkState;

typedef enum EntropyDacSinkFault {
    ENTROPY_DAC_SINK_FAULT_NONE = 0,
    ENTROPY_DAC_SINK_FAULT_INVALID_CONFIG = 1u << 0,
    ENTROPY_DAC_SINK_FAULT_FORMAT_UNSUPPORTED = 1u << 1,
    ENTROPY_DAC_SINK_FAULT_XRUN = 1u << 2,
    ENTROPY_DAC_SINK_FAULT_DEVICE_LOST = 1u << 3,
    ENTROPY_DAC_SINK_FAULT_DRIVER = 1u << 4
} EntropyDacSinkFault;

typedef struct EntropyDacSinkDeviceInfo {
    uint32_t struct_size;
    uint32_t backend;
    uint32_t active;
    uint32_t is_default;
    char endpoint_id_utf8[ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES];
    char display_name_utf8[ENTROPY_DAC_SINK_NAME_BYTES];
    uint64_t reserved[4];
} EntropyDacSinkDeviceInfo;

typedef struct EntropyDacSinkConfig {
    uint32_t struct_size;
    uint32_t backend;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t output_format;
    uint32_t requested_buffer_frames;
    uint32_t revision;
    uint32_t reserved_u32;
    char endpoint_id_utf8[ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES];
    uint64_t reserved[4];
} EntropyDacSinkConfig;

typedef struct EntropyDacSinkBufferRequest {
    uint32_t struct_size;
    uint32_t frames;
    uint32_t bytes;
    uint32_t timeout_ms;
    uint64_t device_position_frames;
    uint64_t device_qpc_position;
    uint64_t reserved[3];
} EntropyDacSinkBufferRequest;

typedef struct EntropyDacSinkPcmBlock {
    uint32_t struct_size;
    uint32_t frames;
    const uint8_t* data;
    uint32_t byte_count;
    uint32_t flags;
    uint32_t accepted_frames;
    uint64_t reserved[4];
} EntropyDacSinkPcmBlock;

typedef struct EntropyDacSinkStatus {
    uint32_t struct_size;
    uint32_t state;
    uint32_t fault_flags;
    int32_t last_driver_result;
    uint32_t applied_revision;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t output_format;
    uint32_t buffer_frames;
    uint32_t block_alignment_bytes;
    uint64_t submitted_frames;
    uint64_t rendered_buffers;
    uint64_t xrun_count;
    uint64_t timeout_count;
    uint64_t device_loss_count;
    uint64_t device_position_frames;
    uint64_t device_qpc_position;
    uint64_t reserved[4];
} EntropyDacSinkStatus;

typedef struct EntropyDacSinkApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *refresh_devices)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *device_count)(EntropyModuleHandle, uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *get_device)(EntropyModuleHandle, uint32_t, EntropyDacSinkDeviceInfo*);
    EntropyResult (ENTROPY_ABI_CALL *open)(EntropyModuleHandle, const EntropyDacSinkConfig*);
    EntropyResult (ENTROPY_ABI_CALL *close)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *start)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *stop)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *wait_buffer)(EntropyModuleHandle, EntropyDacSinkBufferRequest*);
    EntropyResult (ENTROPY_ABI_CALL *submit)(EntropyModuleHandle, EntropyDacSinkPcmBlock*);
    EntropyResult (ENTROPY_ABI_CALL *get_status)(EntropyModuleHandle, EntropyDacSinkStatus*);
    uint64_t reserved[8];
} EntropyDacSinkApiV1;

#ifdef __cplusplus
}
#endif
