#pragma once
#include <stdint.h>
/* B2-FF-006 TEMP_FORENSIC. Separate additive interface; neither audio V1 nor
   compact diagnostics V1 is reinterpreted. POD layout, no C++ object crossings. */
#if defined(_WIN32)
#define ENTROPY_FORENSIC_CALL __cdecl
#else
#define ENTROPY_FORENSIC_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_EGRESS_FORENSIC_ID "audio.entropy.egress-forensic.v1"
#define ENTROPY_EGRESS_FORENSIC_VERSION 0x00010000u
#define ENTROPY_FORENSIC_PUSH 1u
#define ENTROPY_FORENSIC_RENDER 2u
#define ENTROPY_FORENSIC_FIFO_OBSERVED 1u
#define ENTROPY_FORENSIC_CONTROL_CALLED 2u
#define ENTROPY_FORENSIC_CONTROL_UPDATED 4u
#define ENTROPY_FORENSIC_FILL_AFTER_VALID 8u
/* Rejection reason is the actual observe() branch, not inferred from a clock. */
enum EntropyEstimateReason {
 ENTROPY_EST_NOT_CALLED=0, ENTROPY_EST_VALID=1, ENTROPY_EST_NOT_PREPARED=2,
 ENTROPY_EST_DISCONTINUOUS=3, ENTROPY_EST_ZERO_OBSERVATION=4,
 ENTROPY_EST_NONMONOTONIC=5, ENTROPY_EST_GAP_OVER_500MS=6,
 ENTROPY_EST_WINDOW_SHORT=7, ENTROPY_EST_TOO_FEW_POINTS=8,
 ENTROPY_EST_DEGENERATE=9, ENTROPY_EST_RATE_INVALID=10
};
typedef struct EntropyForensicControl {
 uint64_t observe_calls, accepted_observations, consecutive_rejections;
 uint64_t last_accepted_host100ns, last_valid_host100ns, reset_count;
 uint64_t controller_calls;
 uint32_t estimate_reason, estimate_valid, decision_updated, reset_site;
 double window_seconds, estimate_confidence, estimated_sink_hz, estimated_nominal_hz;
 double base_ppm, backlog_ppm, requested_ppm, applied_ppm;
 double fill_before, step_begin, step_end;
} EntropyForensicControl;
typedef struct EntropyForensicEvent {
 uint32_t struct_size, kind, valid_fields, flags;
 uint64_t sequence, enter_ns, exit_ns;
 uint64_t sink_frame, device_host100ns;
 uint64_t nominal_count_observed, output_count_observed;
 uint32_t requested_frames, consumed_frames, produced_frames, state;
 int32_t api_result;
 uint32_t clock_fault_flags;
 double last_fill_observed;
 EntropyForensicControl control;
} EntropyForensicEvent;
typedef struct EntropyForensicStatus {
 uint32_t struct_size, version, enabled, queue_capacity;
 uint64_t now_ns, push_dropped, render_dropped;
 uint64_t push_written, render_written;
 uint64_t reserved[4];
} EntropyForensicStatus;
/* Exactly ONE non-realtime collector may call drain per module instance.
   get_status/now_ns do not drain. Reset/destruction requires ALL callers stopped.
   handle is the same opaque instance address supplied to the original module API.
   int32 return codes use EntropyResult numeric values. Drain max capacity 4096. */
typedef struct EntropyEgressForensicApiV1 {
 uint32_t struct_size, extension_version;
 int32_t (ENTROPY_FORENSIC_CALL *get_status)(void* handle, EntropyForensicStatus*);
 int32_t (ENTROPY_FORENSIC_CALL *drain)(void* handle, uint32_t kind,
     EntropyForensicEvent* events, uint32_t capacity, uint32_t* produced);
 uint64_t reserved[8];
} EntropyEgressForensicApiV1;
#ifdef __cplusplus
}
#endif
