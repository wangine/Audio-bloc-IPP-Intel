#pragma once
#include <entropy/module_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ENTROPY_CROSSOVER_PROGRAM_BLOCK_EXTENSION_ID "audio.entropy.crossover-program-block.v1"
#define ENTROPY_CROSSOVER_PROGRAM_BLOCK_EXTENSION_VERSION 0x00010000u
#define ENTROPY_PROGRAM_INPUT_MUSIC 1u
#define ENTROPY_PROGRAM_INPUT_CINEMA 2u
/* Processing-thread-only synchronous result. A successful call identifies the
 * snapshot used for this block, NOT delayed sample onset or physical output.
 * session changes at prepare/reset/state reprepare; revision changes only when
 * the adopted MUSIC/CINEMA target changes. No status on a failed/empty block.
 * required_inputs includes ramp contributors needed for the next block; the
 * host must also supply inputs for any newly requested target. */
typedef struct EntropyCrossoverProgramInputs {
    uint32_t struct_size;
    uint32_t output_source_mask; /* lanes routed by the serialized Output Matrix state */
    uint64_t music_session, music_generation, cinema_generation;
} EntropyCrossoverProgramInputs;
typedef struct EntropyCrossoverProgramBlock {
    uint32_t struct_size;
    uint32_t program;
    uint32_t required_inputs;
    uint32_t frames;
    uint64_t session;
    uint64_t block_sequence;
    uint64_t program_revision;
    /* Producer-output high-water identity, after actual FIR/delay contributors.
     * output_revision==0 means initial fill, no output identity yet.
     * Coalesce arrivals to one egress flag per changed block; first frame and
     * count remain explicit. No new arrival: first_transition_frame=UINT32_MAX. */
    uint64_t output_revision;
    uint32_t output_program;
    uint32_t first_transition_frame;
    uint32_t transitions_in_block;
    uint32_t reserved0;
    uint64_t reserved[4];
} EntropyCrossoverProgramBlock;
typedef struct EntropyCrossoverProgramBlockApiV1 {
    uint32_t struct_size;
    uint32_t extension_version;
    EntropyResult (ENTROPY_ABI_CALL *process_f64)(EntropyModuleHandle,
        const EntropyAudioBlockF64*, const EntropyProcessContext*,
        const EntropyCrossoverProgramInputs*, EntropyCrossoverProgramBlock*);
    /* Processing owner: call after serialized control updates, immediately
     * before collecting inputs. No control/prepare/reset interleaving until
     * process_f64, which requires exactly prepared maximum_frames per call. */
    EntropyResult (ENTROPY_ABI_CALL *get_required_inputs)(EntropyModuleHandle, uint32_t*);
    uint64_t reserved[7];
} EntropyCrossoverProgramBlockApiV1;
#ifdef __cplusplus
}
#endif
