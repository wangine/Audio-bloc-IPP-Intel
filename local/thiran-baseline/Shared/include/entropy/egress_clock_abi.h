#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_EGRESS_CLOCK_EXTENSION_ID "audio.entropy.egress-clock-sync.v1"
#define ENTROPY_EGRESS_CLOCK_EXTENSION_VERSION 0x00010000u
#define ENTROPY_EGRESS_CLOCK_MAX_CHANNELS 16u

typedef enum EntropyEgressClockFlags {
    ENTROPY_EGRESS_CLOCK_NONE = 0,
    ENTROPY_EGRESS_CLOCK_DISCONTINUITY = 1u << 0,
    ENTROPY_EGRESS_CLOCK_END_OF_STREAM = 1u << 1
} EntropyEgressClockFlags;

typedef enum EntropyEgressClockState {
    ENTROPY_EGRESS_CLOCK_PREROLL = 0,
    ENTROPY_EGRESS_CLOCK_LOCKED = 1,
    ENTROPY_EGRESS_CLOCK_STARVED = 2,
    ENTROPY_EGRESS_CLOCK_RESYNC = 3,
    ENTROPY_EGRESS_CLOCK_FAULT = 4
} EntropyEgressClockState;

typedef enum EntropyEgressClockFault {
    ENTROPY_EGRESS_CLOCK_FAULT_NONE = 0,
    ENTROPY_EGRESS_CLOCK_FAULT_INVALID_CONFIG = 1u << 0,
    ENTROPY_EGRESS_CLOCK_FAULT_NONFINITE_AUDIO = 1u << 1,
    ENTROPY_EGRESS_CLOCK_FAULT_FIFO_OVERRUN = 1u << 2
} EntropyEgressClockFault;

typedef struct EntropyEgressClockConfig {
    uint32_t struct_size;
    uint32_t revision;
    uint32_t engine_rate;
    uint32_t sink_rate;
    uint32_t channels;
    uint32_t maximum_input_frames;
    uint32_t maximum_render_frames;
    uint32_t fifo_capacity_frames;
    uint32_t target_fill_frames;
    double maximum_correction_ppm;
    double correction_smoothing_ms;
    uint64_t reserved[6];
} EntropyEgressClockConfig;

typedef struct EntropyEgressClockPushBlockF64 {
    uint32_t struct_size;
    uint32_t channels;
    uint32_t frames;
    uint32_t flags;
    const double* const* inputs;
    uint32_t consumed_frames;
    uint32_t nominal_frames_written;
    uint32_t reserved_u32;
    uint64_t reserved[4];
} EntropyEgressClockPushBlockF64;

typedef struct EntropyEgressClockRenderBlockF64 {
    uint32_t struct_size;
    uint32_t channels;
    uint32_t frames;
    uint32_t flags;
    double* const* outputs;
    uint32_t produced_frames;
    uint32_t state;
    uint32_t reserved_u32;
    uint64_t sink_position_frames;
    uint64_t host_time_100ns;
    uint64_t reserved[3];
} EntropyEgressClockRenderBlockF64;

typedef struct EntropyEgressClockTelemetry {
    uint32_t struct_size;
    uint32_t applied_revision;
    uint32_t state;
    uint32_t fault_flags;
    uint32_t engine_rate;
    uint32_t sink_rate;
    uint32_t channels;
    uint32_t fifo_capacity_frames;
    double fill_frames;
    double target_fill_frames;
    double correction_ppm;
    double requested_correction_ppm;
    uint64_t input_frames;
    uint64_t nominal_frames;
    uint64_t output_frames;
    uint64_t render_callbacks;
    /* Render callbacks that lacked unplanned source frames. Intentional
       PREROLL and fault-produced silence are not starvation. */
    uint64_t starved_callbacks;
    uint64_t resync_count;
    uint64_t nonfinite_sample_count;
    uint64_t sink_position_frames;
    uint64_t host_time_100ns;
    uint64_t reserved[4];
} EntropyEgressClockTelemetry;

typedef struct EntropyEgressClockApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *configure)(EntropyModuleHandle, const EntropyEgressClockConfig*);
    EntropyResult (ENTROPY_ABI_CALL *get_config)(EntropyModuleHandle, EntropyEgressClockConfig*);
    EntropyResult (ENTROPY_ABI_CALL *push)(EntropyModuleHandle, EntropyEgressClockPushBlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *render)(EntropyModuleHandle, EntropyEgressClockRenderBlockF64*);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle, EntropyEgressClockTelemetry*);
    EntropyResult (ENTROPY_ABI_CALL *clear_fault)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *latency_sink_frames)(EntropyModuleHandle, uint32_t*);
    uint64_t reserved[8];
} EntropyEgressClockApiV1;

#ifdef __cplusplus
}
#endif
