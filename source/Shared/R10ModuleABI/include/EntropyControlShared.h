#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace entropy::control
{
inline constexpr std::uint32_t kMagic = 0x39434145; // "EAC9"
inline constexpr std::uint32_t kVersion = 0x00090200;
inline constexpr wchar_t kMappingName[] =
    L"Local\\EntropyAudio.Control.v092";

inline constexpr int kInputSourceCapacity = 128;
inline constexpr int kAsioInputChannels = 16;
inline constexpr int kInputSources = kInputSourceCapacity;
inline constexpr int kInputBuses = kAsioInputChannels;
inline constexpr int kOutputSources = 16;
inline constexpr int kOutputDestinations = 16;
inline constexpr int kDacSlots = 2;

// Display decoration must not change AUX capture or meter detection.
inline bool isAuxSourceName(const wchar_t* name) noexcept
{
    if (!name) return false;
    if (wcsncmp(name, L"[DEFAULT] ", 10) == 0) name += 10;
    return wcsncmp(name, L"AUX LEFT [WINDOWS / APPS]", 24) == 0 ||
           wcsncmp(name, L"AUX RIGHT [WINDOWS / APPS]", 25) == 0;
}

enum class InputSourceKind : LONG
{
    entropyTidal = 1,
    windowsCapture = 2,
    surroundDecoder = 3,
    entropyAux = 4,
    program = 5,
    reserved = 6
};

enum class SrcEngine : LONG
{
    r8brainReference = 0,
    soxrVhq = 1,
    entropySincLab = 2
};

inline SrcEngine sanitiseSrcEngine(LONG value) noexcept
{
    switch (static_cast<SrcEngine>(value))
    {
        case SrcEngine::r8brainReference:
        case SrcEngine::soxrVhq:
        case SrcEngine::entropySincLab:
            return static_cast<SrcEngine>(value);
        default:
            return SrcEngine::soxrVhq;
    }
}

struct InputSourceControl
{
    LONG kind;
    LONG endpointChannel;
    wchar_t stableKey[192];
    wchar_t endpointId[256];
    wchar_t displayName[128];
};

struct OutputDestinationControl
{
    LONG endpointChannel;
    LONG reserved;
    wchar_t stableKey[192];
    wchar_t endpointId[256];
    wchar_t displayName[128];
};

struct DacSinkControl
{
    LONG enabled;
    LONG exclusiveMode;
    LONG reserved0;
    LONG reserved1;
    LONG firstDestination;
    LONG channelCount;
    float targetFill;
    float maximumCorrectionPpm;
    wchar_t endpointName[128];
    wchar_t endpointId[256];
};

struct alignas(64) SharedControlState
{
    volatile LONG sequence;
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t structSize;
    volatile LONG generation;

    ULONG inputSourceCount;
    ULONG asioInputChannels;
    InputSourceControl inputSources[kInputSourceCapacity];
    float inputMatrix[kInputSources * kInputBuses];
    float outputMatrix[kOutputSources * kOutputDestinations];
    OutputDestinationControl outputDestinations[kOutputDestinations];
    LONG tidalSrcEngine;
    LONG auxSrcEngine;
    DacSinkControl dac[kDacSlots];
};

inline void publishBegin(SharedControlState* state)
{
    InterlockedIncrement(&state->sequence);
    MemoryBarrier();
}

inline void publishEnd(SharedControlState* state)
{
    MemoryBarrier();
    InterlockedIncrement(&state->sequence);
}

inline bool readSnapshot(
    const SharedControlState* source,
    SharedControlState& destination)
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
} // namespace entropy::control
