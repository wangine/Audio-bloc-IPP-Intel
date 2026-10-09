#pragma once

#include <entropy/module_abi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTROPY_PARAMETRIC_EQ_EXTENSION_ID "audio.entropy.parametric-eq.v1"
#define ENTROPY_PARAMETRIC_EQ_EXTENSION_VERSION 0x00010000u
#define ENTROPY_PARAMETRIC_EQ_STATE_SCHEMA 0x31514550u
#define ENTROPY_PARAMETRIC_EQ_STATE_VERSION 0x00010000u
#define ENTROPY_PARAMETRIC_EQ_BAND_COUNT 8u

typedef enum EntropyParametricEqMode {
    ENTROPY_PARAMETRIC_EQ_MODE_BYPASS = 0,
    ENTROPY_PARAMETRIC_EQ_MODE_STATIC = 1,
    ENTROPY_PARAMETRIC_EQ_MODE_DYNAMIC = 2,
    ENTROPY_PARAMETRIC_EQ_MODE_STATIC_AND_DYNAMIC = 3
} EntropyParametricEqMode;

typedef enum EntropyParametricEqFilterType {
    ENTROPY_PARAMETRIC_EQ_FILTER_BELL = 0,
    ENTROPY_PARAMETRIC_EQ_FILTER_LOW_SHELF = 1,
    ENTROPY_PARAMETRIC_EQ_FILTER_HIGH_SHELF = 2
} EntropyParametricEqFilterType;

typedef enum EntropyParametricEqDetectorMode {
    ENTROPY_PARAMETRIC_EQ_DETECTOR_PEAK = 0,
    ENTROPY_PARAMETRIC_EQ_DETECTOR_RMS = 1,
    ENTROPY_PARAMETRIC_EQ_DETECTOR_HYBRID = 2
} EntropyParametricEqDetectorMode;

typedef enum EntropyParametricEqDetectorSource {
    ENTROPY_PARAMETRIC_EQ_SOURCE_LOCAL = 0,
    ENTROPY_PARAMETRIC_EQ_SOURCE_GLOBAL = 1,
    ENTROPY_PARAMETRIC_EQ_SOURCE_HYBRID = 2
} EntropyParametricEqDetectorSource;

typedef enum EntropyParametricEqDynamicsMode {
    ENTROPY_PARAMETRIC_EQ_DOWN_COMPRESSION = 0,
    ENTROPY_PARAMETRIC_EQ_UP_COMPRESSION = 1,
    ENTROPY_PARAMETRIC_EQ_DOWN_EXPANSION = 2,
    ENTROPY_PARAMETRIC_EQ_UP_EXPANSION = 3
} EntropyParametricEqDynamicsMode;

typedef enum EntropyParametricEqParameterKind {
    ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT = 0,
    ENTROPY_PARAMETRIC_EQ_PARAMETER_BOOLEAN = 1,
    ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE = 2
} EntropyParametricEqParameterKind;

typedef enum EntropyParametricEqParameterField {
    ENTROPY_PARAMETRIC_EQ_FIELD_MODE = 0,
    ENTROPY_PARAMETRIC_EQ_FIELD_ENABLED = 1,
    ENTROPY_PARAMETRIC_EQ_FIELD_FILTER_TYPE = 2,
    ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_MODE = 3,
    ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_SOURCE = 4,
    ENTROPY_PARAMETRIC_EQ_FIELD_DYNAMICS_MODE = 5,
    ENTROPY_PARAMETRIC_EQ_FIELD_FREQUENCY_HZ = 6,
    ENTROPY_PARAMETRIC_EQ_FIELD_Q = 7,
    ENTROPY_PARAMETRIC_EQ_FIELD_STATIC_GAIN_DB = 8,
    ENTROPY_PARAMETRIC_EQ_FIELD_THRESHOLD_DB = 9,
    ENTROPY_PARAMETRIC_EQ_FIELD_RATIO = 10,
    ENTROPY_PARAMETRIC_EQ_FIELD_KNEE_DB = 11,
    ENTROPY_PARAMETRIC_EQ_FIELD_ATTACK_MS = 12,
    ENTROPY_PARAMETRIC_EQ_FIELD_RELEASE_MS = 13,
    ENTROPY_PARAMETRIC_EQ_FIELD_MAX_CUT_DB = 14,
    ENTROPY_PARAMETRIC_EQ_FIELD_MAX_BOOST_DB = 15
} EntropyParametricEqParameterField;

typedef struct EntropyParametricEqParameterV1 {
    uint32_t struct_size;
    uint32_t index;
    uint32_t kind;
    uint32_t choice_count;
    uint32_t band_index; /* UINT32_MAX for module-wide controls */
    uint32_t field;
    uint32_t reserved_u32[2];
    double minimum;
    double maximum;
    double step;
    double default_value;
    char identifier[48];
    char label[80];
    uint64_t reserved[4];
} EntropyParametricEqParameterV1;

typedef struct EntropyParametricEqBandV1 {
    uint32_t struct_size;
    uint32_t enabled;
    uint32_t filter_type;
    uint32_t detector_mode;
    uint32_t detector_source;
    uint32_t dynamics_mode;
    uint32_t reserved_u32[2];
    double frequency_hz;
    double q;
    double static_gain_db;
    double threshold_db;
    double ratio;
    double knee_db;
    double attack_ms;
    double release_ms;
    double max_cut_db;
    double max_boost_db;
} EntropyParametricEqBandV1;

typedef struct EntropyParametricEqSettingsV1 {
    uint32_t struct_size;
    uint32_t mode;
    uint32_t band_count;
    uint32_t reserved_u32;
    uint64_t sequence;
    EntropyParametricEqBandV1 bands[ENTROPY_PARAMETRIC_EQ_BAND_COUNT];
} EntropyParametricEqSettingsV1;

typedef struct EntropyParametricEqTelemetryV1 {
    uint32_t struct_size;
    uint32_t prepared;
    uint32_t module_bypass;
    uint32_t band_count;
    uint64_t applied_sequence;
    uint64_t processed_frames;
    float detector_db[ENTROPY_PARAMETRIC_EQ_BAND_COUNT];
    float dynamic_gain_db[ENTROPY_PARAMETRIC_EQ_BAND_COUNT];
    uint64_t reserved[8];
} EntropyParametricEqTelemetryV1;

typedef struct EntropyParametricEqApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *submit_settings)(
        EntropyModuleHandle, const EntropyParametricEqSettingsV1*, uint64_t*);
    EntropyResult (ENTROPY_ABI_CALL *read_settings)(
        EntropyModuleHandle, EntropyParametricEqSettingsV1*);
    EntropyResult (ENTROPY_ABI_CALL *read_telemetry)(
        EntropyModuleHandle, EntropyParametricEqTelemetryV1*);
    EntropyResult (ENTROPY_ABI_CALL *export_state)(
        EntropyModuleHandle, void*, uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *validate_state)(
        EntropyModuleHandle, const void*, uint32_t);
    EntropyResult (ENTROPY_ABI_CALL *import_state)(
        EntropyModuleHandle, const void*, uint32_t, uint64_t*);
    /* Added in reserved v1 tail; original callback offsets remain stable. */
    EntropyResult (ENTROPY_ABI_CALL *parameter_count)(
        EntropyModuleHandle, uint32_t*);
    EntropyResult (ENTROPY_ABI_CALL *parameter_descriptor)(
        EntropyModuleHandle, uint32_t, EntropyParametricEqParameterV1*);
    uint64_t reserved[6];
} EntropyParametricEqApiV1;

#ifdef __cplusplus
}
#endif
