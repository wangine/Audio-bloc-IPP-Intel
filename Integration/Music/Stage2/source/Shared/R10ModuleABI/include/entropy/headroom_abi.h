#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_HEADROOM_EXTENSION_ID "audio.entropy.headroom.manager.v1"
#define ENTROPY_HEADROOM_EXTENSION_VERSION 0x00010001u
#define ENTROPY_HEADROOM_MODE_EXTENSION_ID "audio.entropy.headroom.manager.mode.v1"
#define ENTROPY_HEADROOM_MODE_EXTENSION_VERSION 0x00010000u
#define ENTROPY_HEADROOM_POLICY_EXTENSION_ID "audio.entropy.headroom.manager.policy.v2"
#define ENTROPY_HEADROOM_POLICY_EXTENSION_VERSION 0x00020000u
#define ENTROPY_HEADROOM_CONTROL_EXTENSION_ID "audio.entropy.headroom.manager.control.v3"
#define ENTROPY_HEADROOM_CONTROL_EXTENSION_VERSION 0x00030000u
#define ENTROPY_HEADROOM_ANALYZER_EXTENSION_ID "audio.entropy.headroom.manager.analyzer.v4"
#define ENTROPY_HEADROOM_ANALYZER_EXTENSION_VERSION 0x00040000u
#define ENTROPY_HEADROOM_TIMING_EXTENSION_ID "audio.entropy.headroom.manager.timing.v5"
#define ENTROPY_HEADROOM_TIMING_EXTENSION_VERSION 0x00050000u
typedef struct EntropyHeadroomTimingStateV5 {
    uint32_t struct_size;
    uint32_t revision;
    double attack_ms;
    double release_ms;
} EntropyHeadroomTimingStateV5;
typedef struct EntropyHeadroomTimingApiV5 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *set_timing)(EntropyModuleHandle,const EntropyHeadroomTimingStateV5*);
    EntropyResult (ENTROPY_ABI_CALL *get_timing)(EntropyModuleHandle,EntropyHeadroomTimingStateV5*);
} EntropyHeadroomTimingApiV5;
#define ENTROPY_HEADROOM_CHANNELS 16u
#define ENTROPY_HEADROOM_MAX_GROUPS 16u
typedef enum EntropyHeadroomFault {
    ENTROPY_HEADROOM_FAULT_NONE=0,
    ENTROPY_HEADROOM_FAULT_INVALID_CONFIG=1u<<0,
    ENTROPY_HEADROOM_FAULT_INVALID_LEDGER=1u<<1,
    ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO=1u<<2
} EntropyHeadroomFault;
typedef enum EntropyHeadroomGroupStatus {
    ENTROPY_HEADROOM_GROUP_UNITY=0,
    ENTROPY_HEADROOM_GROUP_REDUCED=1,
    ENTROPY_HEADROOM_GROUP_TRANSITION=2,
    ENTROPY_HEADROOM_GROUP_FAULT=3
} EntropyHeadroomGroupStatus;
typedef enum EntropyHeadroomOperatingMode {
    ENTROPY_HEADROOM_MODE_AUTO=0,
    ENTROPY_HEADROOM_MODE_BIT_PERFECT=1,
    ENTROPY_HEADROOM_MODE_MANUAL_SAFE=2,
    /* Source-compatible names for v1/v2 hosts. */
    ENTROPY_HEADROOM_MODE_ACTIVE=ENTROPY_HEADROOM_MODE_AUTO,
    ENTROPY_HEADROOM_MODE_UNITY_BYPASS=ENTROPY_HEADROOM_MODE_BIT_PERFECT,
    ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION=ENTROPY_HEADROOM_MODE_MANUAL_SAFE
} EntropyHeadroomOperatingMode;
typedef struct EntropyHeadroomLedgerEntry {
    uint32_t struct_size;
    uint32_t bounded;
    double declared_positive_contribution_db;
    double guaranteed_input_headroom_db;
    double preventive_reserve_db;
    uint64_t reserved[3];
} EntropyHeadroomLedgerEntry;
typedef struct EntropyHeadroomState {
    uint32_t struct_size;
    uint32_t revision;
    uint32_t group_count;
    uint32_t active_channel_mask;
    double transition_ms;
    uint32_t channel_group[ENTROPY_HEADROOM_CHANNELS];
    EntropyHeadroomLedgerEntry groups[ENTROPY_HEADROOM_MAX_GROUPS];
    uint64_t reserved[4];
} EntropyHeadroomState;
typedef struct EntropyHeadroomGroupTelemetry {
    double current_gain;
    double target_gain;
    double gain_reduction_db;
    double ledger_required_headroom_db;
    double reserve_margin_db;
    double transition_progress;
    uint32_t status;
    uint32_t reserved_u32;
} EntropyHeadroomGroupTelemetry;
typedef struct EntropyHeadroomTelemetry {
    uint32_t struct_size;
    uint32_t applied_revision;
    uint32_t group_count;
    uint32_t fault_flags;
    uint32_t host_engine_rate;
    uint32_t transition_samples;
    uint64_t nonfinite_sample_count;
    uint64_t processed_frames;
    EntropyHeadroomGroupTelemetry groups[ENTROPY_HEADROOM_MAX_GROUPS];
    double measured_input_peak;
    double measured_input_true_peak;
    double auto_analysis_reduction_db;
    uint32_t analyzer_state;
    uint32_t limiting_channel_mask;
    double channel_input_peak[ENTROPY_HEADROOM_CHANNELS];
    uint32_t true_peak_status;
    uint32_t reserved_u32;
} EntropyHeadroomTelemetry;
typedef struct EntropyHeadroomApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *set_state)(EntropyModuleHandle,const EntropyHeadroomState*);
    EntropyResult (ENTROPY_ABI_CALL *get_state)(EntropyModuleHandle,EntropyHeadroomState*);
    EntropyResult (ENTROPY_ABI_CALL *clear_fault)(EntropyModuleHandle);
    EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle,EntropyHeadroomTelemetry*);
    uint64_t reserved[8];
} EntropyHeadroomApiV1;
typedef struct EntropyHeadroomModeState {
    uint32_t struct_size;
    uint32_t requested_mode;
    uint32_t effective_mode;
    uint32_t transition_active;
    double transition_progress;
    uint64_t reserved[4];
} EntropyHeadroomModeState;
typedef struct EntropyHeadroomModeApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *set_mode)(EntropyModuleHandle,uint32_t);
    EntropyResult (ENTROPY_ABI_CALL *get_mode)(EntropyModuleHandle,EntropyHeadroomModeState*);
    uint64_t reserved[8];
} EntropyHeadroomModeApiV1;
/*
 * V2 is a separate extension so older hosts keep the exact V1 contract.
 * fixed_attenuation_db is a positive attenuation magnitude. It is used only
 * in FIXED_SAFETY_ATTENUATION; ACTIVE remains ledger-owned and UNITY BYPASS
 * remains a steady-state bit-exact copy path.
 */
