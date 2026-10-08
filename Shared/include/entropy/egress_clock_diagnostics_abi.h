#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif

/* B2-FF-002: optional, additive ABI. No change to egress_clock_abi.h.
   Stage A only: compact capture. Trace/export/Host integration are unavailable. */
#define ENTROPY_EGRESS_DIAGNOSTICS_EXTENSION_ID "audio.entropy.egress-clock-diagnostics.v1"
#define ENTROPY_EGRESS_DIAGNOSTICS_EXTENSION_VERSION 0x00010000u
#define ENTROPY_EGRESS_DIAGNOSTICS_RECORD_VERSION 1u

enum EntropyEgressDiagnosticPublication { ENTROPY_FF_EMPTY=0, ENTROPY_FF_WRITING=1, ENTROPY_FF_READY=2 };
enum EntropyEgressDiagnosticOrigin { ENTROPY_FF_ORIGIN_NONE=0, ENTROPY_FF_ORIGIN_PUSH=1, ENTROPY_FF_ORIGIN_RENDER=2, ENTROPY_FF_ORIGIN_CONFIGURE=3 };
enum EntropyEgressDiagnosticSite { ENTROPY_FF_SITE_CAPACITY=1, ENTROPY_FF_SITE_NONFINITE=2, ENTROPY_FF_SITE_CONFIG=3, ENTROPY_FF_SITE_NOMINAL_CONFIG=4, ENTROPY_FF_SITE_CONTROL_PREPARE=5 };
enum EntropyEgressDiagnosticValid { ENTROPY_FF_VALID_CONFIG=1u, ENTROPY_FF_VALID_CAPACITY_OPERANDS=2u };
#define ENTROPY_FF_NAMESPACE_EGRESS_CLOCK 1u

typedef struct EntropyEgressFirstFaultRecord {
    uint32_t struct_size;
    uint32_t record_version;
    uint32_t fault_namespace;
    uint32_t fault_code;
    uint32_t origin;
    uint32_t site;
    int32_t api_result;
    uint32_t valid_fields;
    uint64_t instance_id; /* unique only within this loaded DLL; qualify by process */
    uint64_t lifecycle_id; /* changed by quiescent reset/configure */
    uint64_t first_event_id; /* 1 within lifecycle; not a cross-thread clock */
    uint64_t monotonic_ns; /* steady_clock, NOT UTC; 0 means unavailable */
    uint32_t config_revision;
    uint32_t engine_rate;
    uint32_t sink_rate;
    uint32_t channels;
    int64_t write_frame_used;
    int64_t published_read_frame_used;
    uint64_t retained_used;
    uint64_t maximum_produced_used;
    uint64_t producer_epoch;
    uint32_t incoming_frames;
    uint32_t block_flags;
    uint32_t guard_frames;
    uint32_t capacity_frames;
    uint32_t target_frames;
    uint32_t last_render_snapshot_valid; /* always 0 in stage A; do not infer */
    uint64_t reserved[4];
} EntropyEgressFirstFaultRecord;

typedef struct EntropyEgressDiagnosticSnapshot {
    uint32_t struct_size;
    uint32_t extension_version;
    uint32_t publication_state;
    uint32_t compact_enabled;
    uint32_t forensic_trace_available; /* 0 in stage A */
    uint32_t forensic_trace_enabled; /* 0 in stage A */
    uint64_t preroll_silence_callbacks;
    uint64_t true_underrun_callbacks;
    uint64_t fault_silence_callbacks;
    char build_profile[32];
    char build_source_id[48];
    EntropyEgressFirstFaultRecord first_fault;
    uint64_t reserved[4];
} EntropyEgressDiagnosticSnapshot;

typedef struct EntropyEgressDiagnosticsApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    /* Read-only, bounded; no observer registration, reset, drain or injection. */
    EntropyResult (ENTROPY_ABI_CALL *get_snapshot)(EntropyModuleHandle, EntropyEgressDiagnosticSnapshot*);
    uint64_t reserved[8];
} EntropyEgressDiagnosticsApiV1;

#ifdef __cplusplus
}
#endif
