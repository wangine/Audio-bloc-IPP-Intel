#pragma once
#include <stddef.h>
#include <stdint.h>
#define STAGE_BUS_ABI 0x00010000u
#define STAGE_BUS_MAGIC UINT64_C(0x3153554247545345)
#define STAGE_BUS_FRAMES 1024u
#define STAGE_BUS_RATE 192000u
#define STAGE_BUS_CAPACITY 4u
#define MIX_CINEMA10_V1 1u
#define LRC_CINEMA11_V1 2u
#define MUSIC_STEREO2_V1 3u
#define MUSIC_LRC3_V1 4u
#define CINEMA_51_6_V1 5u
#define CINEMA_71_8_V1 6u
#define STAGE_BUS_BINARY64_LE 1u
#define STAGE_BUS_PLANAR 1u
/* All fields have fixed little-endian wire representation; reserved bytes zero. */
typedef struct StageBusSessionHeader
{
    uint64_t magic;
    uint32_t abi_version, header_bytes;
    uint8_t session_id[16];
    uint32_t layout_id, channels, sample_rate, sample_format, planar, frames_per_block, capacity,
        block_header_bytes;
    uint64_t plane_stride, slot_stride, slots_offset, mapping_bytes, qpc_frequency;
    uint8_t layout_hash[32], contract_hash[32];
    uint32_t producer_pid, consumer_pid;
    uint8_t reserved[80];
} StageBusSessionHeader;
typedef struct StageBusProgramSnapshot
{
    uint32_t program, required_inputs, music_program_active, analyze_music_final;
    uint32_t manual_mute_mask, effective_mute_mask, hardware_armed, output_source_mask;
    uint64_t program_session, program_revision, output_revision, project_revision;
} StageBusProgramSnapshot;
typedef struct StageBusFrameRequest
{
    uint8_t session_id[16];
    uint64_t stream_epoch, block_sequence, first_frame, control_revision;
    uint32_t frames, reserved;
    StageBusProgramSnapshot program;
} StageBusFrameRequest;
typedef struct StageBusBlockHeader
{
    uint8_t session_id[16];
    uint64_t stream_epoch, block_sequence, first_frame, control_revision;
    uint64_t source_generation[3], qpc_timestamp, music_session_revision;
    uint32_t frames, validity_mask, discontinuity_mask, source_state[3], source_rate[3];
    uint32_t reserved;
} StageBusBlockHeader;
typedef struct StageBusSourceControl
{
    uint8_t session_id[16];
    uint64_t source_revision;
    uint32_t manual_mute_mask, src_engines[3], music_left, music_right;
    uint64_t music_binding_revision, reserved;
} StageBusSourceControl;
#ifdef __cplusplus
static_assert(sizeof(StageBusSourceControl) == 64);
static_assert(offsetof(StageBusSourceControl, source_revision) == 16);
static_assert(offsetof(StageBusSourceControl, music_binding_revision) == 48);
static_assert(sizeof(StageBusSessionHeader) == 256);
static_assert(sizeof(StageBusProgramSnapshot) == 64);
static_assert(sizeof(StageBusFrameRequest) == 120);
static_assert(sizeof(StageBusBlockHeader) == 128);
static_assert(offsetof(StageBusSessionHeader, plane_stride) == 64);
static_assert(offsetof(StageBusSessionHeader, contract_hash) == 136);
static_assert(offsetof(StageBusFrameRequest, program) == 56);
static_assert(offsetof(StageBusBlockHeader, frames) == 88);
#endif
