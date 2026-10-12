#pragma once

#include <cstddef>
#include <cstdint>

namespace entropy::surroundipc
{
inline constexpr wchar_t kMappingName[] =
    L"Local\\EntropyAudio.SurroundDecoder.v1";
inline constexpr std::uint32_t kMagic = 0x31534445; // "EDS1"
inline constexpr std::uint32_t kVersion = 0x00010000;
inline constexpr std::uint32_t kSampleRate = 48000;
inline constexpr std::uint32_t kChannels = 8;
inline constexpr std::uint32_t kRingFrames = 32768;

struct alignas(64) RingHeader
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t sampleRate;
    std::uint32_t channelCount;
    std::uint32_t capacityFrames;
    std::uint32_t reserved0;
    volatile std::int64_t writeFrame;
    volatile std::int64_t generation;
    volatile std::int64_t callbackCount;
    std::uint8_t reserved[16];
};

static_assert(sizeof(RingHeader) == 64);
inline constexpr std::size_t kSampleCount =
    static_cast<std::size_t>(kChannels) * kRingFrames;
inline constexpr std::size_t kMappingBytes =
    sizeof(RingHeader) + kSampleCount * sizeof(float);
}
