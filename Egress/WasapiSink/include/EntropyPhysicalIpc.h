#pragma once

#include <cstddef>
#include <cstdint>

namespace entropy::physical
{
inline constexpr wchar_t kInputMappingName[] =
    L"Local\\EntropyAudio.PhysicalInput.v1";
inline constexpr std::uint32_t kInputMagic = 0x31495045; // "EPI1"
inline constexpr std::uint32_t kInputVersion = 0x00010000;
inline constexpr std::uint32_t kInputSampleRate = 192000;
inline constexpr std::uint32_t kInputChannels = 128;
inline constexpr std::uint32_t kInputRingFrames = 16384;

struct alignas(64) InputRingHeader
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t sampleRate;
    std::uint32_t channelCount;
    std::uint16_t containerBits;
    std::uint16_t validBits;
    std::uint32_t capacityFrames;
    std::uint32_t reserved0;
    volatile std::int64_t writeFrame;
    volatile std::int64_t generation;
    volatile std::int64_t callbackCount;
    std::uint8_t reserved[8];
};

static_assert(sizeof(InputRingHeader) == 64);

inline constexpr std::size_t kInputSampleCount =
    static_cast<std::size_t>(kInputChannels) *
    static_cast<std::size_t>(kInputRingFrames);
inline constexpr std::size_t kInputMappingBytes =
    sizeof(InputRingHeader) +
    kInputSampleCount * sizeof(std::int32_t);

inline constexpr wchar_t kTelemetryMappingName[] =
    L"Local\\EntropyMatrixCrossover02.PhysicalTelemetry.v7";
inline constexpr std::uint32_t kTelemetryMagic = 0x37545045; // "EPT7"
inline constexpr std::uint32_t kTelemetryVersion = 0x00070000;
inline constexpr std::uint32_t kTelemetryDacSlots = 2;

struct alignas(64) Telemetry
{
    volatile std::int32_t sequence;
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t structSize;
    volatile std::int32_t renderEndpointOpen;
    std::int32_t renderChannels;
    std::uint32_t renderBufferFrames;
    std::int32_t lastOpenResult;
    volatile std::int64_t renderCallbacks;
    volatile std::int64_t renderFrames;
    volatile std::int64_t asioWriteFrame;
    volatile std::uint64_t heartbeatMs;
    float outputPeak[16];
    volatile std::int64_t readerReadyBuffers;
    volatile std::int64_t readerStarvedBuffers;
    volatile std::int64_t readerResyncs;
    volatile std::int32_t readerReady;
    volatile std::int32_t readerReason;
    float readerFillFrames;
    float readerTargetFrames;
    float readerCorrectionPpm;
    std::int32_t outputSampleRate;
    std::int16_t outputContainerBits;
    std::int16_t outputValidBits;
    std::int32_t outputEncoding;
    std::int32_t dacSlot;
    std::int32_t reserved[2];
};

struct alignas(64) TelemetryBank
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t sinkCount;
    Telemetry sinks[kTelemetryDacSlots];
};

inline bool readTelemetrySnapshot(
    const Telemetry* source,
    Telemetry& destination)
{
    if (source == nullptr)
        return false;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const auto before = source->sequence;
        if ((before & 1) != 0)
            continue;
        destination = *source;
        const auto after = source->sequence;
        if (before == after && (after & 1) == 0)
            return true;
    }
    return false;
}
} // namespace entropy::physical