typedef struct EntropyHeadroomPolicyStateV2 {
    uint32_t struct_size;
    uint32_t requested_mode;
    uint32_t effective_mode;
    uint32_t transition_active;
    double transition_progress;
    double fixed_attenuation_db;
    double fixed_target_gain;
    uint64_t reserved[4];
} EntropyHeadroomPolicyStateV2;
typedef struct EntropyHeadroomPolicyApiV2 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *set_policy)(EntropyModuleHandle,uint32_t,double);
    EntropyResult (ENTROPY_ABI_CALL *get_policy)(EntropyModuleHandle,EntropyHeadroomPolicyStateV2*);
    uint64_t reserved[8];
} EntropyHeadroomPolicyApiV2;

typedef enum EntropyHeadroomAnalyzerState {
    ENTROPY_HEADROOM_ANALYZER_IDLE=0,
    ENTROPY_HEADROOM_ANALYZER_TRACKING=1,
    ENTROPY_HEADROOM_ANALYZER_HOLD=2,
    ENTROPY_HEADROOM_ANALYZER_RECOVERY=3
} EntropyHeadroomAnalyzerState;

/*
 * Canonical v3 control contract. manual_gain_db is an actual gain in dB and
 * therefore must be finite and <= 0.0. AUTO remains ledger-owned; BIT_PERFECT
 * reaches a literal copy path after the sample-domain transition.
 */
typedef struct EntropyHeadroomControlStateV3 {
    uint32_t struct_size;
    uint32_t requested_mode;
    uint32_t effective_mode;
    uint32_t transition_active;
    double transition_progress;
    double manual_gain_db;
    double manual_target_gain;
    uint64_t reserved[4];
} EntropyHeadroomControlStateV3;
typedef struct EntropyHeadroomControlApiV3 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *set_mode)(EntropyModuleHandle,uint32_t);
    EntropyResult (ENTROPY_ABI_CALL *set_manual_gain_db)(EntropyModuleHandle,double);
    EntropyResult (ENTROPY_ABI_CALL *get_control_state)(EntropyModuleHandle,EntropyHeadroomControlStateV3*);
    uint64_t reserved[8];
} EntropyHeadroomControlApiV3;

/*
 * V4 separates the early gain actuator from the late program analyzer.
 * process_f64() remains the actuator immediately after Input Matrix.
 * analyze_final_f64() observes the final FP64 program after Output Matrix and
 * before ClockSync/Guardian; it never writes audio and never owns lifecycle.
 * The target calculated here is consumed by the next actuator block.
 */
typedef struct EntropyHeadroomAnalysisBlockF64 {
    uint32_t struct_size;
    uint32_t channels;
    uint32_t frames;
    uint32_t reserved_u32;
    const double* const* inputs;
    uint64_t reserved[4];
} EntropyHeadroomAnalysisBlockF64;
typedef struct EntropyHeadroomAnalyzerApiV4 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *analyze_final_f64)(
        EntropyModuleHandle,const EntropyHeadroomAnalysisBlockF64*);
    uint64_t reserved[8];
} EntropyHeadroomAnalyzerApiV4;
#ifdef __cplusplus
}
#endif
