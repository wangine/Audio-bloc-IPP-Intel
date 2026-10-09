#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_LOUDNESS_EXTENSION_ID "audio.entropy.loudness.v1"
#define ENTROPY_LOUDNESS_EXTENSION_VERSION 0x00010000u
#define ENTROPY_LOUDNESS_MAX_PARAMETERS 64u
#define ENTROPY_LOUDNESS_ENDPOINT_ID_BYTES 512u
#define ENTROPY_LOUDNESS_CURVE_POINTS 161u
// Control calls are non-realtime; process is a single audio-owner call. Destroy
// requires host process calls to be quiescent. Queue-full returns INVALID_STATE,
// never modifies the returned sequence and never overwrites accepted commands.
// Settings sequence is assigned by the DLL. Readback acknowledges block adoption.
typedef enum EntropyLoudnessParameterKind {
    ENTROPY_LOUDNESS_FLOAT=0, ENTROPY_LOUDNESS_BOOLEAN=1, ENTROPY_LOUDNESS_INTEGER=2
} EntropyLoudnessParameterKind;
typedef enum EntropyLoudnessMicrophoneState {
    ENTROPY_LOUDNESS_MIC_NONE=0, ENTROPY_LOUDNESS_MIC_OPENING=1,
    ENTROPY_LOUDNESS_MIC_ACTIVE=2, ENTROPY_LOUDNESS_MIC_STALE=3,
    ENTROPY_LOUDNESS_MIC_UNAVAILABLE=4, ENTROPY_LOUDNESS_MIC_ERROR=5
} EntropyLoudnessMicrophoneState;
typedef struct EntropyLoudnessParameter {
    uint32_t struct_size, index, kind, reserved;
    double minimum, maximum, step, default_value;
    char identifier[48], label[80];
} EntropyLoudnessParameter;
typedef struct EntropyLoudnessSettings {
    uint32_t struct_size, parameter_count;
    uint64_t sequence;
    double values[ENTROPY_LOUDNESS_MAX_PARAMETERS];
} EntropyLoudnessSettings;
typedef struct EntropyLoudnessMicrophone {
    uint32_t struct_size, available;
    char endpoint_id[ENTROPY_LOUDNESS_ENDPOINT_ID_BYTES], name[256];
} EntropyLoudnessMicrophone;
typedef struct EntropyLoudnessTelemetry {
    uint32_t struct_size, prepared, master_bypass, fault;
    uint64_t applied_sequence, processed_frames, control_epoch;
    uint32_t microphone_state, microphone_rate, microphone_channels, queue_full_count;
    double microphone_age_ms, microphone_rms_dbfs, microphone_peak_dbfs;
    double spl, smoothed_spl, loudness, mid_ratio_db, high_ratio_db;
    double air_mid, air_high, left_dbfs, right_dbfs, left_watts, right_watts;
    double bass_db, presence_db, treble_db, output_peak_dbfs, safety_reduction_db;
    double air_wet_peak_dbfs, air_safety_db, air_mid_peak_dbfs, air_high_peak_dbfs;
    double adaptive_amount, listening_phon, reference_phon;
    double curve_target_db[ENTROPY_LOUDNESS_CURVE_POINTS];
    double curve_realised_db[ENTROPY_LOUDNESS_CURVE_POINTS];
    char microphone_endpoint_id[ENTROPY_LOUDNESS_ENDPOINT_ID_BYTES];
    char detail[256];
    uint64_t microphone_open_attempts;
} EntropyLoudnessTelemetry;
typedef struct EntropyLoudnessApiV1 {
    uint32_t struct_size, extension_version;
    EntropyResult (ENTROPY_ABI_CALL *parameter_count)(EntropyModuleHandle,uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *parameter_descriptor)(EntropyModuleHandle,uint32_t,EntropyLoudnessParameter*);
    EntropyResult (ENTROPY_ABI_CALL *submit_settings)(EntropyModuleHandle,const EntropyLoudnessSettings*,uint64_t*);
    EntropyResult (ENTROPY_ABI_CALL *read_settings)(EntropyModuleHandle,EntropyLoudnessSettings*);
    EntropyResult (ENTROPY_ABI_CALL *read_telemetry)(EntropyModuleHandle,EntropyLoudnessTelemetry*);
    EntropyResult (ENTROPY_ABI_CALL *export_state)(EntropyModuleHandle,void*,uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *import_state)(EntropyModuleHandle,const void*,uint32_t,uint64_t*);
    EntropyResult (ENTROPY_ABI_CALL *microphone_count)(EntropyModuleHandle,uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *microphone_descriptor)(EntropyModuleHandle,uint32_t,EntropyLoudnessMicrophone*);
    EntropyResult (ENTROPY_ABI_CALL *select_microphone)(EntropyModuleHandle,const char*,uint64_t*);
    // Pure non-mutating project preflight; validates the opaque payload.
    EntropyResult (ENTROPY_ABI_CALL *validate_state)(EntropyModuleHandle,const void*,uint32_t);
    // Audio owner only, exclusively between process calls while programme is
    // stopped. Adopts commands/ACK without processing or inventing audio frames.
    EntropyResult (ENTROPY_ABI_CALL *service_stopped)(EntropyModuleHandle);
    uint64_t reserved[6];
} EntropyLoudnessApiV1;
#ifdef __cplusplus
}
#endif
