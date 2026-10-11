#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_LRC_EXTENSION_ID "audio.entropy.lrc-aligner.v1"
#define ENTROPY_LRC_EXTENSION_VERSION 0x00010000u
#define ENTROPY_LRC_STATE_SCHEMA 0x3143524cu
#define ENTROPY_LRC_MAX_DELAY 65536u
typedef enum EntropyLrcMode { ENTROPY_LRC_BYPASS_EXACT=0, ENTROPY_LRC_ALIGN_STATIC=1 } EntropyLrcMode;
typedef struct EntropyLrcAlignerStateV1 {
 uint32_t struct_size,mode;
 uint32_t delay_l_samples,delay_r_samples,delay_c_samples,target_latency_samples;
 uint64_t reserved[8];
} EntropyLrcAlignerStateV1;
typedef struct EntropyLrcAlignerTelemetryV1 {
 uint32_t struct_size,reserved_u32;
 EntropyLrcAlignerStateV1 state;
 uint64_t processed_frames,reset_count,fault_count;
 uint32_t configured,active,bypassed,reserved_flags;
 uint64_t reserved[4];
} EntropyLrcAlignerTelemetryV1;
/* All calls serialized by owner. set/import require reset before processing.
   Control/state operations are outside the realtime process scope. */
typedef struct EntropyLrcAlignerApiV1 {
 uint32_t struct_size,abi_version;
 EntropyResult (ENTROPY_ABI_CALL *get_state)(EntropyModuleHandle,EntropyLrcAlignerStateV1*);
 EntropyResult (ENTROPY_ABI_CALL *set_state)(EntropyModuleHandle,const EntropyLrcAlignerStateV1*);
 EntropyResult (ENTROPY_ABI_CALL *export_state)(EntropyModuleHandle,void*,uint32_t*);
 EntropyResult (ENTROPY_ABI_CALL *import_state)(EntropyModuleHandle,const void*,uint32_t);
 EntropyResult (ENTROPY_ABI_CALL *get_telemetry)(EntropyModuleHandle,EntropyLrcAlignerTelemetryV1*);
 uint64_t reserved[4];
} EntropyLrcAlignerApiV1;
#ifdef __cplusplus
}
static_assert(sizeof(EntropyLrcAlignerStateV1)==88);
#endif
