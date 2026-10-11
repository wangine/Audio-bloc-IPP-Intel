#pragma once
#include "endpoint_provider_v2.h"
#define STAGE1_CONTROL_MAGIC UINT64_C(0x3143544D31535445)
#define STAGE1_CONTROL_ABI 0x00010000u
#define STAGE1_CONTROL_ENDPOINTS 32u
#define STAGE1_OP_STATUS 0u
#define STAGE1_OP_ENUMERATE 1u
#define STAGE1_OP_SELECT 2u
typedef struct Stage1ControlRequest {
    uint64_t magic;
    uint32_t abiVersion, structSize, operation, role, mode, catalogOffset;
    char providerId[96], endpointId[512];
    uint64_t requestId, reserved[4];
} Stage1ControlRequest;
typedef struct Stage1SourceSnapshot {
    char providerId[96], endpointId[512];
    uint32_t state, representation, mode, rate, containerBits, validBits, fillFrames, metadataFlags;
    uint64_t generation, packets, framesIn, framesOut, lostFrames, overruns, underruns, faults;
    uint64_t packetSequence, firstSourceFrame, qpcTimestamp, qpcFrequency;
    double prePeakL, prePeakR, preRmsL, preRmsR, preMidRms, preSideRms, postPeakL, postPeakR, postRmsL,
        postRmsR, postMidRms, postSideRms;
    double truePeakL, truePeakR, startupPeakL, startupPeakR;
    uint64_t startupFramesMeasured;
    uint32_t truePeakReady, startupComplete;
} Stage1SourceSnapshot;
typedef struct Stage1CatalogEntry {
    char providerId[96];
    EntropyEndpointInfoV2 endpoint;
} Stage1CatalogEntry;
typedef struct Stage1ControlResponse {
    uint64_t magic;
    uint32_t abiVersion, structSize;
    uint64_t requestId;
    uint32_t status, endpointCount, truncated, catalogTotal;
    uint64_t blocksPublished, publisherDrops, controlRevision, musicRevision;
    uint64_t clockQpcFrequency, clockLateBlocks;
    uint32_t clockRunning, outputRate;
    uint8_t sessionId[16];
    char message[256];
    Stage1SourceSnapshot sources[2];
    Stage1CatalogEntry endpoints[STAGE1_CONTROL_ENDPOINTS];
} Stage1ControlResponse;
#ifdef __cplusplus
static_assert(sizeof(Stage1ControlRequest) == 680);
static_assert(sizeof(Stage1ControlResponse) < 65536);
static_assert(sizeof(Stage1SourceSnapshot) == 880);
#endif
