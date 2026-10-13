#pragma once
#include <stdint.h>

#define ENTROPY_DYNAMIC_RECOVERY_SCHEMA_ID 0x44524331u
#define ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION 1u

typedef struct EntropyDynamicRecoveryConfigV1 {
    uint32_t struct_size;
    uint32_t version;
    uint32_t enabled;
    uint32_t expansion_enabled;
    double transient_strength;
    double onset_threshold_db;
    double max_attack_boost_db;
    double expansion_ratio;
    double expansion_threshold_dbfs;
    double max_expansion_boost_db;
    double max_total_boost_db;
    double wet_mix;
    uint64_t reserved[4];
} EntropyDynamicRecoveryConfigV1;
