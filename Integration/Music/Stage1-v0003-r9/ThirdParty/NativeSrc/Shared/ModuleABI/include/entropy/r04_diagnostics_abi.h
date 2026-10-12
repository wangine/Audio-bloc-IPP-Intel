#pragma once
#include <entropy/module_abi.h>
#include <entropy/egress_forensic_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_R04_CLOCK_ID "audio.entropy.a06a-r04-clock-diag.v1"
#define ENTROPY_R04_SINK_ID "audio.entropy.a06a-r04-sink-diag.v1"
#define ENTROPY_R04_VERSION 0x00010000u
typedef struct EntropyR04ClockQuality {
 uint64_t ok, s_false, fail, raw_position, raw_qpc_100ns;
 uint32_t fresh, stale;
 int32_t hresult;
 uint32_t called;
} EntropyR04ClockQuality;
typedef struct EntropyR04ControlDelta {
 double delta_requested, delta_applied, max_abs_delta_applied;
 double update_dt_seconds, applied_ppm_per_second;
 uint32_t rate_valid, reserved;
} EntropyR04ControlDelta;
typedef struct EntropyR04ClockSnapshot {
 uint32_t struct_size, control_called, state, engine_reset_reason;
 uint64_t callback_sequence, input_frames, nominal_frames, output_frames;
 EntropyForensicControl control;
 EntropyR04ControlDelta delta;
 double read_position_begin, fractional_phase_begin, read_position_end, fractional_phase_end;
 double target_fill, fill_error, fill_after;
 uint32_t fifo_capacity, phase_valid;
} EntropyR04ClockSnapshot;
/* Both accessors are owner-thread ONLY, immediately after render/wait_buffer.
   Never call from Qt/collector; host copies the POD into its SPSC queue.
   Existing audio ABI, HRESULT acceptance and all DSP behavior remain intact. */
typedef struct EntropyR04ClockApi {
 uint32_t struct_size, extension_version;
 EntropyResult (ENTROPY_ABI_CALL *owner_snapshot)(EntropyModuleHandle, EntropyR04ClockSnapshot*);
} EntropyR04ClockApi;
typedef struct EntropyR04SinkApi {
 uint32_t struct_size, extension_version;
 EntropyResult (ENTROPY_ABI_CALL *owner_snapshot)(EntropyModuleHandle, EntropyR04ClockQuality*);
} EntropyR04SinkApi;
#ifdef __cplusplus
}
#endif
