#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace entropy::telemetry
{
inline constexpr std::uint32_t kEngineMagic = 0x45544E50;
inline constexpr std::uint32_t kEngineVersion = 0x00070000;
inline constexpr wchar_t kEngineMappingName[] =
    L"Local\\EntropyAudioEngineStateV050";

struct EngineState
{
    volatile LONG sequence;
    ULONG magic;
    ULONG version;
    ULONG structSize;
    volatile LONG engineRunning;
    volatile LONG bridgeConnected;
    volatile LONG signalPresent;
    ULONG sampleRate;
    USHORT channels;
    USHORT containerBits;
    USHORT validBits;
    USHORT reserved;
    ULONG availableFrames;
    ULONGLONG totalFramesWritten;
    ULONGLONG totalFramesRead;
    ULONGLONG lostFrames;
    ULONGLONG fifoUnderruns;
    ULONGLONG rebufferCount;
    ULONG bufferedFrames;
    ULONG sourceSampleRate;
    USHORT sourceContainerBits;
    USHORT sourceValidBits;
    ULONGLONG engineFramesProcessed;
    ULONGLONG heartbeatMs;
    float peakL;
    float peakR;
    float rmsL;
    float rmsR;
    ULONG inputBusChannels;
    ULONG inputBusActiveMask;
    ULONGLONG inputBusFrames;
    float inputPeak[16];
    float inputRms[16];
    volatile LONG auxBridgeConnected;
    volatile LONG auxSignalPresent;
    ULONG auxSourceSampleRate;
    USHORT auxSourceContainerBits;
    USHORT auxSourceValidBits;
    ULONGLONG auxLostFrames;
    ULONGLONG auxTotalFramesWritten;
    ULONGLONG auxTotalFramesRead;
    float auxPeakL;
    float auxPeakR;
    float auxRmsL;
    float auxRmsR;
    float tidalPeakL;
    float tidalPeakR;
    float tidalRmsL;
    float tidalRmsR;
};

inline bool readEngineSnapshot(
    const EngineState* source,
    EngineState& destination)
{
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const LONG before = source->sequence;
        if ((before & 1) != 0)
        {
            Sleep(0);
            continue;
        }

        MemoryBarrier();
        std::memcpy(&destination, source, sizeof(destination));
        MemoryBarrier();

        const LONG after = source->sequence;
        if (before == after && (after & 1) == 0)
            return true;
    }

    return false;
}

inline constexpr wchar_t kAsioOutputMappingName[] =
    L"Local\\EntropyAudio.AsioOutput.v1";
inline constexpr std::uint32_t kAsioOutputMagic = 0x314F4145;
inline constexpr std::uint32_t kAsioOutputVersion = 0x00010000;
inline constexpr std::uint32_t kAsioOutputChannels = 16;
inline constexpr std::uint32_t kAsioOutputRingFrames = 32768;

struct alignas(64) AsioOutputRingHeader
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

static_assert(sizeof(AsioOutputRingHeader) == 64);

inline constexpr std::size_t kAsioOutputMappingBytes =
    sizeof(AsioOutputRingHeader) +
    static_cast<std::size_t>(kAsioOutputChannels) *
        static_cast<std::size_t>(kAsioOutputRingFrames) *
        sizeof(std::int32_t);
} // namespace entropy::telemetry
