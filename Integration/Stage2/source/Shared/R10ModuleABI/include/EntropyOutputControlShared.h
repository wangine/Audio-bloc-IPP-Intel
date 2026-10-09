#pragma once

#include "EntropyControlShared.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace entropy::outputcontrol
{
inline constexpr std::uint32_t kMagic = 0x344F4345; // "ECO4"
inline constexpr std::uint32_t kVersion = 0x00040000;
inline constexpr wchar_t kMappingName[] =
    L"Local\\EntropyMatrixCrossover02.OutputControl.v4";

inline constexpr int kGigSources = control::kOutputSources;
inline constexpr int kDirectSources = control::kAsioInputChannels;
inline constexpr int kDestinations = control::kOutputDestinations;
inline constexpr int kDacSlots = control::kDacSlots;

struct alignas(64) SharedOutputState
{
    volatile LONG sequence;
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t structSize;
    volatile LONG generation;

    float gigMatrix[kGigSources * kDestinations];
    float directMatrix[kDirectSources * kDestinations];
    control::OutputDestinationControl destinations[kDestinations];
    control::DacSinkControl dac[kDacSlots];
};

inline void publishBegin(SharedOutputState* state)
{
    InterlockedIncrement(&state->sequence);
    MemoryBarrier();
}

inline void publishEnd(SharedOutputState* state)
{
    MemoryBarrier();
    InterlockedIncrement(&state->sequence);
}

inline bool readSnapshot(
    const SharedOutputState* source,
    SharedOutputState& destination)
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
} // namespace entropy::outputcontrol
