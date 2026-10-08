#pragma once
#include <cstdint>

namespace entropy::engine {
struct GraphTelemetry {
    std::uint64_t completedBlocks = 0;
    std::uint32_t sampleRate = 192000;
    std::uint32_t firLatencySamples = 0;
    std::uint32_t destinationMask = 0;
    std::uint32_t headroomFaults = 0;
};
}
