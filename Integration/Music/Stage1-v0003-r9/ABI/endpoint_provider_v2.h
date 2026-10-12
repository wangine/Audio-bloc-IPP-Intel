#pragma once
#include <stdint.h>
#ifdef _WIN32
#define ENTROPY_PROVIDER_CALL __cdecl
#define ENTROPY_PROVIDER_EXPORT __declspec(dllexport)
#else
#define ENTROPY_PROVIDER_CALL
#define ENTROPY_PROVIDER_EXPORT
#endif
#define ENTROPY_PROVIDER_ABI_V2 0x00020000u
#define ENTROPY_ROLE_MUSIC 1u
#define ENTROPY_ROLE_AUX 2u
#define ENTROPY_PACKET_DISCONTINUITY 1u
#define ENTROPY_PACKET_SILENCE 2u
#define ENTROPY_PACKET_RAW_IDENTITY 4u
#define ENTROPY_PACKET_METADATA_PARTIAL 8u
#define ENTROPY_REP_PCM 1u
#define ENTROPY_REP_F64_PLANAR 2u
#define ENTROPY_MODE_UNKNOWN 0u
#define ENTROPY_MODE_SHARED 1u
#define ENTROPY_MODE_EXCLUSIVE 2u
#define ENTROPY_CAP_RAW_PCM 1u
#define ENTROPY_CAP_CONVERTED_F64 2u
#define ENTROPY_MAX_PACKET_FRAMES 65536u
#ifdef __cplusplus
extern "C" {
#endif
typedef struct EntropyIngressFormatV2 {
    uint32_t structSize, sampleRate, channels, containerBits, validBits;
    uint32_t blockAlign, averageBytesPerSecond, channelMask, formatTag, representation;
    uint8_t subFormat[16];
    uint32_t reserved[2];
} EntropyIngressFormatV2;
typedef struct EntropyIngressPacketV2 {
    uint32_t abiVersion, structSize;
    EntropyIngressFormatV2 format;
    uint64_t generation, packetSequence, firstSourceFrame, qpcTimestamp, qpcFrequency, lostFrames;
    uint32_t frames, flags, mode, payloadBytes;
    // Original word length, distinct from FP64 payload. Both zero means unknown.
    uint32_t originalContainerBits, originalValidBits;
    uint64_t reserved[2];
} EntropyIngressPacketV2;
typedef struct EntropyProviderInfoV2 {
    uint32_t abiVersion, structSize, capabilities, roles;
    char providerId[96], name[128];
    uint64_t reserved[2];
} EntropyProviderInfoV2;
typedef struct EntropyEndpointInfoV2 {
    uint32_t abiVersion, structSize, roles, eligible, state, flow, accessModes, reserved;
    char endpointId[512], name[256], windowsEndpointId[512];
    EntropyIngressFormatV2 preferredFormat;
} EntropyEndpointInfoV2;
typedef struct EntropyProviderApiV2 {
    uint32_t abiVersion, structSize;
    EntropyProviderInfoV2 info;
    int32_t(ENTROPY_PROVIDER_CALL *enumerate)(uint32_t, EntropyEndpointInfoV2 *);
    int32_t(ENTROPY_PROVIDER_CALL *open)(const char *, uint32_t, void **);
    int32_t(ENTROPY_PROVIDER_CALL *read)(void *, EntropyIngressPacketV2 *, void *, uint32_t);
    void(ENTROPY_PROVIDER_CALL *cancel)(void *);
    void(ENTROPY_PROVIDER_CALL *close)(void *);
    uint64_t reserved[4];
} EntropyProviderApiV2;
typedef int32_t(ENTROPY_PROVIDER_CALL *EntropyProviderEntryV2)(uint32_t, EntropyProviderApiV2 *);
#ifdef __cplusplus
}
static_assert(sizeof(EntropyIngressFormatV2) == 64);
static_assert(sizeof(EntropyIngressPacketV2) == 160);
static_assert(sizeof(EntropyProviderInfoV2) == 256);
static_assert(sizeof(EntropyEndpointInfoV2) == 1376);
static_assert(sizeof(EntropyProviderApiV2) == 336);
#endif
