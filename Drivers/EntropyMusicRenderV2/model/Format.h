#pragma once
#include "Types.h"
namespace entropy::music {
constexpr uint32_t MaxPacketFrames=65536, RingFrames=262144, RingDescriptors=4096;
constexpr uint8_t PcmGuid[16]={1,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
struct PcmFormat {
    uint32_t structSize, sampleRate, channels, containerBits, validBits;
    uint32_t blockAlign, averageBytesPerSecond, channelMask, formatTag, representation;
    uint8_t subFormat[16];
    uint32_t reserved[2];
};
static_assert(sizeof(PcmFormat)==64);
bool validateFormat(const PcmFormat&) noexcept;
bool payloadSize(const PcmFormat&,uint32_t frames,uint32_t& bytes) noexcept;
}
