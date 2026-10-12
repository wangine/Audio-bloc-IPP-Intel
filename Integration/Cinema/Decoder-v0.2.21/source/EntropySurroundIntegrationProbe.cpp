#include "EntropySurroundIpc.h"
#include "C:/Projects/EntropyAudio/EntropyAudioEngine-v0.5/EntropyPhysicalIpc.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
template <typename Header, typename Sample>
struct Mapping
{
    HANDLE handle = nullptr;
    void* base = nullptr;
    const Header* header = nullptr;
    const Sample* samples = nullptr;
    ~Mapping()
    {
        if (base != nullptr) UnmapViewOfFile(base);
        if (handle != nullptr) CloseHandle(handle);
    }
};
}

int main()
{
    Mapping<entropy::surroundipc::RingHeader, float> decoder;
    decoder.handle = OpenFileMappingW(FILE_MAP_READ, FALSE,
        entropy::surroundipc::kMappingName);
    if (decoder.handle == nullptr)
    {
        std::cerr << "Decoder ring is not available.\n";
        return 2;
    }
    decoder.base = MapViewOfFile(decoder.handle, FILE_MAP_READ, 0, 0,
        entropy::surroundipc::kMappingBytes);
    decoder.header = static_cast<const entropy::surroundipc::RingHeader*>(decoder.base);
    decoder.samples = reinterpret_cast<const float*>(static_cast<const BYTE*>(decoder.base) +
        sizeof(entropy::surroundipc::RingHeader));

    Mapping<entropy::physical::InputRingHeader, std::int32_t> physical;
    physical.handle = OpenFileMappingW(FILE_MAP_READ, FALSE,
        entropy::physical::kInputMappingName);
    if (physical.handle == nullptr)
    {
        std::cerr << "Physical input ring is not available.\n";
        return 3;
    }
    physical.base = MapViewOfFile(physical.handle, FILE_MAP_READ, 0, 0,
        entropy::physical::kInputMappingBytes);
    physical.header = static_cast<const entropy::physical::InputRingHeader*>(physical.base);
    physical.samples = reinterpret_cast<const std::int32_t*>(
        static_cast<const BYTE*>(physical.base) + sizeof(entropy::physical::InputRingHeader));

    std::array<float, 8> decoderPeak{};
    const auto decoderWrite = static_cast<std::uint64_t>(decoder.header->writeFrame);
    const auto decoderTake = std::min<std::uint64_t>(decoderWrite, 48000);
    for (std::uint64_t absolute = decoderWrite - decoderTake; absolute < decoderWrite; ++absolute)
    {
        const auto* frame = decoder.samples +
            (absolute & (entropy::surroundipc::kRingFrames - 1)) * 8;
        for (int channel = 0; channel < 8; ++channel)
            decoderPeak[channel] = std::max(decoderPeak[channel], std::abs(frame[channel]));
    }

    std::array<float, 8> physicalPeak{};
    const auto physicalWrite = static_cast<std::uint64_t>(physical.header->writeFrame);
    const auto physicalTake = std::min<std::uint64_t>(physicalWrite,
        entropy::physical::kInputRingFrames);
    for (std::uint64_t absolute = physicalWrite - physicalTake; absolute < physicalWrite; ++absolute)
    {
        const auto* frame = physical.samples +
            (absolute & (entropy::physical::kInputRingFrames - 1)) *
                entropy::physical::kInputChannels;
        for (int channel = 0; channel < 8; ++channel)
            physicalPeak[channel] = std::max(physicalPeak[channel],
                std::abs(frame[channel + 2] / 2147483648.0f));
    }

    constexpr const char* names[] = {"FL", "FR", "C", "LFE", "SL", "SR", "BL", "BR"};
    std::cout << "Channel  Decoder48k  Physical192k\n";
    int active = 0;
    for (int channel = 0; channel < 8; ++channel)
    {
        std::cout << names[channel] << "\t " << decoderPeak[channel]
                  << "\t " << physicalPeak[channel] << "\n";
        if (decoderPeak[channel] > 1.0e-5f && physicalPeak[channel] > 1.0e-5f)
            ++active;
    }
    std::cout << "Matched active channels: " << active << "\n";
    return active >= 4 ? 0 : 4;
}
